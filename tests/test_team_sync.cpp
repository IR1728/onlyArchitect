#ifdef NDEBUG
#undef NDEBUG
#endif
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <cassert>
#include <filesystem>
#include <iostream>
#include "ea/repository/Repository.hpp"
#include "ea/sync/SyncProtocol.hpp"
#include "gui/TeamSyncClient.hpp"
#include "gui/TeamSyncServer.hpp"

namespace fs = std::filesystem;

static bool waitUntil(const std::function<bool()>& cond, int timeoutMs = 4000) {
    QElapsedTimer timer;
    timer.start();
    while (!cond() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return cond();
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "=======================================================\n";
    std::cout << " Running LAN/VPN Team Sync Tests (Protocol + Server)   \n";
    std::cout << "=======================================================\n";

    // 1. Unit test SyncProtocol framing (fragmented TCP stream + multiple frames)
    {
        ea::sync::Frame f1{ea::sync::MessageType::PushUpdate, 42, "Lenin", {1, 2, 3, 4, 5}};
        ea::sync::Frame f2{ea::sync::MessageType::PeerList, 42, "Lenin,Iran", {}};
        auto b1 = ea::sync::encodeFrame(f1);
        auto b2 = ea::sync::encodeFrame(f2);

        std::vector<uint8_t> stream;
        ea::sync::Frame decoded;

        // Feed partial bytes first
        stream.insert(stream.end(), b1.begin(), b1.begin() + 5);
        assert(!ea::sync::tryDecodeFrame(stream, decoded));

        // Feed rest of f1 + all of f2
        stream.insert(stream.end(), b1.begin() + 5, b1.end());
        stream.insert(stream.end(), b2.begin(), b2.end());

        assert(ea::sync::tryDecodeFrame(stream, decoded));
        assert(decoded.type == ea::sync::MessageType::PushUpdate);
        assert(decoded.revision == 42 && decoded.meta == "Lenin" && decoded.payload.size() == 5);

        assert(ea::sync::tryDecodeFrame(stream, decoded));
        assert(decoded.type == ea::sync::MessageType::PeerList);
        assert(decoded.meta == "Lenin,Iran" && decoded.payload.empty());
        assert(stream.empty());

        const auto cursorBytes = ea::sync::encodeCursorPayload({7, 123.5f, -45.25f});
        const auto decodedCursor = ea::sync::decodeCursorPayload(cursorBytes);
        assert(decodedCursor && decodedCursor->diagramId == 7 && decodedCursor->x == 123.5f && decodedCursor->y == -45.25f);

        const auto moveBytes = ea::sync::encodeElementMovePayload({7, 99, 10, -20, 190, -140});
        const auto decodedMove = ea::sync::decodeElementMovePayload(moveBytes);
        assert(decodedMove && decodedMove->diagramId == 7 && decodedMove->elementId == 99 &&
               decodedMove->left == 10 && decodedMove->top == -20 && decodedMove->right == 190 && decodedMove->bottom == -140);
        std::cout << "  ✓ SyncProtocol binary framing + CursorMove/ElementMove payloads\n";
    }

    // 2. End-to-end 2-client multiplayer sync over localhost TCP
    const fs::path masterPath = fs::temp_directory_path() / "onlyarchitect_team_master.qea";
    fs::remove(masterPath);
    {
        ea::Repository seed;
        assert(seed.CreateRepository(masterPath.string()));
        seed.CloseFile();
    }

    ea::gui::TeamSyncServer server;
    assert(server.start(0, QString::fromStdString(masterPath.string())));
    const quint16 port = server.port();
    assert(port > 0);

    ea::gui::TeamSyncClient clientLenin;
    ea::gui::TeamSyncClient clientIran;

    bool leninConnected = false;
    bool iranConnected = false;
    bool leninAcked = false;
    bool iranReceivedUpdate = false;
    bool conflictTriggered = false;
    QString leninLocalPath;
    QString iranLocalPath;
    QString lastUpdateAuthor;

    QObject::connect(&clientLenin, &ea::gui::TeamSyncClient::connectedToTeam,
                     [&](quint64 rev, const QString& path, const QStringList&) {
                         assert(rev == 1);
                         leninLocalPath = path;
                         leninConnected = true;
                     });
    QObject::connect(&clientIran, &ea::gui::TeamSyncClient::connectedToTeam,
                     [&](quint64 rev, const QString& path, const QStringList&) {
                         assert(rev == 1);
                         iranLocalPath = path;
                         iranConnected = true;
                     });
    QObject::connect(&clientLenin, &ea::gui::TeamSyncClient::pushAcknowledged,
                     [&](quint64 rev) {
                         assert(rev >= 2);
                         leninAcked = true;
                     });
    QObject::connect(&clientIran, &ea::gui::TeamSyncClient::remoteUpdateReceived,
                     [&](quint64 rev, const QString& author, const QString& path) {
                         assert(rev >= 2);
                         lastUpdateAuthor = author;
                         iranLocalPath = path;
                         iranReceivedUpdate = true;
                     });
    QObject::connect(&clientIran, &ea::gui::TeamSyncClient::conflictDetected,
                     [&](quint64 serverRev, const QString&, const QString&) {
                         assert(serverRev == 2);
                         conflictTriggered = true;
                     });

    clientLenin.connectToServer("127.0.0.1", port, "Lenin");
    clientIran.connectToServer("127.0.0.1", port, "Iran");
    assert(waitUntil([&]() { return leninConnected && iranConnected; }));
    assert(waitUntil([&]() { return server.connectedUsers().size() == 2; }));
    std::cout << "  ✓ Two clients (Lenin, Iran) connected and received initial rev #1 snapshot\n";

    // Lenin edits his local workspace (adds UML Class "MultiplayerSync") and pushes to server
    {
        ea::Repository repo;
        assert(repo.OpenFile(leninLocalPath.toStdString()));
        auto root = repo.GetModels().GetAt(0);
        assert(root);
        ea::Element cls;
        cls.SetName("MultiplayerSync");
        cls.SetType(ea::ObjectType::Class);
        cls.SetPackageID(root->GetPackageID());
        assert(repo.AddElement(cls) != nullptr);
        repo.CloseFile();
    }
    assert(clientLenin.pushLocalFile(leninLocalPath));
    assert(waitUntil([&]() { return leninAcked && iranReceivedUpdate; }));
    assert(lastUpdateAuthor == "Lenin");
    assert(server.revision() == 2);

    // Verify Iran's local synced .qea contains Lenin's new class!
    {
        ea::Repository iranRepo;
        assert(iranRepo.OpenFile(iranLocalPath.toStdString()));
        bool found = false;
        auto root = iranRepo.GetModels().GetAt(0);
        assert(root);
        for (size_t i = 0; i < root->GetElements().size(); ++i) {
            if (auto e = root->GetElements().GetAt(static_cast<short>(i)); e && e->GetName() == "MultiplayerSync") {
                found = true;
            }
        }
        assert(found && "Iran's synced repository must contain Lenin's MultiplayerSync class");
        iranRepo.CloseFile();
    }
    std::cout << "  ✓ Real-time push & broadcast verified (Iran received Lenin's MultiplayerSync class)\n";

    // 3. Live real-time cursor & element drag relay (<1ms ephemeral frames, no revision bump)
    {
        bool gotCursor = false;
        bool gotMove = false;
        QObject::connect(&clientIran, &ea::gui::TeamSyncClient::remoteCursorMoved,
                         [&](const QString& user, int diagId, float x, float y) {
                             if (user == "Lenin" && diagId == 3 && x == 250.0f && y == 180.0f) gotCursor = true;
                         });
        QObject::connect(&clientIran, &ea::gui::TeamSyncClient::remoteElementMoved,
                         [&](const QString& user, int diagId, int elemId, int l, int t, int r, int b) {
                             if (user == "Lenin" && diagId == 3 && elemId == 55 && l == 100 && t == -50 && r == 280 && b == -160) {
                                 gotMove = true;
                             }
                         });
        clientLenin.sendCursorMove(3, 250.0f, 180.0f);
        clientLenin.sendElementMove(3, 55, 100, -50, 280, -160);
        assert(waitUntil([&]() { return gotCursor && gotMove; }));
        assert(server.revision() == 2 && "Ephemeral cursor/drag packets must not bump repository revision");
        std::cout << "  ✓ Real-time CursorMove & ElementMove live relay verified\n";
    }

    // 3b. Live Team Chat broadcast + history replay on reconnect
    {
        bool iranGotChat = false;
        QObject::connect(&clientIran, &ea::gui::TeamSyncClient::chatMessageReceived,
                         [&](const QString& author, const QString& text, qint64) {
                             if (author == "Lenin" && text == "Hola equipo desde LAN!") iranGotChat = true;
                         });
        clientLenin.sendChatMessage("Hola equipo desde LAN!");
        assert(waitUntil([&]() { return iranGotChat; }));

        ea::gui::TeamSyncClient lateJoiner;
        bool lateGotHistory = false;
        QObject::connect(&lateJoiner, &ea::gui::TeamSyncClient::chatMessageReceived,
                         [&](const QString& author, const QString& text, qint64) {
                             if (author == "Lenin" && text == "Hola equipo desde LAN!") lateGotHistory = true;
                         });
        lateJoiner.connectToServer("127.0.0.1", port, "Invitado");
        assert(waitUntil([&]() { return lateGotHistory; }));
        lateJoiner.disconnectFromServer();
        std::cout << "  ✓ Live Team Chat broadcast & history replay verified\n";
    }

    // 4. Rapid consecutive pushes coalesce cleanly in TeamSyncClient without false Conflicts
    {
        quint64 ackedRev = 0;
        bool unexpectedConflict = false;
        QObject::connect(&clientLenin, &ea::gui::TeamSyncClient::pushAcknowledged,
                         [&](quint64 rev) { ackedRev = rev; });
        QObject::connect(&clientLenin, &ea::gui::TeamSyncClient::conflictDetected,
                         [&](quint64, const QString&, const QString&) { unexpectedConflict = true; });
        assert(clientLenin.pushLocalFile(leninLocalPath));
        assert(clientLenin.pushLocalFile(leninLocalPath)); // Queued while first push is in flight!
        assert(waitUntil([&]() { return ackedRev >= 4; }));
        assert(!unexpectedConflict && "Queued local pushes must not self-conflict");
        std::cout << "  ✓ Rapid consecutive local pushes serialized cleanly (rev #" << ackedRev << ", 0 false conflicts)\n";
    }

    // 5. Optimistic concurrency check: a stale push with baseRevision == 1 must be rejected with Conflict
    {
        QTcpSocket raw;
        std::vector<uint8_t> rx;
        bool gotWelcome = false;
        bool gotConflict = false;

        QObject::connect(&raw, &QTcpSocket::connected, [&]() {
            auto helloBytes = ea::sync::encodeFrame({ea::sync::MessageType::Hello, 0, "RaceUser", {}});
            raw.write(reinterpret_cast<const char*>(helloBytes.data()), static_cast<qint64>(helloBytes.size()));
            raw.flush();
        });
        QObject::connect(&raw, &QTcpSocket::readyRead, [&]() {
            QByteArray c = raw.readAll();
            rx.insert(rx.end(), reinterpret_cast<const uint8_t*>(c.constData()),
                      reinterpret_cast<const uint8_t*>(c.constData()) + c.size());
            ea::sync::Frame reply;
            while (ea::sync::tryDecodeFrame(rx, reply)) {
                if (reply.type == ea::sync::MessageType::Welcome) {
                    gotWelcome = true;
                    auto stalePush = ea::sync::encodeFrame({ea::sync::MessageType::PushUpdate, 1, "RaceUser", {9, 8, 7}});
                    raw.write(reinterpret_cast<const char*>(stalePush.data()), static_cast<qint64>(stalePush.size()));
                    raw.flush();
                } else if (reply.type == ea::sync::MessageType::Conflict && reply.revision == server.revision()) {
                    gotConflict = true;
                }
            }
        });

        raw.connectToHost("127.0.0.1", port);
        assert(waitUntil([&]() { return gotWelcome && gotConflict; }));
        (void)conflictTriggered;
        std::cout << "  ✓ Optimistic concurrency control rejected stale push with Conflict (rev #" << server.revision() << " preserved)\n";
    }

    // 6. Security checks: rolling .bak backup exists, corrupt SQLite push & OAZ1 zip-bomb are rejected
    {
        const fs::path bakPath = fs::path(masterPath.string() + ".bak");
        assert(fs::exists(bakPath) && fs::file_size(bakPath) >= 100 && "Rolling .bak backup must exist");

        const quint64 revBefore = server.revision();
        QTcpSocket attacker;
        std::vector<uint8_t> rx;
        bool gotWelcome = false;
        int errorCount = 0;

        QObject::connect(&attacker, &QTcpSocket::connected, [&]() {
            auto h = ea::sync::encodeFrame({ea::sync::MessageType::Hello, 0, "Attacker", {}});
            attacker.write(reinterpret_cast<const char*>(h.data()), static_cast<qint64>(h.size()));
            attacker.flush();
        });
        QObject::connect(&attacker, &QTcpSocket::readyRead, [&]() {
            QByteArray c = attacker.readAll();
            rx.insert(rx.end(), reinterpret_cast<const uint8_t*>(c.constData()),
                      reinterpret_cast<const uint8_t*>(c.constData()) + c.size());
            ea::sync::Frame reply;
            while (ea::sync::tryDecodeFrame(rx, reply)) {
                if (reply.type == ea::sync::MessageType::Welcome && !gotWelcome) {
                    gotWelcome = true;
                    // 6a. Non-SQLite garbage overwrite attempt
                    auto badPush = ea::sync::encodeFrame({ea::sync::MessageType::PushUpdate, revBefore, "Attacker",
                                                          {'N', 'O', 'T', '_', 'S', 'Q', 'L', 'I', 'T', 'E'}});
                    attacker.write(reinterpret_cast<const char*>(badPush.data()), static_cast<qint64>(badPush.size()));
                    // 6b. Forged OAZ1 zip-bomb declaring 512 MiB (0x20000000) uncompressed
                    std::vector<uint8_t> bomb = {'O', 'A', 'Z', '1', 0x20, 0x00, 0x00, 0x00, 0x78, 0x9c, 0x03, 0x00};
                    auto bombPush = ea::sync::encodeFrame({ea::sync::MessageType::PushUpdate, revBefore, "Attacker", bomb});
                    attacker.write(reinterpret_cast<const char*>(bombPush.data()), static_cast<qint64>(bombPush.size()));
                    attacker.flush();
                } else if (reply.type == ea::sync::MessageType::Error) {
                    ++errorCount;
                }
            }
        });
        attacker.connectToHost("127.0.0.1", port);
        assert(waitUntil([&]() { return gotWelcome && errorCount >= 2; }));
        assert(server.revision() == revBefore && "Malicious pushes must never advance revision or overwrite master");
        attacker.disconnectFromHost();
        std::cout << "  ✓ Security verified: rolling .bak created, corrupt SQLite & OAZ1 zip-bomb rejected\n";
    }

    clientLenin.disconnectFromServer();
    clientIran.disconnectFromServer();
    server.stop();

    // 7. Room PIN / Token authentication check
    {
        ea::gui::TeamSyncServer secured;
        assert(secured.start(0, QString::fromStdString(masterPath.string()), "SecretPIN-2026"));
        const quint16 secPort = secured.port();

        ea::gui::TeamSyncClient badClient;
        bool denied = false;
        QObject::connect(&badClient, &ea::gui::TeamSyncClient::syncError, [&](const QString&) { denied = true; });
        badClient.connectToServer("127.0.0.1", secPort, "Intruder", "WrongPIN");
        assert(waitUntil([&]() { return denied; }));
        badClient.disconnectFromServer();

        ea::gui::TeamSyncClient goodClient;
        bool joined = false;
        QObject::connect(&goodClient, &ea::gui::TeamSyncClient::connectedToTeam,
                         [&](quint64, const QString&, const QStringList&) { joined = true; });
        goodClient.connectToServer("127.0.0.1", secPort, "Authorized", "SecretPIN-2026");
        assert(waitUntil([&]() { return joined; }));
        goodClient.disconnectFromServer();
        secured.stop();
        std::cout << "  ✓ Room PIN authentication verified (wrong PIN rejected, valid PIN accepted)\n";
    }

    std::error_code ec;
    fs::remove(masterPath, ec);
    fs::remove(fs::path(masterPath.string() + ".bak"), ec);

    std::cout << "=======================================================\n";
    std::cout << " ALL LAN/VPN TEAM SYNC TESTS PASSED!                   \n";
    std::cout << "=======================================================\n";
    return 0;
}
