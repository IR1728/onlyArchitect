#include "TeamSyncClient.hpp"
#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QUuid>
#include <cstring>

namespace ea::gui {

namespace {

constexpr char kCompressedMagic[4] = {'O', 'A', 'Z', '1'};
constexpr char kSqliteMagic[16] = "SQLite format 3";
constexpr size_t kMaxSnapshotBytes = 128u * 1024u * 1024u;
constexpr size_t kMaxRxBufferBytes = 132u * 1024u * 1024u;

std::vector<uint8_t> compressSnapshot(const QByteArray& raw) {
    if (raw.isEmpty() || static_cast<size_t>(raw.size()) > kMaxSnapshotBytes) return {};
    const QByteArray z = qCompress(raw, 1);
    std::vector<uint8_t> out;
    out.reserve(4 + static_cast<size_t>(z.size()));
    out.insert(out.end(), kCompressedMagic, kCompressedMagic + 4);
    out.insert(out.end(), reinterpret_cast<const uint8_t*>(z.constData()),
               reinterpret_cast<const uint8_t*>(z.constData()) + z.size());
    return out;
}

QByteArray decompressSnapshotIfNeeded(const std::vector<uint8_t>& wireBytes) {
    if (wireBytes.empty()) return {};
    if (wireBytes.size() >= 4 && std::memcmp(wireBytes.data(), kCompressedMagic, 4) == 0) {
        if (wireBytes.size() <= 8) return {};
        const uint32_t declaredSize =
            (static_cast<uint32_t>(wireBytes[4]) << 24) |
            (static_cast<uint32_t>(wireBytes[5]) << 16) |
            (static_cast<uint32_t>(wireBytes[6]) << 8) |
            static_cast<uint32_t>(wireBytes[7]);
        if (declaredSize == 0 || declaredSize > kMaxSnapshotBytes) return {};
        const QByteArray raw = qUncompress(reinterpret_cast<const uchar*>(wireBytes.data() + 4),
                                           static_cast<qsizetype>(wireBytes.size() - 4));
        if (raw.isEmpty() || static_cast<size_t>(raw.size()) > kMaxSnapshotBytes) return {};
        return raw;
    }
    if (wireBytes.size() > kMaxSnapshotBytes) return {};
    return QByteArray(reinterpret_cast<const char*>(wireBytes.data()), static_cast<qsizetype>(wireBytes.size()));
}

} // namespace

TeamSyncClient::TeamSyncClient(QObject* parent) : QObject(parent) {
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &TeamSyncClient::onSocketConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &TeamSyncClient::onSocketReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &TeamSyncClient::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &TeamSyncClient::onSocketError);
}

TeamSyncClient::~TeamSyncClient() {
    disconnectFromServer();
}

void TeamSyncClient::connectToServer(const QString& host, quint16 port, const QString& username, const QString& roomToken) {
    disconnectFromServer();
    m_username = username.trimmed().isEmpty() ? "Usuario" : username.trimmed();
    m_roomToken = roomToken.trimmed();
    m_revision = 0;
    m_peers.clear();
    m_rxBuffer.clear();
    m_pushInFlight = false;
    m_pendingPushPath.clear();

    const QString cacheDir = QDir::tempPath() + "/onlyarchitect_team_sync";
    QDir().mkpath(cacheDir);
    const QString token = QUuid::createUuid().toString(QUuid::Id128).left(12);
    m_localPath = cacheDir + QString("/team_workspace_%1.qea").arg(token);

    m_socket->connectToHost(host.trimmed(), port);
}

void TeamSyncClient::disconnectFromServer() {
    m_pushInFlight = false;
    m_pendingPushPath.clear();
    if (m_socket && m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->abort();
        }
    }
    m_rxBuffer.clear();
}

bool TeamSyncClient::pushLocalFile(const QString& localQeaPath) {
    if (!isConnected()) return false;
    if (m_pushInFlight) {
        // Queue the latest state and send immediately once the in-flight PushAck updates m_revision
        m_pendingPushPath = localQeaPath;
        return true;
    }
    return dispatchPushNow(localQeaPath);
}

bool TeamSyncClient::dispatchPushNow(const QString& localQeaPath) {
    if (!isConnected()) return false;

    QFile f(localQeaPath);
    if (!f.open(QIODevice::ReadOnly)) {
        emit syncError(QString("No se pudo leer el archivo local para sincronizar: %1").arg(localQeaPath));
        return false;
    }
    const QByteArray raw = f.readAll();
    if (raw.isEmpty()) return false;

    ea::sync::Frame frame;
    frame.type = ea::sync::MessageType::PushUpdate;
    frame.revision = m_revision;
    frame.meta = m_username.toStdString();
    frame.payload = compressSnapshot(raw);
    m_pushInFlight = true;
    sendFrame(frame);
    return true;
}

void TeamSyncClient::sendCursorMove(int diagramId, float x, float y) {
    if (!isConnected() || diagramId <= 0) return;
    ea::sync::Frame frame;
    frame.type = ea::sync::MessageType::CursorMove;
    frame.revision = m_revision;
    frame.meta = m_username.toStdString();
    frame.payload = ea::sync::encodeCursorPayload({diagramId, x, y});
    sendFrame(frame);
}

void TeamSyncClient::sendElementMove(int diagramId, int elementId, int left, int top, int right, int bottom) {
    if (!isConnected() || diagramId <= 0 || elementId <= 0) return;
    ea::sync::Frame frame;
    frame.type = ea::sync::MessageType::ElementMove;
    frame.revision = m_revision;
    frame.meta = m_username.toStdString();
    frame.payload = ea::sync::encodeElementMovePayload({diagramId, elementId, left, top, right, bottom});
    sendFrame(frame);
}

void TeamSyncClient::sendChatMessage(const QString& text) {
    const QString clean = text.trimmed();
    if (!isConnected() || clean.isEmpty()) return;
    const QByteArray utf8 = clean.toUtf8();
    ea::sync::Frame frame;
    frame.type = ea::sync::MessageType::ChatMessage;
    frame.revision = static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch());
    frame.meta = m_username.toStdString();
    frame.payload.assign(reinterpret_cast<const uint8_t*>(utf8.constData()),
                         reinterpret_cast<const uint8_t*>(utf8.constData()) + utf8.size());
    sendFrame(frame);
}

void TeamSyncClient::sendFrame(const ea::sync::Frame& frame) {
    if (!isConnected()) return;
    const auto encoded = ea::sync::encodeFrame(frame);
    m_socket->write(reinterpret_cast<const char*>(encoded.data()), static_cast<qint64>(encoded.size()));
    m_socket->flush();
}

bool TeamSyncClient::writeSnapshotToLocalCache(const std::vector<uint8_t>& wireBytes) {
    if (wireBytes.empty()) return false;
    const QByteArray raw = decompressSnapshotIfNeeded(wireBytes);
    if (raw.size() < 100 || std::memcmp(raw.constData(), kSqliteMagic, 16) != 0) {
        emit syncError(QStringLiteral("Snapshot remoto rechazado: datos corruptos o SQLite inválido."));
        return false;
    }

    QSaveFile f(m_localPath);
    if (!f.open(QIODevice::WriteOnly)) {
        emit syncError(QString("No se pudo escribir el snapshot local en: %1").arg(m_localPath));
        return false;
    }
    f.write(raw);
    return f.commit();
}

void TeamSyncClient::onSocketConnected() {
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);

    ea::sync::Frame hello;
    hello.type = ea::sync::MessageType::Hello;
    hello.revision = 0;
    hello.meta = m_username.toStdString();
    if (!m_roomToken.isEmpty()) {
        const QByteArray tok = m_roomToken.toUtf8();
        hello.payload.assign(reinterpret_cast<const uint8_t*>(tok.constData()),
                             reinterpret_cast<const uint8_t*>(tok.constData()) + tok.size());
    }
    sendFrame(hello);
}

void TeamSyncClient::onSocketReadyRead() {
    const QByteArray chunk = m_socket->readAll();
    if (m_rxBuffer.size() + static_cast<size_t>(chunk.size()) > kMaxRxBufferBytes) {
        emit syncError(QStringLiteral("Conexión cerrada: el servidor envió demasiados datos sin fin de trama."));
        disconnectFromServer();
        return;
    }
    m_rxBuffer.insert(m_rxBuffer.end(),
                      reinterpret_cast<const uint8_t*>(chunk.constData()),
                      reinterpret_cast<const uint8_t*>(chunk.constData()) + chunk.size());

    ea::sync::Frame frame;
    while (ea::sync::tryDecodeFrame(m_rxBuffer, frame)) {
        switch (frame.type) {
            case ea::sync::MessageType::Welcome: {
                m_revision = frame.revision;
                m_pushInFlight = false;
                m_pendingPushPath.clear();
                m_peers = QString::fromStdString(frame.meta).split(',', Qt::SkipEmptyParts);
                if (writeSnapshotToLocalCache(frame.payload)) {
                    emit connectedToTeam(m_revision, m_localPath, m_peers);
                }
                break;
            }
            case ea::sync::MessageType::BroadcastUpdate: {
                m_revision = frame.revision;
                const QString author = QString::fromStdString(frame.meta);
                if (writeSnapshotToLocalCache(frame.payload)) {
                    emit remoteUpdateReceived(m_revision, author, m_localPath);
                }
                break;
            }
            case ea::sync::MessageType::PushAck: {
                m_revision = frame.revision;
                m_pushInFlight = false;
                emit pushAcknowledged(m_revision);
                if (!m_pendingPushPath.isEmpty()) {
                    const QString nextPath = m_pendingPushPath;
                    m_pendingPushPath.clear();
                    dispatchPushNow(nextPath);
                }
                break;
            }
            case ea::sync::MessageType::Conflict: {
                m_revision = frame.revision;
                m_pushInFlight = false;
                m_pendingPushPath.clear();
                const QString msg = QString::fromStdString(frame.meta);
                if (writeSnapshotToLocalCache(frame.payload)) {
                    emit conflictDetected(m_revision, msg, m_localPath);
                }
                break;
            }
            case ea::sync::MessageType::PeerList: {
                m_peers = QString::fromStdString(frame.meta).split(',', Qt::SkipEmptyParts);
                emit peersUpdated(m_peers);
                break;
            }
            case ea::sync::MessageType::CursorMove: {
                if (auto c = ea::sync::decodeCursorPayload(frame.payload)) {
                    emit remoteCursorMoved(QString::fromStdString(frame.meta), c->diagramId, c->x, c->y);
                }
                break;
            }
            case ea::sync::MessageType::ElementMove: {
                if (auto m = ea::sync::decodeElementMovePayload(frame.payload)) {
                    emit remoteElementMoved(QString::fromStdString(frame.meta), m->diagramId, m->elementId,
                                            m->left, m->top, m->right, m->bottom);
                }
                break;
            }
            case ea::sync::MessageType::ChatMessage: {
                const QString author = QString::fromStdString(frame.meta);
                const QString text = QString::fromUtf8(reinterpret_cast<const char*>(frame.payload.data()),
                                                       static_cast<qsizetype>(frame.payload.size()));
                const qint64 ts = frame.revision > 0 ? static_cast<qint64>(frame.revision)
                                                     : QDateTime::currentMSecsSinceEpoch();
                emit chatMessageReceived(author, text, ts);
                break;
            }
            case ea::sync::MessageType::Error: {
                m_pushInFlight = false;
                emit syncError(QString::fromStdString(frame.meta));
                break;
            }
            default:
                break;
        }
    }
}

void TeamSyncClient::onSocketDisconnected() {
    m_pushInFlight = false;
    m_pendingPushPath.clear();
    m_peers.clear();
    emit disconnectedFromTeam();
}

void TeamSyncClient::onSocketError(QAbstractSocket::SocketError /*error*/) {
    m_pushInFlight = false;
    if (m_socket) {
        emit syncError(m_socket->errorString());
    }
}

} // namespace ea::gui
