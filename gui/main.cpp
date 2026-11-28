#include <QApplication>
#include "MainWindow.hpp"
#include "DarkTheme.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("onlyArchitect Studio");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("onlyArchitect");

    // Material Design 3 on Nord: Fusion style, M3 palette and type scale, dynamic color from a seed
    ea::gui::DarkTheme::apply(MainWindow::savedThemeSeed());

    MainWindow window;
    if (argc > 1) {
        window.openFile(QString::fromLocal8Bit(argv[1]));
    }
    window.show();

    return app.exec();
}
