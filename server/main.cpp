#include <QCoreApplication>
#include <QFileInfo>
#include <iostream>
#include <string>
#include "gui/TeamSyncServer.hpp"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    if (argc < 2) {
        std::cout << "Uso: onlyarchitect-server <repositorio.qea> [--port <puerto>] [--token <pin>]\n"
                  << "Ejemplo: onlyarchitect-server mi_proyecto.qea --port 7777 --token miClaveSegura\n";
        return 1;
    }

    const QString qeaPath = QString::fromLocal8Bit(argv[1]);
    quint16 port = 7777;
    QString roomToken;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            try {
                const int parsed = std::stoi(argv[++i]);
                if (parsed >= 1 && parsed <= 65535) port = static_cast<quint16>(parsed);
            } catch (...) {
                std::cerr << "Error: puerto inválido.\n";
                return 1;
            }
        } else if ((arg == "--token" || arg == "-t") && i + 1 < argc) {
            roomToken = QString::fromLocal8Bit(argv[++i]);
        }
    }

    if (!QFileInfo::exists(qeaPath)) {
        std::cerr << "Error: No existe el archivo " << qeaPath.toStdString() << "\n";
        return 1;
    }

    ea::gui::TeamSyncServer server;
    QObject::connect(&server, &ea::gui::TeamSyncServer::serverLog, [](const QString& msg) {
        std::cout << "[onlyarchitect-server] " << msg.toStdString() << "\n" << std::flush;
    });

    if (!server.start(port, qeaPath, roomToken)) {
        std::cerr << "Error al iniciar el servidor: " << server.lastError().toStdString() << "\n";
        return 1;
    }

    std::cout << "Presiona Ctrl+C para detener el servidor.\n" << std::flush;
    return app.exec();
}
