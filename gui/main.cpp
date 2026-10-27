#include <QApplication>
#include <QStyleFactory>
#include "MainWindow.hpp"
#include "DarkTheme.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("OpenEA Studio");
    app.setApplicationVersion("17.2.1");
    app.setOrganizationName("OpenEA");

    // Modern Fusion style with Prototype Dark Studio Theme
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setStyleSheet(ea::gui::GetStudioDarkStyleSheet());

    MainWindow window;
    if (argc > 1) {
        window.openFile(QString::fromLocal8Bit(argv[1]));
    }
    window.show();

    return app.exec();
}
