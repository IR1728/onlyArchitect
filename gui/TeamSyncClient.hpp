#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTcpSocket>
#include <cstdint>
#include <vector>
#include "ea/sync/SyncProtocol.hpp"

namespace ea::gui {

class TeamSyncClient : public QObject {
    Q_OBJECT
public:
    explicit TeamSyncClient(QObject* parent = nullptr);
    ~TeamSyncClient() override;

    void connectToServer(const QString& host, quint16 port, const QString& username, const QString& roomToken = QString());
    void disconnectFromServer();
    bool pushLocalFile(const QString& localQeaPath);

    // Lightweight real-time presence, live-drag & team chat packets
    void sendCursorMove(int diagramId, float x, float y);
    void sendElementMove(int diagramId, int elementId, int left, int top, int right, int bottom);
    void sendChatMessage(const QString& text);

    bool isConnected() const { return m_socket && m_socket->state() == QAbstractSocket::ConnectedState; }
    uint64_t currentRevision() const { return m_revision; }
    QString username() const { return m_username; }
    QString localWorkspacePath() const { return m_localPath; }
    QStringList peers() const { return m_peers; }

signals:
    void connectedToTeam(quint64 revision, const QString& localQeaPath, const QStringList& peers);
    void remoteUpdateReceived(quint64 revision, const QString& author, const QString& localQeaPath);
    void pushAcknowledged(quint64 revision);
    void conflictDetected(quint64 serverRevision, const QString& message, const QString& localQeaPath);
    void peersUpdated(const QStringList& peers);
    void remoteCursorMoved(const QString& username, int diagramId, float x, float y);
    void remoteElementMoved(const QString& username, int diagramId, int elementId, int left, int top, int right, int bottom);
    void chatMessageReceived(const QString& author, const QString& text, qint64 timestampMs);
    void disconnectedFromTeam();
    void syncError(const QString& message);

private slots:
    void onSocketConnected();
    void onSocketReadyRead();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    void sendFrame(const ea::sync::Frame& frame);
    bool dispatchPushNow(const QString& localQeaPath);
    bool writeSnapshotToLocalCache(const std::vector<uint8_t>& wireBytes);

    QTcpSocket* m_socket{nullptr};
    QString m_username;
    QString m_roomToken;
    QString m_localPath;
    uint64_t m_revision{0};
    QStringList m_peers;
    std::vector<uint8_t> m_rxBuffer;

    // Coalesce rapid local edits so we never send two overlapping pushes with the same base revision
    bool m_pushInFlight{false};
    QString m_pendingPushPath;
};

} // namespace ea::gui
