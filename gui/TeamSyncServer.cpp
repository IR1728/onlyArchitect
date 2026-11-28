#include "TeamSyncServer.hpp"
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QSaveFile>
#include <sqlite3.h>
#include <algorithm>
#include <cstring>

namespace ea::gui {

namespace {

constexpr char kCompressedMagic[4] = {'O', 'A', 'Z', '1'};
constexpr char kSqliteMagic[16] = "SQLite format 3";
constexpr size_t kMaxSnapshotBytes = 128u * 1024u * 1024u;  // 128 MiB uncompressed cap (anti-zip-bomb)
constexpr size_t kMaxRxBufferBytes = 132u * 1024u * 1024u;  // Per-socket receive buffer cap
constexpr size_t kMaxPeers = 64;
constexpr size_t kMaxChatBytes = 4096;
constexpr int kMaxUsernameChars = 64;

bool constantTimeEquals(const QByteArray& a, const QByteArray& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (qsizetype i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return diff == 0;
}

QString sanitizeUsername(const QString& raw) {
    QString out;
    out.reserve(std::min<int>(raw.size(), kMaxUsernameChars));
    for (const QChar ch : raw.trimmed()) {
        if (ch.isPrint() && ch != ',' && ch != '\r' && ch != '\n') {
            out.append(ch);
            if (out.size() >= kMaxUsernameChars) break;
        }
    }
    return out.isEmpty() ? QStringLiteral("Usuario") : out;
}

std::vector<uint8_t> compressSnapshot(const std::vector<uint8_t>& raw) {
    if (raw.empty() || raw.size() > kMaxSnapshotBytes) return {};
    // Fast level-1 zlib compression shrinks sparse SQLite .qea pages by ~85-95% in <2ms
    const QByteArray z = qCompress(reinterpret_cast<const uchar*>(raw.data()), static_cast<qsizetype>(raw.size()), 1);
    std::vector<uint8_t> out;
    out.reserve(4 + static_cast<size_t>(z.size()));
    out.insert(out.end(), kCompressedMagic, kCompressedMagic + 4);
    out.insert(out.end(), reinterpret_cast<const uint8_t*>(z.constData()),
               reinterpret_cast<const uint8_t*>(z.constData()) + z.size());
    return out;
}

// Safely decompresses an OAZ1 payload after validating its 4-byte big-endian declared size
// to prevent zip-bomb / OOM attacks. Returns empty vector on corruption or oversize payload.
std::vector<uint8_t> decompressSnapshotSafe(const std::vector<uint8_t>& payload, QString& errOut) {
    if (payload.empty()) {
        errOut = QStringLiteral("Snapshot vacío.");
        return {};
    }
    if (payload.size() >= 4 && std::memcmp(payload.data(), kCompressedMagic, 4) == 0) {
        if (payload.size() <= 8) {
            errOut = QStringLiteral("Encabezado OAZ1 truncado.");
            return {};
        }
        const uint32_t declaredSize =
            (static_cast<uint32_t>(payload[4]) << 24) |
            (static_cast<uint32_t>(payload[5]) << 16) |
            (static_cast<uint32_t>(payload[6]) << 8) |
            static_cast<uint32_t>(payload[7]);
        if (declaredSize == 0 || declaredSize > kMaxSnapshotBytes) {
            errOut = QStringLiteral("Snapshot rechazado: tamaño descomprimido fuera de límite seguro (anti zip-bomb).");
            return {};
        }
        const QByteArray raw = qUncompress(reinterpret_cast<const uchar*>(payload.data() + 4),
                                           static_cast<qsizetype>(payload.size() - 4));
        if (raw.isEmpty() || static_cast<size_t>(raw.size()) > kMaxSnapshotBytes) {
            errOut = QStringLiteral("No se pudo descomprimir el snapshot OAZ1 (datos corruptos).");
            return {};
        }
        return std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(raw.constData()),
                                    reinterpret_cast<const uint8_t*>(raw.constData()) + raw.size());
    }
    if (payload.size() > kMaxSnapshotBytes) {
        errOut = QStringLiteral("Snapshot sin comprimir excede el límite de 128 MiB.");
        return {};
    }
    return payload;
}

} // namespace

TeamSyncServer::TeamSyncServer(QObject* parent) : QObject(parent) {
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &TeamSyncServer::onNewConnection);
}

TeamSyncServer::~TeamSyncServer() {
    stop();
}

void TeamSyncServer::updateCompressedCache() {
    m_masterWireBytes = compressSnapshot(m_masterBytes);
}

bool TeamSyncServer::validateQeaSnapshot(const std::vector<uint8_t>& rawBytes, QString& errOut) const {
    if (rawBytes.size() < 100 || std::memcmp(rawBytes.data(), kSqliteMagic, 16) != 0) {
        errOut = QStringLiteral("Snapshot rechazado: no tiene un encabezado válido de SQLite 3 (.qea).");
        return false;
    }
    const QString tmpPath = m_masterPath + QStringLiteral(".verify.tmp");
    {
        QFile tmp(tmpPath);
        if (!tmp.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            errOut = QStringLiteral("No se pudo crear archivo temporal de validación.");
            return false;
        }
        const qint64 written = tmp.write(reinterpret_cast<const char*>(rawBytes.data()),
                                         static_cast<qint64>(rawBytes.size()));
        tmp.close();
        if (written != static_cast<qint64>(rawBytes.size())) {
            QFile::remove(tmpPath);
            errOut = QStringLiteral("Escritura incompleta durante validación del snapshot.");
            return false;
        }
    }

    bool valid = false;
    sqlite3* db = nullptr;
    const QByteArray pathUtf8 = tmpPath.toUtf8();
    if (sqlite3_open_v2(pathUtf8.constData(), &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK && db) {
        sqlite3_stmt* stmt = nullptr;
        bool quickOk = false;
        if (sqlite3_prepare_v2(db, "PRAGMA quick_check(1);", -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const auto* txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                quickOk = (txt && std::strcmp(txt, "ok") == 0);
            }
            sqlite3_finalize(stmt);
        }
        bool hasSchema = false;
        if (quickOk && sqlite3_prepare_v2(
                           db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name='t_package' LIMIT 1;",
                           -1, &stmt, nullptr) == SQLITE_OK) {
            hasSchema = (sqlite3_step(stmt) == SQLITE_ROW);
            sqlite3_finalize(stmt);
        }
        valid = quickOk && hasSchema;
        if (!valid) {
            errOut = !quickOk ? QStringLiteral("Snapshot rechazado: falló PRAGMA quick_check (SQLite dañado).")
                              : QStringLiteral("Snapshot rechazado: el archivo SQLite no contiene esquema UML (t_package).");
        }
    } else {
        errOut = QStringLiteral("Snapshot rechazado: SQLite no pudo abrir la base de datos recibida.");
    }
    if (db) sqlite3_close(db);
    QFile::remove(tmpPath);
    return valid;
}

bool TeamSyncServer::loadMasterFromDisk() {
    QFile f(m_masterPath);
    if (!f.open(QIODevice::ReadOnly)) {
        m_lastError = QString("No se pudo leer el archivo maestro: %1").arg(m_masterPath);
        return false;
    }
    if (f.size() <= 0 || static_cast<size_t>(f.size()) > kMaxSnapshotBytes) {
        m_lastError = QString("El archivo maestro está vacío o excede 128 MiB: %1").arg(m_masterPath);
        return false;
    }
    const QByteArray raw = f.readAll();
    m_masterBytes.assign(reinterpret_cast<const uint8_t*>(raw.constData()),
                         reinterpret_cast<const uint8_t*>(raw.constData()) + raw.size());
    if (m_masterBytes.empty()) return false;
    updateCompressedCache();
    return true;
}

bool TeamSyncServer::saveMasterToDisk(const std::vector<uint8_t>& rawBytes) {
    // Rolling backup of previous good master so accidental destructive edits can always be recovered
    if (!m_masterBytes.empty()) {
        QSaveFile bak(m_masterPath + QStringLiteral(".bak"));
        if (bak.open(QIODevice::WriteOnly)) {
            bak.write(reinterpret_cast<const char*>(m_masterBytes.data()),
                      static_cast<qint64>(m_masterBytes.size()));
            bak.commit();
        }
    }

    QSaveFile f(m_masterPath);
    if (!f.open(QIODevice::WriteOnly)) {
        m_lastError = QString("No se pudo escribir el archivo maestro: %1").arg(m_masterPath);
        return false;
    }
    f.write(reinterpret_cast<const char*>(rawBytes.data()), static_cast<qint64>(rawBytes.size()));
    if (!f.commit()) {
        m_lastError = QString("Error al confirmar guardado atómico en: %1").arg(m_masterPath);
        return false;
    }
    m_masterBytes = rawBytes;
    updateCompressedCache();
    return true;
}

bool TeamSyncServer::start(quint16 port, const QString& masterQeaPath, const QString& roomToken) {
    stop();
    m_masterPath = masterQeaPath;
    m_roomToken = roomToken.trimmed();
    m_revision = 1;
    m_chatHistory.clear();
    if (!loadMasterFromDisk()) return false;

    if (!m_server->listen(QHostAddress::AnyIPv4, port)) {
        m_lastError = m_server->errorString();
        return false;
    }

    emit serverLog(QString("Servidor LAN/VPN escuchando en puerto %1 (%2, rev #%3, %4 KB comprimidos%5)")
                       .arg(m_server->serverPort())
                       .arg(QFileInfo(m_masterPath).fileName())
                       .arg(m_revision)
                       .arg(std::max<size_t>(1, m_masterWireBytes.size() / 1024))
                       .arg(m_roomToken.isEmpty() ? QString() : QStringLiteral(", sala protegida con PIN")));
    return true;
}

void TeamSyncServer::stop() {
    for (auto& [sock, _] : m_peers) {
        if (sock) {
            sock->disconnect(this);
            sock->disconnectFromHost();
            sock->deleteLater();
        }
    }
    m_peers.clear();
    if (m_server && m_server->isListening()) {
        m_server->close();
    }
    emit clientCountChanged(0, {});
}

QStringList TeamSyncServer::connectedUsers() const {
    QStringList users;
    users.reserve(static_cast<int>(m_peers.size()));
    for (const auto& [_, peer] : m_peers) {
        if (peer.authenticated) {
            users.append(peer.username);
        }
    }
    users.sort(Qt::CaseInsensitive);
    return users;
}

void TeamSyncServer::onNewConnection() {
    while (QTcpSocket* sock = m_server->nextPendingConnection()) {
        if (m_peers.size() >= kMaxPeers) {
            ea::sync::Frame err;
            err.type = ea::sync::MessageType::Error;
            err.meta = "Servidor lleno: límite máximo de conexiones alcanzado.";
            sendFrame(sock, err);
            sock->disconnectFromHost();
            sock->deleteLater();
            continue;
        }
        // Disable Nagle's algorithm so live cursor/drag packets and snapshots arrive immediately
        sock->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        sock->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
        m_peers[sock] = PeerState{};
        connect(sock, &QTcpSocket::readyRead, this, &TeamSyncServer::onClientReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &TeamSyncServer::onClientDisconnected);
        if (sock->bytesAvailable() > 0) {
            const QByteArray chunk = sock->readAll();
            auto& buf = m_peers[sock].rxBuffer;
            if (buf.size() + static_cast<size_t>(chunk.size()) > kMaxRxBufferBytes) {
                sock->disconnectFromHost();
                continue;
            }
            buf.insert(buf.end(),
                       reinterpret_cast<const uint8_t*>(chunk.constData()),
                       reinterpret_cast<const uint8_t*>(chunk.constData()) + chunk.size());
            ea::sync::Frame frame;
            while (m_peers.count(sock) && ea::sync::tryDecodeFrame(m_peers[sock].rxBuffer, frame)) {
                handleFrame(sock, frame);
            }
        }
    }
}

void TeamSyncServer::onClientReadyRead() {
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock || !m_peers.count(sock)) return;

    const QByteArray chunk = sock->readAll();
    auto& buf = m_peers[sock].rxBuffer;
    if (buf.size() + static_cast<size_t>(chunk.size()) > kMaxRxBufferBytes) {
        emit serverLog(QString("Cliente desconectado por exceder límite de buffer RX (%1)").arg(m_peers[sock].username));
        sock->disconnectFromHost();
        return;
    }
    buf.insert(buf.end(),
               reinterpret_cast<const uint8_t*>(chunk.constData()),
               reinterpret_cast<const uint8_t*>(chunk.constData()) + chunk.size());

    ea::sync::Frame frame;
    while (m_peers.count(sock) && ea::sync::tryDecodeFrame(m_peers[sock].rxBuffer, frame)) {
        handleFrame(sock, frame);
    }
}

void TeamSyncServer::onClientDisconnected() {
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;

    auto it = m_peers.find(sock);
    QString name = "Anon";
    bool wasAuthed = false;
    if (it != m_peers.end()) {
        name = it->second.username;
        wasAuthed = it->second.authenticated;
        m_peers.erase(it);
    }
    sock->deleteLater();
    if (wasAuthed) {
        emit serverLog(QString("Desconectado: %1").arg(name));
        broadcastPeerList();
    }
}

void TeamSyncServer::sendFrame(QTcpSocket* socket, const ea::sync::Frame& frame) {
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) return;
    const auto encoded = ea::sync::encodeFrame(frame);
    socket->write(reinterpret_cast<const char*>(encoded.data()), static_cast<qint64>(encoded.size()));
    socket->flush();
}

void TeamSyncServer::broadcastPeerList() {
    const QStringList users = connectedUsers();
    ea::sync::Frame frame;
    frame.type = ea::sync::MessageType::PeerList;
    frame.revision = m_revision;
    frame.meta = users.join(",").toStdString();
    for (const auto& [sock, peer] : m_peers) {
        if (peer.authenticated) sendFrame(sock, frame);
    }
    emit clientCountChanged(static_cast<int>(users.size()), users);
}

void TeamSyncServer::handleFrame(QTcpSocket* socket, const ea::sync::Frame& frame) {
    auto it = m_peers.find(socket);
    if (it == m_peers.end()) return;

    if (frame.type == ea::sync::MessageType::Hello) {
        if (!m_roomToken.isEmpty()) {
            const QByteArray provided(reinterpret_cast<const char*>(frame.payload.data()),
                                      static_cast<qsizetype>(frame.payload.size()));
            if (!constantTimeEquals(provided.trimmed(), m_roomToken.toUtf8())) {
                ea::sync::Frame err;
                err.type = ea::sync::MessageType::Error;
                err.meta = "Acceso denegado: PIN/clave de sala incorrecto.";
                sendFrame(socket, err);
                emit serverLog(QString("Conexión rechazada (PIN incorrecto) desde %1").arg(socket->peerAddress().toString()));
                socket->disconnectFromHost();
                return;
            }
        }

        it->second.username = sanitizeUsername(QString::fromStdString(frame.meta));
        it->second.authenticated = true;

        ea::sync::Frame welcome;
        welcome.type = ea::sync::MessageType::Welcome;
        welcome.revision = m_revision;
        welcome.meta = connectedUsers().join(",").toStdString();
        welcome.payload = m_masterWireBytes;
        sendFrame(socket, welcome);

        // Replay recent chat messages so newcomers immediately see team context
        for (const auto& msg : m_chatHistory) {
            sendFrame(socket, msg);
        }

        emit serverLog(QString("Conectado: %1 (enviada rev #%2)").arg(it->second.username).arg(m_revision));
        broadcastPeerList();
        return;
    }

    // Refuse all state-changing or relay frames before completing authenticated Hello handshake
    if (!it->second.authenticated) {
        ea::sync::Frame err;
        err.type = ea::sync::MessageType::Error;
        err.meta = "Autenticación requerida (envía Hello primero).";
        sendFrame(socket, err);
        socket->disconnectFromHost();
        return;
    }

    // Fast ephemeral real-time relay (0 disk I/O, <1ms latency)
    if (frame.type == ea::sync::MessageType::CursorMove || frame.type == ea::sync::MessageType::ElementMove) {
        if ((frame.type == ea::sync::MessageType::CursorMove && frame.payload.size() != 12) ||
            (frame.type == ea::sync::MessageType::ElementMove && frame.payload.size() != 24)) {
            return;
        }
        ea::sync::Frame relay = frame;
        relay.meta = it->second.username.toStdString();
        const auto encoded = ea::sync::encodeFrame(relay);
        for (const auto& [peerSock, peer] : m_peers) {
            if (peer.authenticated && peerSock != socket && peerSock &&
                peerSock->state() == QAbstractSocket::ConnectedState) {
                peerSock->write(reinterpret_cast<const char*>(encoded.data()), static_cast<qint64>(encoded.size()));
            }
        }
        return;
    }

    // Live Team Chat relay + short in-memory history (50 msgs) for newly joined peers
    if (frame.type == ea::sync::MessageType::ChatMessage) {
        if (frame.payload.empty() || frame.payload.size() > kMaxChatBytes) return;
        ea::sync::Frame chat = frame;
        chat.meta = it->second.username.toStdString();
        m_chatHistory.push_back(chat);
        if (m_chatHistory.size() > 50) {
            m_chatHistory.erase(m_chatHistory.begin());
        }
        const auto encoded = ea::sync::encodeFrame(chat);
        for (const auto& [peerSock, peer] : m_peers) {
            if (peer.authenticated && peerSock && peerSock->state() == QAbstractSocket::ConnectedState) {
                peerSock->write(reinterpret_cast<const char*>(encoded.data()), static_cast<qint64>(encoded.size()));
                peerSock->flush();
            }
        }
        return;
    }

    if (frame.type == ea::sync::MessageType::PushUpdate) {
        if (frame.payload.empty()) return;

        // Optimistic concurrency control: reject stale writes and send latest snapshot
        if (frame.revision < m_revision) {
            ea::sync::Frame conflict;
            conflict.type = ea::sync::MessageType::Conflict;
            conflict.revision = m_revision;
            conflict.meta = "Conflicto de versión: otro compañero guardó cambios más recientes.";
            conflict.payload = m_masterWireBytes;
            sendFrame(socket, conflict);
            emit serverLog(QString("Conflicto rechazado de %1 (base rev #%2 < actual #%3)")
                               .arg(it->second.username)
                               .arg(frame.revision)
                               .arg(m_revision));
            return;
        }

        QString validationErr;
        const auto rawBytes = decompressSnapshotSafe(frame.payload, validationErr);
        if (rawBytes.empty() || !validateQeaSnapshot(rawBytes, validationErr)) {
            ea::sync::Frame err;
            err.type = ea::sync::MessageType::Error;
            err.revision = m_revision;
            err.meta = validationErr.toStdString();
            sendFrame(socket, err);
            emit serverLog(QString("Push rechazado de %1: %2").arg(it->second.username, validationErr));
            return;
        }

        if (!saveMasterToDisk(rawBytes)) {
            ea::sync::Frame err;
            err.type = ea::sync::MessageType::Error;
            err.revision = m_revision;
            err.meta = m_lastError.toStdString();
            sendFrame(socket, err);
            return;
        }

        ++m_revision;
        const QString author = it->second.username;

        ea::sync::Frame ack;
        ack.type = ea::sync::MessageType::PushAck;
        ack.revision = m_revision;
        ack.meta = author.toStdString();
        sendFrame(socket, ack);

        ea::sync::Frame broadcast;
        broadcast.type = ea::sync::MessageType::BroadcastUpdate;
        broadcast.revision = m_revision;
        broadcast.meta = author.toStdString();
        broadcast.payload = m_masterWireBytes;
        const auto encodedBroadcast = ea::sync::encodeFrame(broadcast);
        for (const auto& [peerSock, peer] : m_peers) {
            if (peer.authenticated && peerSock != socket && peerSock &&
                peerSock->state() == QAbstractSocket::ConnectedState) {
                peerSock->write(reinterpret_cast<const char*>(encodedBroadcast.data()),
                                static_cast<qint64>(encodedBroadcast.size()));
                peerSock->flush();
            }
        }

        emit revisionUpdated(m_revision, author);
        emit serverLog(QString("Rev #%1 publicada por %2 (%3 KB -> %4 KB, respaldo .bak actualizado)")
                           .arg(m_revision)
                           .arg(author)
                           .arg(std::max<size_t>(1, m_masterBytes.size() / 1024))
                           .arg(std::max<size_t>(1, m_masterWireBytes.size() / 1024)));
    }
}

} // namespace ea::gui
