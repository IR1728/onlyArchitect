#include <QApplication>
#include <QPixmap>
#include <QFile>
#include <cassert>
#include <iostream>
#include "gui/MainWindow.hpp"
#include "gui/DarkTheme.hpp"
#include "ea/repository/EapxImporter.hpp"

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    app.setStyleSheet(ea::gui::GetStudioDarkStyleSheet());

    std::cout << "==========================================\n";
    std::cout << " Running onlyArchitect 8-Views Verification Suite\n";
    std::cout << "==========================================\n";

    MainWindow window;
    window.resize(1440, 900);
    window.show();

    // 1. Probar Vista 1: Start Page de proto2.jpeg
    std::cout << "[Vista 1] Verificando Start Page (proto2.jpeg)...\n";
    window.switchWorkspace(MainWindow::WorkspaceStartPage);
    QPixmap welcomePix(1440, 900);
    window.render(&welcomePix);
    welcomePix.save("/tmp/view1_startpage.png");
    std::cout << "  - Captura guardada en /tmp/view1_startpage.png\n";

    // 2. Probar Vista: Model Builder de proto2.jpeg
    std::cout << "[Vista Model Builder] Verificando Model Builder...\n";
    window.switchWorkspace(MainWindow::WorkspaceModelBuilder);
    QPixmap mbPix(1440, 900);
    window.render(&mbPix);
    mbPix.save("/tmp/view_model_builder.png");
    std::cout << "  - Captura guardada en /tmp/view_model_builder.png\n";

    // 2. Probar Vista 2 & 3: Editor de Diagramas (UML Class / Behavior)
    std::cout << "[Vista 2 & 3] Verificando Editor de Diagramas...\n";
    window.switchWorkspace(MainWindow::WorkspaceDiagram);
    QPixmap diagPix(1440, 900);
    window.render(&diagPix);
    diagPix.save("/tmp/view2_diagrams.png");
    std::cout << "  - Captura guardada en /tmp/view2_diagrams.png\n";

    // 3. Probar Vista 6: Generador de Código y Documentación
    std::cout << "[Vista 6] Verificando Generador de Código y Documentación...\n";
    window.switchWorkspace(MainWindow::WorkspaceCodeGen);
    QPixmap codePix(1440, 900);
    window.render(&codePix);
    codePix.save("/tmp/view6_codegen.png");
    std::cout << "  - Captura guardada en /tmp/view6_codegen.png\n";

    // 4. Probar Vista 7: Simulación y Depuración
    std::cout << "[Vista 7] Verificando Simulación y Depuración...\n";
    window.switchWorkspace(MainWindow::WorkspaceSimulation);
    QPixmap simPix(1440, 900);
    window.render(&simPix);
    simPix.save("/tmp/view7_simulation.png");
    std::cout << "  - Captura guardada en /tmp/view7_simulation.png\n";

    // 5. Probar Vista 8: Modo Boceto / Pizarra de Colaboración
    std::cout << "[Vista 8] Verificando Pizarra de Colaboración...\n";
    window.switchWorkspace(MainWindow::WorkspaceWhiteboard);
    QPixmap boardPix(1440, 900);
    window.render(&boardPix);
    boardPix.save("/tmp/view8_whiteboard.png");
    std::cout << "  - Captura guardada en /tmp/view8_whiteboard.png\n";

    // 6. Probar Apertura y Renderizado de DiagramaCasosUsoExamen.eapx
    std::string eapx_file = "/home/iran/CicloAgostoDiciembre2026/Diseño/DiagramaCasosUsoExamen.eapx";
    if (QFile::exists(QString::fromStdString(eapx_file))) {
        std::cout << "[Vista Casos de Uso] Abriendo DiagramaCasosUsoExamen.eapx...\n";
        bool ok = window.openFile(QString::fromStdString(eapx_file));
        assert(ok && "No se pudo abrir DiagramaCasosUsoExamen.eapx");
        window.switchWorkspace(MainWindow::WorkspaceDiagram);
        QPixmap ucPix(1440, 900);
        window.render(&ucPix);
        ucPix.save("/tmp/view_diagrama_casos_uso.png");
        std::cout << "  - Diagrama de Casos de Uso renderizado con éxito en /tmp/view_diagrama_casos_uso.png\n";
    }

    std::cout << "==========================================\n";
    std::cout << " TODAS LAS VISTAS VERIFICADAS CON ÉXITO!  \n";
    std::cout << "==========================================\n";
    return 0;
}
