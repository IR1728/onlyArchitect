#include <QApplication>
#include <QPixmap>
#include <QFile>
#include <QDockWidget>
#include <QStackedWidget>
#include <QToolButton>
#include <QTableWidget>
#include <QTreeWidget>
#include <QShortcut>
#include <QTabWidget>
#include <QListWidget>
#include <QTemporaryDir>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include "gui/MainWindow.hpp"
#include "gui/DarkTheme.hpp"
#include "ea/repository/EapxImporter.hpp"

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    // Workspace layouts persist through QSettings; keep the test away from the user's config
    QTemporaryDir configDir;
    qputenv("XDG_CONFIG_HOME", configDir.path().toUtf8());
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
    welcomePix.save(configDir.filePath("view1_startpage.png"));

    // 2. Probar Vista: Model Builder de proto2.jpeg
    std::cout << "[Vista Model Builder] Verificando Model Builder...\n";
    window.switchWorkspace(MainWindow::WorkspaceModelBuilder);
    QPixmap mbPix(1440, 900);
    window.render(&mbPix);
    mbPix.save(configDir.filePath("view_model_builder.png"));

    // 2. Probar Vista 2 & 3: Editor de Diagramas (UML Class / Behavior)
    std::cout << "[Vista 2 & 3] Verificando Editor de Diagramas...\n";
    window.switchWorkspace(MainWindow::WorkspaceDiagram);
    QPixmap diagPix(1440, 900);
    window.render(&diagPix);
    diagPix.save(configDir.filePath("view2_diagrams.png"));

    // 3. Probar Vista 6: Generador de Código y Documentación
    std::cout << "[Vista 6] Verificando Generador de Código y Documentación...\n";
    window.switchWorkspace(MainWindow::WorkspaceCodeGen);
    QPixmap codePix(1440, 900);
    window.render(&codePix);
    codePix.save(configDir.filePath("view6_codegen.png"));

    // 4. Probar Vista 7: Simulación y Depuración
    std::cout << "[Vista 7] Verificando Simulación y Depuración...\n";
    window.switchWorkspace(MainWindow::WorkspaceSimulation);
    QPixmap simPix(1440, 900);
    window.render(&simPix);
    simPix.save(configDir.filePath("view7_simulation.png"));

    // 5. Probar Vista 8: Modo Boceto / Pizarra de Colaboración
    std::cout << "[Vista 8] Verificando Pizarra de Colaboración...\n";
    window.switchWorkspace(MainWindow::WorkspaceWhiteboard);
    QPixmap boardPix(1440, 900);
    window.render(&boardPix);
    boardPix.save(configDir.filePath("view8_whiteboard.png"));

    // 6. Prueba opcional de apertura y renderizado de un .eapx externo (ONLYARCHITECT_SAMPLE_EAPX)
    const char* envEapx = std::getenv("ONLYARCHITECT_SAMPLE_EAPX");
    std::string eapx_file = (envEapx && *envEapx) ? std::string(envEapx) : std::string{};
    if (!eapx_file.empty() && QFile::exists(QString::fromStdString(eapx_file))) {
        std::cout << "[Vista Casos de Uso] Abriendo DiagramaCasosUsoExamen.eapx...\n";
        bool ok = window.openFile(QString::fromStdString(eapx_file));
        assert(ok && "No se pudo abrir DiagramaCasosUsoExamen.eapx");
        window.switchWorkspace(MainWindow::WorkspaceDiagram);
        QPixmap ucPix(1440, 900);
        window.render(&ucPix);
        ucPix.save(configDir.filePath("view_diagrama_casos_uso.png"));
    }

    // 7. Cinta: una página por pestaña y botones conectados a las ventanas de análisis
    std::cout << "[Cinta] Verificando pestañas y botones de la cinta...\n";
    auto* ribbon = window.findChild<ea::gui::RibbonBarWidget*>();
    assert(ribbon);
    auto* ribbonStack = ribbon->findChild<QStackedWidget*>();
    assert(ribbonStack && ribbonStack->count() == 10);

    auto findButton = [](QWidget* page, const QString& label) -> QToolButton* {
        for (auto* b : page->findChildren<QToolButton*>()) {
            if (b->text().endsWith(label)) return b;
        }
        return nullptr;
    };
    ribbon->setCurrentTab(ea::gui::RibbonBarWidget::TabLayout);
    assert(findButton(ribbonStack->currentWidget(), "Diagram Layout") && "Layout tab must show the Layout ribbon");
    assert(!findButton(ribbonStack->currentWidget(), "Stop") && "Layout tab must not show simulation controls");

    auto dockNamed = [&window](const QString& title) -> QDockWidget* {
        for (auto* d : window.findChildren<QDockWidget*>()) {
            if (d->windowTitle() == title) return d;
        }
        return nullptr;
    };
    struct RibbonCase { int tab; QString button; QString dock; };
    const RibbonCase cases[] = {
        {ea::gui::RibbonBarWidget::TabStart, "Find in Project", "Find in Project"},
        {ea::gui::RibbonBarWidget::TabStart, "Traceability", "Traceability"},
        {ea::gui::RibbonBarWidget::TabDesign, "Matrix", "Relationship Matrix"},
        {ea::gui::RibbonBarWidget::TabDesign, "Specification", "Specification Manager"},
    };
    for (const auto& c : cases) {
        auto* dock = dockNamed(c.dock);
        assert(dock && !dock->isVisible());
        ribbon->setCurrentTab(c.tab);
        auto* btn = findButton(ribbonStack->currentWidget(), c.button);
        assert(btn);
        btn->click();
        assert(dock->isVisible());
        std::cout << "  - " << c.button.toStdString() << " -> " << c.dock.toStdString() << ": OK\n";
    }

    // Las ventanas conectadas muestran datos reales del modelo abierto.
    // Si no existe un modelo .qea de muestra externo, creamos un modelo temporal para probarlas igual.
    int traceTargetId = 88;
    auto* search = window.findChild<ea::gui::ModelSearchWidget*>();
    search->runSearch(ea::desktop::PredefinedSearchType::ClassesAndInterfaces, "Account");
    int hits = search->findChild<QTableWidget*>()->rowCount();
    if (hits == 0) {
        const QString tempModelPath = configDir.filePath("test_views_sample.qea");
        ea::Repository seedRepo;
        const bool created = seedRepo.CreateRepository(tempModelPath.toStdString());
        assert(created);
        auto rootPkg = seedRepo.GetModels().GetAt(0);
        assert(rootPkg);
        ea::Element accountElem;
        accountElem.SetName("Account");
        accountElem.SetType(ea::ObjectType::Class);
        accountElem.SetPackageID(rootPkg->GetPackageID());
        auto saved = seedRepo.AddElement(accountElem);
        assert(saved);
        traceTargetId = saved->GetElementID();
        seedRepo.CloseFile();

        const bool opened = window.openFile(tempModelPath);
        assert(opened);
        search->runSearch(ea::desktop::PredefinedSearchType::ClassesAndInterfaces, "Account");
        hits = search->findChild<QTableWidget*>()->rowCount();
    }
    std::cout << "  - Find in Project 'Account': " << hits << " resultados\n";
    assert(hits > 0);

    auto* trace = window.findChild<ea::gui::FocusAndTraceabilityWidget*>();
    auto* traceTree = trace->findChild<QTreeWidget*>();
    traceTree->clear();
    window.findChild<ModelTreeWidget*>()->elementSelected(traceTargetId);
    std::cout << "  - Traceability tras seleccionar elemento " << traceTargetId << ": " << traceTree->topLevelItemCount() << " raíz\n";
    assert(traceTree->topLevelItemCount() == 1);

    // 8. Selectores de ventanas Alt+1 .. Alt+7 (quick-acces-windows.md)
    std::cout << "[Alt+1..7] Verificando selectores de ventanas...\n";
    QShortcut* altShortcuts[7] = {};
    for (auto* sc : window.findChildren<QShortcut*>()) {
        for (int i = 0; i < 7; ++i) {
            if (sc->key() == QKeySequence(Qt::ALT | Qt::Key(Qt::Key_1 + i))) altShortcuts[i] = sc;
        }
    }
    for (int i = 0; i < 7; ++i) {
        assert(altShortcuts[i] && "Missing Alt+N shortcut");
        emit altShortcuts[i]->activated();
        auto* selector = window.findChild<ea::gui::WindowSelectorDialog*>();
        assert(selector && selector->isVisible());
        assert(selector->findChild<QTabWidget*>()->currentIndex() == i);
        if (i == 0) {
            selector->grab().save(configDir.filePath("view_window_selector_alt1.png"));
        }
        selector->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    std::cout << "  - Alt+1..Alt+7 abren su selector: OK\n";

    auto* searchDock = dockNamed("Find in Project");
    searchDock->hide();
    ribbon->setCurrentTab(ea::gui::RibbonBarWidget::TabStart);
    findButton(ribbonStack->currentWidget(), "Design")->click();
    auto* designSelector = window.findChild<ea::gui::WindowSelectorDialog*>();
    assert(designSelector && designSelector->findChild<QTabWidget*>()->currentIndex() == 0);
    QToolButton* searchItem = nullptr;
    int unavailable = 0;
    for (auto* b : designSelector->findChildren<QToolButton*>()) {
        if (b->text() == "Search") searchItem = b;
        if (!b->isEnabled()) ++unavailable;
    }
    assert(searchItem && searchItem->isEnabled());
    searchItem->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    assert(searchDock->isVisible());
    std::cout << "  - Start > All Windows > Design > Search abre Find in Project (" << unavailable
              << " ventanas aún no disponibles, deshabilitadas)\n";

    // 9. Workspace layouts (workspace.md)
    std::cout << "[Workspace Layouts] Verificando aplicar, guardar y restaurar...\n";
    assert(window.applyWorkspaceLayout("Clear"));
    for (auto* d : window.findChildren<QDockWidget*>()) assert(!d->isVisible());
    assert(window.applyWorkspaceLayout("Default"));
    assert(dockNamed("Browser")->isVisible() && dockNamed("Properties")->isVisible());
    assert(!searchDock->isVisible());
    assert(!window.applyWorkspaceLayout("Wide View") && "Wide View needs auto-hide windows");

    window.runWindowCommand("relationship_matrix");
    window.saveCurrentLayoutAs("Mi layout");
    assert(window.applyWorkspaceLayout("Clear"));
    assert(!dockNamed("Relationship Matrix")->isVisible());
    assert(window.applyWorkspaceLayout("Mi layout"));
    assert(dockNamed("Relationship Matrix")->isVisible() && dockNamed("Browser")->isVisible());
    ea::gui::WorkspaceLayoutStore store;
    assert(store.lastApplied() == "Mi layout");

    window.openWorkspaceLayoutDialog();
    auto* layoutDialog = window.findChild<ea::gui::WorkspaceLayoutDialog*>();
    assert(layoutDialog && layoutDialog->isVisible());
    bool listed = false;
    for (auto* item : layoutDialog->findChild<QListWidget*>()->findItems("Mi layout", Qt::MatchStartsWith)) {
        listed = item != nullptr;
    }
    assert(listed);
    layoutDialog->close();
    store.removeUserLayout("Mi layout");
    assert(!store.hasUserLayout("Mi layout"));
    std::cout << "  - Clear, Default, guardar 'Mi layout', aplicarlo y listarlo en el diálogo: OK\n";

    std::cout << "==========================================\n";
    std::cout << " TODAS LAS VISTAS VERIFICADAS CON ÉXITO!  \n";
    std::cout << "==========================================\n";
    return 0;
}
