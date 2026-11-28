#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "ea/sync/SyncProtocol.hpp"

namespace ea::gui {

class TeamSyncServer : public QObject {
    Q_OBJECT
public:
    explicit TeamSyncServer(QObject* parent = nullptr);
    ~TeamSyncServer() override;

    bool start(quint16 port, const QString& masterQeaPath, const QString& roomToken = QString());
    void stop();

    bool isRunning() const { return m_server && m_server->isListening(); }
    quint16 port() const { return m_server ? m_server->serverPort() : 0; }
    uint64_t revision() const { return m_revision; }
    QStringList connectedUsers() const;
    QString masterFilePath() const { return m_masterPath; }
    QString lastError() const { return m_lastError; }
    bool hasRoomToken() const { return !m_roomToken.isEmpty(); }

signals:
    void clientCountChanged(int count, const QStringList& users);
    void revisionUpdated(quint64 revision, const QString& author);
    void serverLog(const QString& message);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();

private:
    struct PeerState {
        QString username{"Anon"};
        bool authenticated{false};
        std::vector<uint8_t> rxBuffer;
    };

    void handleFrame(QTcpSocket* socket, const ea::sync::Frame& frame);
    void sendFrame(QTcpSocket* socket, const ea::sync::Frame& frame);
    void broadcastPeerList();
    bool loadMasterFromDisk();
    bool validateQeaSnapshot(const std::vector<uint8_t>& rawBytes, QString& errOut) const;
    bool saveMasterToDisk(const std::vector<uint8_t>& rawBytes);
    void updateCompressedCache();

    QTcpServer* m_server{nullptr};
    QString m_masterPath;
    QString m_roomToken;
    QString m_lastError;
    uint64_t m_revision{1};
    std::vector<uint8_t> m_masterBytes;
    std::vector<uint8_t> m_masterWireBytes; // OAZ1 + zlib-compressed snapshot for fast broadcast
    std::vector<ea::sync::Frame> m_chatHistory; // Last 50 live team chat messages
    std::unordered_map<QTcpSocket*, PeerState> m_peers;
};

} // namespace ea::gui
