#include "MainWindow.hpp"
#include <QColorDialog>
#include <QSettings>
#include "DarkTheme.hpp"
#include "M3Icons.hpp"
#include "EapToQeaTransferDialog.hpp"
#include "NewDiagramDialog.hpp"
#include "EditElementDialog.hpp"
#include "ServerConnectionDialog.hpp"
#include "ea/diagram/UmlDiagramKinds.hpp"
#include <QInputDialog>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QEventLoop>
#include <thread>
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QShortcut>
#include <QMenu>
#include <algorithm>
#include "ea/diagram/DiagramLayout.hpp"
#include "ea/export/Exporters.hpp"
#include "ea/xmi/Xmi.hpp"
#include "ea/editor/CanvasGeometry.hpp"
#include <QSignalBlocker>
#include <QTimer>
#include <cmath>
#include "ea/repository/EapxImporter.hpp"

namespace {

// A QStackedWidget whose minimumSizeHint does not grow to the union of all hidden workspaces
class ResponsiveStackedWidget : public QStackedWidget {
public:
    using QStackedWidget::QStackedWidget;
    QSize minimumSizeHint() const override { return QSize(320, 200); }
    QSize sizeHint() const override { return QSize(760, 520); }
};

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("onlyArchitect Studio");
    setMinimumSize(780, 520);
    if (const QScreen* screen = QGuiApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        const int w = std::clamp(static_cast<int>(avail.width() * 0.90), 900, 1400);
        const int h = std::clamp(static_cast<int>(avail.height() * 0.88), 600, 880);
        resize(w, h);
    } else {
        resize(1240, 760);
    }

    createRibbon();

    // Contenedor principal con Stack responsivo de espacios de trabajo
    m_workspaceStack = new ResponsiveStackedWidget(this);

    // 0. Start Page (Página de Inicio de proto2.jpeg)
    m_startPageWidget = new ea::gui::StartPageWidget(this);
    m_workspaceStack->addWidget(m_startPageWidget);

    // 1. Editor de Diagramas (con pestañas: Start Page | Components, etc.)
    m_diagramTabs = new QTabWidget(this);
    m_diagramTabs->setTabsClosable(true);
    m_diagramTabs->setMovable(true);
    ea::gui::setThemedStyleSheet(m_diagramTabs,
        "QTabWidget::pane { border: 1px solid @outlineVariant; background-color: @surface; }"
        "QTabBar::tab { background-color: @surfaceLow; color: @onSurfaceMuted; padding: 6px 14px; font-size: 11.5px; border-top-left-radius: 4px; border-top-right-radius: 4px; margin-right: 2px; border: 1px solid @outlineVariant; border-bottom: none; }"
        "QTabBar::tab:selected { background-color: @surfaceContainer; color: @primary; font-weight: bold; border-top: 2px solid @primary; }"
    );
    m_workspaceStack->addWidget(m_diagramTabs);

    // 2. Model Builder (Creador de Modelos UML)
    m_modelBuilderWidget = new ea::gui::ModelBuilderWidget(m_repo, this);
    m_workspaceStack->addWidget(m_modelBuilderWidget);
    connect(m_modelBuilderWidget, &ea::gui::ModelBuilderWidget::modelCreated, this, [this](int /*pkgId*/, int diagId) {
        m_treeWidget->populateTree();
        if (diagId > 0) {
            onDiagramDoubleClicked(diagId);
        }
        scheduleTeamSyncPush();
    });

    // 3. Generador de Código y Documentación
    m_codeGenWidget = new CodeGeneratorWidget(m_repo, this);
    m_workspaceStack->addWidget(m_codeGenWidget);

    // 4. Simulación y Depuración
    m_simWidget = new ea::SimulationWidget(m_repo, this);
    m_workspaceStack->addWidget(m_simWidget);

    // 5. Modo Boceto / Pizarra de Colaboración
    m_whiteboardWidget = new ea::WhiteboardWidget(m_repo, this);
    m_workspaceStack->addWidget(m_whiteboardWidget);

    // 6. Planificación de Proyecto (Kanban, Gantt, Calendario)
    m_plannerWidget = new ea::gui::ProjectPlannerWidget(m_repo, this);
    m_workspaceStack->addWidget(m_plannerWidget);
    connect(m_plannerWidget, &ea::gui::ProjectPlannerWidget::plannerDataChanged, this, [this]() {
        m_treeWidget->populateTree();
        scheduleTeamSyncPush();
    });
    connect(m_plannerWidget, &ea::gui::ProjectPlannerWidget::elementActivated, this, [this](int elemId) {
        onElementSelected(elemId);
        if (m_diagramTabs && m_diagramTabs->count() > 0) switchWorkspace(WorkspaceDiagram);
    });

    setCentralWidget(m_workspaceStack);

    connect(m_diagramTabs, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);

    // Conectar eventos de la Start Page
    connect(m_startPageWidget, &ea::gui::StartPageWidget::openProjectRequested, this, [this](const QString& path) {
        if (openFile(path)) {
            switchWorkspace(WorkspaceDiagram);
        }
    });
    connect(m_startPageWidget, &ea::gui::StartPageWidget::openFileBrowseRequested, this, &MainWindow::onOpenTriggered);
    connect(m_startPageWidget, &ea::gui::StartPageWidget::newProjectRequested, this, &MainWindow::onNewTriggered);
    connect(m_startPageWidget, &ea::gui::StartPageWidget::manageProjectsRequested, this, &MainWindow::onTransferEapToQeaTriggered);
    connect(m_startPageWidget, &ea::gui::StartPageWidget::serverConnectionRequested, this, &MainWindow::onServerConnectionTriggered);

    createDocks();
    createAnalysisDocks();
    connectRibbonCommands();
    setupWindowSelectorShortcuts();
    setupStatusBar();
    setupTeamSync();
    setupUndoRedo();
    m_baseDockState = saveState();

    switchWorkspace(WorkspaceStartPage);

    if (m_layoutStore.loadOnStartup()) {
        applyWorkspaceLayout(m_layoutStore.lastApplied());
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::createRibbon() {
    auto headerContainer = new QWidget(this);
    auto headerLayout = new QVBoxLayout(headerContainer);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(0);

    m_ribbonBar = new ea::gui::RibbonBarWidget(headerContainer);
    headerLayout->addWidget(m_ribbonBar);

    m_breadcrumbBar = new ea::gui::BreadcrumbBarWidget(headerContainer);
    headerLayout->addWidget(m_breadcrumbBar);

    setMenuWidget(headerContainer);

    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::showStartPageClicked, this, [this]() {
        switchWorkspace(WorkspaceStartPage);
    });
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::showModelBuilderClicked, this, [this]() {
        switchWorkspace(WorkspaceModelBuilder);
    });
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::serverConnectionClicked, this, &MainWindow::onServerConnectionTriggered);
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::showWhiteboardClicked, this, [this]() {
        switchWorkspace(WorkspaceWhiteboard);
    });
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::showChatClicked, this, [this]() {
        showDock(m_chatDock);
    });
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::showJournalClicked, this, [this]() {
        m_collabWidget->showTab(ea::gui::CollabHubWidget::TabJournal);
        showDock(m_collabDock);
    });
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::showDiscussClicked, this, [this]() {
        m_collabWidget->showTab(ea::gui::CollabHubWidget::TabDiscuss);
        showDock(m_collabDock);
    });
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::showReviewClicked, this, [this]() {
        m_collabWidget->showTab(ea::gui::CollabHubWidget::TabReview);
        showDock(m_collabDock);
    });
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::showKanbanClicked, this, [this]() {
        m_plannerWidget->showView(ea::gui::ProjectPlannerWidget::ViewKanban);
        switchWorkspace(WorkspacePlanner);
    });
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::showGanttClicked, this, [this]() {
        m_plannerWidget->showView(ea::gui::ProjectPlannerWidget::ViewGantt);
        switchWorkspace(WorkspacePlanner);
    });
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::showCalendarClicked, this, [this]() {
        m_plannerWidget->showView(ea::gui::ProjectPlannerWidget::ViewCalendar);
        switchWorkspace(WorkspacePlanner);
    });
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::showSqlConsoleClicked, this, [this]() {
        showDock(m_bottomDock);
    });
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::windowSelectorRequested, this, &MainWindow::openWindowSelector);
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::visualStyleClicked, this, &MainWindow::chooseThemeSeed);
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::fullScreenClicked, this, [this]() {
        isFullScreen() ? showNormal() : showFullScreen();
    });
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::helpClicked, this, &MainWindow::onAboutTriggered);
    connect(m_ribbonBar->workspaceMenu(), &QMenu::aboutToShow, this, &MainWindow::populateWorkspaceMenu);
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::openFileClicked, this, &MainWindow::onOpenTriggered);
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::newFileClicked, this, &MainWindow::onNewTriggered);
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::saveFileClicked, this, &MainWindow::onSaveTriggered);

    connect(m_breadcrumbBar, &ea::gui::BreadcrumbBarWidget::navigateHome, this, [this]() {
        switchWorkspace(WorkspaceStartPage);
    });
    connect(m_breadcrumbBar, &ea::gui::BreadcrumbBarWidget::navigateBack, this, [this]() {
        if (m_diagramTabs && m_diagramTabs->count() > 1) {
            int idx = (m_diagramTabs->currentIndex() - 1 + m_diagramTabs->count()) % m_diagramTabs->count();
            m_diagramTabs->setCurrentIndex(idx);
            switchWorkspace(WorkspaceDiagram);
        }
    });
    connect(m_breadcrumbBar, &ea::gui::BreadcrumbBarWidget::navigateForward, this, [this]() {
        if (m_diagramTabs && m_diagramTabs->count() > 1) {
            int idx = (m_diagramTabs->currentIndex() + 1) % m_diagramTabs->count();
            m_diagramTabs->setCurrentIndex(idx);
            switchWorkspace(WorkspaceDiagram);
        }
    });
    connect(m_breadcrumbBar, &ea::gui::BreadcrumbBarWidget::searchPackage, this, [this](const QString& q) {
        if (auto* filter = m_treeWidget ? m_treeWidget->findChild<QLineEdit*>() : nullptr) {
            filter->setText(q);
        }
    });
}

void MainWindow::createDocks() {
    // 1. Left Dock: Browser (Project Browser)
    m_browserDock = new QDockWidget("Browser", this);
    m_browserDock->setObjectName("BrowserDock");
    m_browserDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_treeWidget = new ModelTreeWidget(m_repo, this);
    m_browserDock->setWidget(m_treeWidget);
    addDockWidget(Qt::LeftDockWidgetArea, m_browserDock);

    connect(m_treeWidget, &ModelTreeWidget::diagramDoubleClicked, this, &MainWindow::onDiagramDoubleClicked);
    connect(m_treeWidget, &ModelTreeWidget::elementSelected, this, &MainWindow::onElementSelected);
    connect(m_treeWidget, &ModelTreeWidget::elementEditRequested, this, &MainWindow::onElementEditRequested);
    connect(m_treeWidget, &ModelTreeWidget::newPackageRequested, this, &MainWindow::onNewPackageRequested);
    connect(m_treeWidget, &ModelTreeWidget::renamePackageRequested, this, &MainWindow::onRenamePackageRequested);
    connect(m_treeWidget, &ModelTreeWidget::renameElementRequested, this, &MainWindow::onRenameElementRequested);
    connect(m_treeWidget, &ModelTreeWidget::deleteElementRequested, this, &MainWindow::onDeleteElementRequested);
    connect(m_treeWidget, &ModelTreeWidget::renameDiagramRequested, this, &MainWindow::onRenameDiagramRequested);
    connect(m_treeWidget, &ModelTreeWidget::refineUseCaseRequested, this, &MainWindow::refineUseCaseToSequence);

    // 2. Left Dock 2: Toolbox (Caja de herramientas de proto2.jpeg)
    m_toolboxDock = new QDockWidget("Toolbox", this);
    m_toolboxDock->setObjectName("ToolboxDock");
    m_toolboxDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_toolboxWidget = new ea::gui::ToolboxWidget(this);
    m_toolboxDock->setWidget(m_toolboxWidget);
    addDockWidget(Qt::LeftDockWidgetArea, m_toolboxDock);

    // Browser arriba-izquierda y Toolbox visible justo debajo para arrastrar piezas UML sin buscar pestañas ocultas
    splitDockWidget(m_browserDock, m_toolboxDock, Qt::Vertical);
    resizeDocks({m_browserDock, m_toolboxDock}, {240, 260}, Qt::Vertical);

    connect(m_toolboxWidget, &ea::gui::ToolboxWidget::toolActivated, this, &MainWindow::onToolActivated);

    // 3. Right Dock Top: Properties
    m_propertiesDock = new QDockWidget("Properties", this);
    m_propertiesDock->setObjectName("PropertiesDock");
    m_propertiesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_propsWidget = new PropertiesWidget(m_repo, this);
    m_propertiesDock->setWidget(m_propsWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);

    // 4. Right Dock Bottom: Notes (con formato WYSIWYG)
    m_notesDock = new QDockWidget("Notes", this);
    m_notesDock->setObjectName("NotesDock");
    m_notesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_notesWidget = new ea::gui::NotesWidget(m_repo, this);
    m_notesDock->setWidget(m_notesWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_notesDock);

    // 5. Bottom Dock: SQL Console
    m_bottomDock = new QDockWidget("SQL Console & Output", this);
    m_bottomDock->setObjectName("SqlConsoleDock");
    m_bottomDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    m_sqlWidget = new SqlConsoleWidget(m_repo, this);
    m_bottomDock->setWidget(m_sqlWidget);
    addDockWidget(Qt::BottomDockWidgetArea, m_bottomDock);
    m_bottomDock->hide(); // Oculto por defecto para coincidir con la vista limpia de proto2.jpeg
}

void MainWindow::createAnalysisDocks() {
    // Find in Project (Start > Explore > Search)
    m_searchDock = new QDockWidget("Find in Project", this);
    m_searchDock->setObjectName("FindInProjectDock");
    m_searchWidget = new ea::gui::ModelSearchWidget(m_repo, this);
    m_searchDock->setWidget(m_searchWidget);
    addDockWidget(Qt::BottomDockWidgetArea, m_searchDock);
    connect(m_searchWidget, &ea::gui::ModelSearchWidget::elementDoubleClicked, this, &MainWindow::onElementSelected);
    connect(m_searchWidget, &ea::gui::ModelSearchWidget::diagramDoubleClicked, this, &MainWindow::onDiagramDoubleClicked);

    // Relationship Matrix (Design > Package > Matrix)
    m_matrixDock = new QDockWidget("Relationship Matrix", this);
    m_matrixDock->setObjectName("RelationshipMatrixDock");
    m_matrixWidget = new ea::gui::RelationshipMatrixWidget(m_repo, this);
    m_matrixDock->setWidget(m_matrixWidget);
    addDockWidget(Qt::BottomDockWidgetArea, m_matrixDock);

    // Specification Manager (Design > Package > Specification View)
    m_specDock = new QDockWidget("Specification Manager", this);
    m_specDock->setObjectName("SpecificationManagerDock");
    m_specWidget = new ea::gui::SpecificationManagerWidget(m_repo, this);
    m_specDock->setWidget(m_specWidget);
    addDockWidget(Qt::BottomDockWidgetArea, m_specDock);
    connect(m_treeWidget, &ModelTreeWidget::packageSelected, m_specWidget, &ea::gui::SpecificationManagerWidget::setPackage);
    connect(m_treeWidget, &ModelTreeWidget::packageSelected, this, [this](int id) { m_activePackageId = id; });
    connect(m_treeWidget, &ModelTreeWidget::newDiagramRequested, this, &MainWindow::onNewDiagramTriggered);

    // Validación UML (Design > Package > Validate)
    m_validationDock = new QDockWidget("Validación UML", this);
    m_validationDock->setObjectName("ValidationDock");
    m_validationWidget = new ValidationResultsWidget(this);
    m_validationDock->setWidget(m_validationWidget);
    addDockWidget(Qt::BottomDockWidgetArea, m_validationDock);
    connect(m_validationWidget, &ValidationResultsWidget::validateRequested, this, [this]() { validateModel(); });
    connect(m_validationWidget, &ValidationResultsWidget::elementActivated, this, &MainWindow::onElementSelected);

    tabifyDockWidget(m_bottomDock, m_searchDock);
    tabifyDockWidget(m_specDock, m_validationDock);
    tabifyDockWidget(m_searchDock, m_matrixDock);
    tabifyDockWidget(m_matrixDock, m_specDock);

    // Traceability / Relationships / Summary (Start > Explore > Traceability)
    m_traceDock = new QDockWidget("Traceability", this);
    m_traceDock->setObjectName("TraceabilityDock");
    m_traceDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_traceWidget = new ea::gui::FocusAndTraceabilityWidget(m_repo, this);
    m_traceDock->setWidget(m_traceWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_traceDock);
    // Chat en vivo (LAN/VPN)
    m_chatDock = new QDockWidget("Chat de Equipo (LAN)", this);
    m_chatDock->setObjectName("TeamChatDock");
    m_chatDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
    m_chatWidget = new ea::gui::TeamChatWidget(this);
    m_chatDock->setWidget(m_chatWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_chatDock);
    connect(m_chatWidget, &ea::gui::TeamChatWidget::connectServerRequested, this, &MainWindow::onServerConnectionTriggered);

    // Bitácora, Discusiones y Revisiones (.qea)
    m_collabDock = new QDockWidget("Bitácora & Revisiones", this);
    m_collabDock->setObjectName("CollabHubDock");
    m_collabDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_collabWidget = new ea::gui::CollabHubWidget(m_repo, this);
    m_collabDock->setWidget(m_collabWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_collabDock);
    connect(m_collabWidget, &ea::gui::CollabHubWidget::collaborationDataChanged, this, &MainWindow::scheduleTeamSyncPush);

    tabifyDockWidget(m_notesDock, m_traceDock);
    tabifyDockWidget(m_traceDock, m_collabDock);
    tabifyDockWidget(m_collabDock, m_chatDock);
    m_notesDock->raise();

    // Ocultas hasta que se abren desde la cinta, como en EA
    for (auto* dock : {m_searchDock, m_matrixDock, m_specDock, m_validationDock, m_traceDock, m_collabDock, m_chatDock}) {
        dock->hide();
    }

    // Proporciones cómodas en cualquier pantalla (720p, 768p, 1080p, 4K)
    resizeDocks({m_browserDock, m_propertiesDock}, {230, 250}, Qt::Horizontal);
    resizeDocks({m_propertiesDock, m_notesDock}, {320, 150}, Qt::Vertical);
}

void MainWindow::showDock(QDockWidget* dock) {
    dock->show();
    dock->raise();
}

void MainWindow::connectRibbonCommands() {
    using Ribbon = ea::gui::RibbonBarWidget;

    connect(m_ribbonBar, &Ribbon::showSearchClicked, this, [this]() { runWindowCommand("search"); });
    connect(m_ribbonBar, &Ribbon::showTraceabilityClicked, this, [this]() { runWindowCommand("traceability"); });
    connect(m_ribbonBar, &Ribbon::showRelationshipMatrixClicked, this, [this]() { runWindowCommand("relationship_matrix"); });
    connect(m_ribbonBar, &Ribbon::showSpecificationClicked, this, [this]() { runWindowCommand("spec_manager"); });
    connect(m_ribbonBar, &Ribbon::showToolboxClicked, this, [this]() { runWindowCommand("toolbox"); });

    connect(m_ribbonBar, &Ribbon::addClassClicked, this, &MainWindow::onAddClassTriggered);
    connect(m_ribbonBar, &Ribbon::addInterfaceClicked, this, &MainWindow::onAddInterfaceTriggered);
    connect(m_ribbonBar, &Ribbon::addComponentClicked, this, [this]() {
        if (auto view = currentDiagramView()) view->addNewElement(ea::ObjectType::Component, "Component");
    });
    connect(m_ribbonBar, &Ribbon::addNoteClicked, this, &MainWindow::onAddNoteTriggered);
    connect(m_ribbonBar, &Ribbon::connectClicked, this, &MainWindow::onLinkElementsTriggered);

    connect(m_ribbonBar, &Ribbon::zoomInClicked, this, &MainWindow::onZoomInTriggered);
    connect(m_ribbonBar, &Ribbon::zoomOutClicked, this, &MainWindow::onZoomOutTriggered);
    connect(m_ribbonBar, &Ribbon::zoomResetClicked, this, &MainWindow::onResetZoomTriggered);
    connect(m_ribbonBar, &Ribbon::fitDiagramClicked, this, &MainWindow::onFitInViewTriggered);
    connect(m_ribbonBar, &Ribbon::autoLayoutClicked, this, &MainWindow::onDiagramLayoutTriggered);

    connect(m_ribbonBar, &Ribbon::generateCodeClicked, this, [this]() { switchWorkspace(WorkspaceCodeGen); });

    auto runSimulation = [this](void (ea::SimulationWidget::*action)()) {
        return [this, action]() {
            if (m_workspaceStack->currentIndex() != WorkspaceSimulation) openSimulator();
            (m_simWidget->*action)();
        };
    };
    connect(m_ribbonBar, &Ribbon::simulationStartClicked, this, runSimulation(&ea::SimulationWidget::play));
    connect(m_ribbonBar, &Ribbon::simulationStepClicked, this, runSimulation(&ea::SimulationWidget::stepForward));
    connect(m_ribbonBar, &Ribbon::simulationPauseClicked, this, runSimulation(&ea::SimulationWidget::pause));
    connect(m_ribbonBar, &Ribbon::simulationStopClicked, this, runSimulation(&ea::SimulationWidget::stop));

    connect(m_ribbonBar, &Ribbon::exportSvgClicked, this, &MainWindow::onExportSvgTriggered);
    connect(m_ribbonBar, &Ribbon::exportPngClicked, this, &MainWindow::onExportPngTriggered);
    connect(m_ribbonBar, &Ribbon::exportPlantUmlClicked, this, &MainWindow::onExportPlantUmlTriggered);
    connect(m_ribbonBar, &Ribbon::exportJsonClicked, this, &MainWindow::onExportJsonTriggered);
    connect(m_ribbonBar, &Ribbon::exportXmiClicked, this, &MainWindow::onExportXmiTriggered);
    connect(m_ribbonBar, &Ribbon::importXmiClicked, this, &MainWindow::onImportXmiTriggered);
}

void MainWindow::openSimulator() {
    auto view = currentDiagramView();
    m_simWidget->loadDiagramForSimulation(view ? view->getDiagramId() : 0);
    switchWorkspace(WorkspaceSimulation);
}

void MainWindow::setupWindowSelectorShortcuts() {
    for (int i = 0; i < ea::gui::WindowSelectorDialog::SelectorCount; ++i) {
        auto* shortcut = new QShortcut(QKeySequence(Qt::ALT | Qt::Key(Qt::Key_1 + i)), this);
        shortcut->setContext(Qt::ApplicationShortcut);
        connect(shortcut, &QShortcut::activated, this, [this, i]() { openWindowSelector(i); });
    }
}

void MainWindow::openWindowSelector(int selector) {
    auto* dlg = new ea::gui::WindowSelectorDialog(selector, m_layoutStore.userLayoutNames(), this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &ea::gui::WindowSelectorDialog::commandSelected, this, &MainWindow::runWindowCommand);
    dlg->open();
}

void MainWindow::runWindowCommand(const QString& command) {
    if (command.startsWith("workspace:")) {
        applyWorkspaceLayout(command.mid(QString("workspace:").size()));
    } else if (command == "validate_model") {
        validateModel();
    } else if (command == "new_diagram") {
        onNewDiagramTriggered();
    } else if (command == "xmi_export") {
        onExportXmiTriggered();
    } else if (command == "xmi_import") {
        onImportXmiTriggered();
    } else if (command == "browser") {
        showDock(m_browserDock);
    } else if (command == "traceability") {
        showDock(m_traceDock);
    } else if (command == "summary") {
        m_traceWidget->showSummary();
        showDock(m_traceDock);
    } else if (command == "search") {
        showDock(m_searchDock);
    } else if (command == "model_builder") {
        switchWorkspace(WorkspaceModelBuilder);
    } else if (command == "properties") {
        showDock(m_propertiesDock);
    } else if (command == "attributes" || command == "operations") {
        m_propsWidget->showTab(command == "attributes" ? "Attributes" : "Operations");
        showDock(m_propertiesDock);
    } else if (command == "notes") {
        showDock(m_notesDock);
    } else if (command == "spec_manager") {
        m_specWidget->refreshSpecification();
        showDock(m_specDock);
    } else if (command == "relationship_matrix") {
        showDock(m_matrixDock);
    } else if (command == "toolbox") {
        showDock(m_toolboxDock);
    } else if (command == "simulator") {
        openSimulator();
    } else if (command == "generate_code") {
        switchWorkspace(WorkspaceCodeGen);
    } else if (command == "workspace_dialog") {
        openWorkspaceLayoutDialog();
    } else if (command == "save_workspace") {
        promptSaveWorkspace();
    }
}

bool MainWindow::applyWorkspaceLayout(const QString& name) {
    QByteArray state;
    int central = WorkspaceDiagram;
    if (m_layoutStore.loadUserLayout(name, state, central)) {
        restoreState(state);
        switchWorkspace(central);
        m_layoutStore.setLastApplied(name);
        return true;
    }
    if (!ea::gui::isImplementedSystemLayout(name)) return false;

    // System workspace layouts restricted to the windows onlyArchitect provides;
    // the Browser dock also hosts the Inspector tab.
    std::vector<QDockWidget*> visible;
    if (name == "Default" || name == "Core Modeling" || name == "Software Engineering") {
        visible = {m_browserDock, m_toolboxDock, m_propertiesDock, m_notesDock};
    } else if (name == "Browse" || name == "Explore") {
        visible = {m_browserDock, m_traceDock};
    } else if (name == "Trace by Matrix") {
        visible = {m_browserDock, m_propertiesDock, m_notesDock, m_matrixDock};
    } else if (name == "Simulation") {
        visible = {m_browserDock};
    }

    restoreState(m_baseDockState);
    for (auto* dock : findChildren<QDockWidget*>()) {
        dock->setVisible(std::find(visible.begin(), visible.end(), dock) != visible.end());
    }
    if (m_browserDock->isVisible()) m_browserDock->raise();

    if (name == "Explore") m_traceWidget->showSummary();
    if (name == "Software Engineering") m_propsWidget->showTab("Attributes");
    if (name == "Simulation") openSimulator();
    else switchWorkspace(WorkspaceDiagram);

    m_layoutStore.setLastApplied(name);
    return true;
}

void MainWindow::saveCurrentLayoutAs(const QString& name) {
    if (name.isEmpty()) return;
    m_layoutStore.saveUserLayout(name, saveState(), m_workspaceStack->currentIndex());
}

void MainWindow::promptSaveWorkspace() {
    ea::gui::SaveWorkspaceLayoutDialog dlg(m_layoutStore.userLayoutNames(), this);
    if (dlg.exec() == QDialog::Accepted) saveCurrentLayoutAs(dlg.layoutName());
}

void MainWindow::openWorkspaceLayoutDialog() {
    auto* dlg = new ea::gui::WorkspaceLayoutDialog(m_layoutStore, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &ea::gui::WorkspaceLayoutDialog::applyRequested, this, &MainWindow::applyWorkspaceLayout);
    connect(dlg, &ea::gui::WorkspaceLayoutDialog::saveCurrentLayoutRequested, this, &MainWindow::saveCurrentLayoutAs);
    dlg->open();
}

void MainWindow::populateWorkspaceMenu() {
    auto* menu = m_ribbonBar->workspaceMenu();
    menu->clear();
    for (const auto& l : ea::gui::systemWorkspaceLayouts()) {
        if (l.implemented) menu->addAction(l.name, this, [this, n = l.name]() { applyWorkspaceLayout(n); });
    }
    const auto userLayouts = m_layoutStore.userLayoutNames();
    if (!userLayouts.isEmpty()) menu->addSeparator();
    for (const auto& name : userLayouts) {
        menu->addAction(name, this, [this, name]() { applyWorkspaceLayout(name); });
    }
    menu->addSeparator();
    menu->addAction("Workspace Layout...", this, &MainWindow::openWorkspaceLayoutDialog);
    menu->addAction("Save Workspace...", this, &MainWindow::promptSaveWorkspace);
}

void MainWindow::setupStatusBar() {
    auto bar = statusBar();
    ea::gui::setThemedStyleSheet(bar, "QStatusBar { background-color: @surfaceLowest; color: @onSurfaceMuted; font-size: 11px; border-top: 1px solid @outlineVariant; }");

    m_statusLabel = new QLabel("Listo — Abre o crea un repositorio (.qea, .eapx, .eap)", this);
    bar->addWidget(m_statusLabel, 1);
    // QStatusBar only hides normal widgets that are visible when a temporary message starts, so a
    // message shown before the window is (e.g. the undo feedback while opening) was painted over
    // the label. Keep the label hidden for as long as any temporary message is on screen.
    connect(bar, &QStatusBar::messageChanged, m_statusLabel,
            [label = m_statusLabel](const QString& message) { label->setVisible(message.isEmpty()); });

    m_perspectivesStatusLabel = new QLabel("All Perspectives  ", this);
    ea::gui::setThemedStyleSheet(m_perspectivesStatusLabel, "color: @onSurfaceVariant;");
    bar->addPermanentWidget(m_perspectivesStatusLabel);

    // Zoom slider
    auto zoomMinus = new QLabel("—", this);
    ea::gui::setThemedStyleSheet(zoomMinus, "color: @onSurfaceDisabled; font-weight: bold;");
    bar->addPermanentWidget(zoomMinus);

    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setRange(static_cast<int>(ea::editor::MinZoom * 100), static_cast<int>(ea::editor::MaxZoom * 100));
    m_zoomSlider->setValue(100);
    m_zoomSlider->setFixedWidth(90);
    m_zoomSlider->setToolTip("Zoom (10 % – 400 %)");
    connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int percent) {
        if (auto view = currentDiagramView()) view->setZoom(percent / 100.0);
    });
    bar->addPermanentWidget(m_zoomSlider);

    auto zoomPlus = new QLabel("+  ", this);
    ea::gui::setThemedStyleSheet(zoomPlus, "color: @onSurfaceDisabled; font-weight: bold;");
    bar->addPermanentWidget(zoomPlus);

    // Badges de estado de teclado / nube como en proto2.jpeg
    m_capLockLabel = new QLabel("CAP  ", this);
    ea::gui::setThemedStyleSheet(m_capLockLabel, "color: @onSurfaceDisabled; font-size: 10px;");
    bar->addPermanentWidget(m_capLockLabel);

    m_numLockLabel = new QLabel("NUM  ", this);
    ea::gui::setThemedStyleSheet(m_numLockLabel, "color: @primary; font-size: 10px; font-weight: bold;");
    bar->addPermanentWidget(m_numLockLabel);

    m_scrlLockLabel = new QLabel("SCRL  ", this);
    ea::gui::setThemedStyleSheet(m_scrlLockLabel, "color: @onSurfaceDisabled; font-size: 10px;");
    bar->addPermanentWidget(m_scrlLockLabel);

    m_cloudButton = new QToolButton(this);
    m_cloudButton->setText("LAN: OFFLINE");
    m_cloudButton->setToolTip("Colaboración en red local / VPN (haz clic para conectar o hospedar)");
    m_cloudButton->setAutoRaise(true);
    ea::gui::setThemedStyleSheet(m_cloudButton, "QToolButton { color: @onSurfaceDisabled; font-size: 10px; font-weight: bold; padding: 1px 6px; border: none; } QToolButton:hover { color: @primary; }");
    connect(m_cloudButton, &QToolButton::clicked, this, &MainWindow::onServerConnectionTriggered);
    bar->addPermanentWidget(m_cloudButton);
}

void MainWindow::setupTeamSync() {
    m_syncServer = new ea::gui::TeamSyncServer(this);
    m_syncClient = new ea::gui::TeamSyncClient(this);

    m_syncPushTimer = new QTimer(this);
    m_syncPushTimer->setSingleShot(true);
    m_syncPushTimer->setInterval(60);
    connect(m_syncPushTimer, &QTimer::timeout, this, &MainWindow::pushTeamSyncIfConnected);

    connect(m_syncClient, &ea::gui::TeamSyncClient::connectedToTeam, this,
            [this](quint64 rev, const QString& localPath, const QStringList& peers) {
                m_applyingRemoteSync = true;
                openFile(localPath);
                m_applyingRemoteSync = false;
                if (m_ribbonBar) m_ribbonBar->setConnectedUser(m_syncClient->username());
                if (m_collabWidget) m_collabWidget->setAuthorName(m_syncClient->username());
                if (m_plannerWidget) m_plannerWidget->setAuthorName(m_syncClient->username());
                if (m_chatWidget) {
                    m_chatWidget->setConnectionState(true, peers, m_syncClient->username());
                    m_chatWidget->appendSystemNotice(QString("Conectado a la sesión (rev #%1)").arg(rev));
                }
                updateTeamSyncBadge();
                statusBar()->showMessage(QString("Conectado en tiempo real (rev #%1, %2 en línea)").arg(rev).arg(peers.size()), 5000);
            });

    connect(m_syncClient, &ea::gui::TeamSyncClient::remoteUpdateReceived, this,
            [this](quint64 rev, const QString& author, const QString& localPath) {
                reloadFromRemoteSync(localPath);
                updateTeamSyncBadge();
                statusBar()->showMessage(QString("Sincronizado rev #%1 (%2)").arg(rev).arg(author), 3500);
            });

    connect(m_syncClient, &ea::gui::TeamSyncClient::pushAcknowledged, this, [this](quint64 rev) {
        updateTeamSyncBadge();
        statusBar()->showMessage(QString("Sincronizado rev #%1").arg(rev), 2000);
    });

    connect(m_syncClient, &ea::gui::TeamSyncClient::conflictDetected, this,
            [this](quint64 serverRev, const QString& /*msg*/, const QString& localPath) {
                reloadFromRemoteSync(localPath);
                updateTeamSyncBadge();
                statusBar()->showMessage(QString("Sincronizado a rev #%1 del servidor (cambio simultáneo resuelto)").arg(serverRev), 5000);
            });

    connect(m_syncClient, &ea::gui::TeamSyncClient::remoteCursorMoved, this,
            [this](const QString& username, int diagramId, float x, float y) {
                if (auto* view = diagramViewForId(diagramId)) {
                    view->updateRemoteCursor(username, QPointF(x, y));
                }
            });

    connect(m_syncClient, &ea::gui::TeamSyncClient::remoteElementMoved, this,
            [this](const QString& /*username*/, int diagramId, int elementId, int left, int top, int right, int bottom) {
                if (auto* view = diagramViewForId(diagramId)) {
                    view->applyRemoteElementGeometry(elementId, {left, top, right, bottom});
                }
            });

    connect(m_syncClient, &ea::gui::TeamSyncClient::peersUpdated, this, [this](const QStringList& peers) {
        updateTeamSyncBadge();
        if (m_chatWidget) m_chatWidget->setConnectionState(m_syncClient->isConnected(), peers, m_syncClient->username());
        for (int i = 0; i < m_diagramTabs->count(); ++i) {
            if (auto* view = qobject_cast<DiagramView*>(m_diagramTabs->widget(i))) {
                view->pruneRemoteCursors(peers);
            }
        }
    });

    connect(m_syncClient, &ea::gui::TeamSyncClient::chatMessageReceived, this,
            [this](const QString& author, const QString& text, qint64 ts) {
                if (m_chatWidget) m_chatWidget->appendChatMessage(author, text, ts);
                if (m_chatDock && !m_chatDock->isVisible()) {
                    statusBar()->showMessage(QString("💬 %1: %2").arg(author, text.left(80)), 5000);
                }
            });

    if (m_chatWidget) {
        connect(m_chatWidget, &ea::gui::TeamChatWidget::sendMessageRequested, this, [this](const QString& text) {
            if (m_syncClient && m_syncClient->isConnected()) {
                m_syncClient->sendChatMessage(text);
            }
        });
    }

    connect(m_syncClient, &ea::gui::TeamSyncClient::disconnectedFromTeam, this, [this]() {
        if (m_syncPushTimer) m_syncPushTimer->stop();
        if (m_ribbonBar) m_ribbonBar->setConnectedUser(QString());
        if (m_chatWidget) {
            m_chatWidget->setConnectionState(false, {}, QString());
            m_chatWidget->appendSystemNotice("Desconectado de la sesión.");
        }
        for (int i = 0; i < m_diagramTabs->count(); ++i) {
            if (auto* view = qobject_cast<DiagramView*>(m_diagramTabs->widget(i))) {
                view->clearRemoteCursors();
            }
        }
        updateTeamSyncBadge();
        statusBar()->showMessage("Desconectado del servidor de equipo.", 4000);
    });

    connect(m_syncClient, &ea::gui::TeamSyncClient::syncError, this, [this](const QString& message) {
        showWarning("Error de sincronización LAN/VPN", message);
    });
}

DiagramView* MainWindow::diagramViewForId(int diagramId) const {
    if (!m_diagramTabs || diagramId <= 0) return nullptr;
    for (int i = 0; i < m_diagramTabs->count(); ++i) {
        if (auto* view = qobject_cast<DiagramView*>(m_diagramTabs->widget(i)); view && view->getDiagramId() == diagramId) {
            return view;
        }
    }
    return nullptr;
}

void MainWindow::reloadFromRemoteSync(const QString& localPath) {
    if (localPath.isEmpty() || !QFile::exists(localPath)) return;
    if (m_syncPushTimer) m_syncPushTimer->stop();

    m_applyingRemoteSync = true;
    const bool opened = m_repo.OpenFile(localPath.toStdString());
    if (opened) {
        m_treeWidget->populateTree();

        // Refresh every open diagram tab in-place without destroying tabs or jumping zoom/camera!
        for (int i = m_diagramTabs->count() - 1; i >= 0; --i) {
            auto* view = qobject_cast<DiagramView*>(m_diagramTabs->widget(i));
            if (!view) continue;
            auto updatedDiagram = m_repo.GetDiagramByID(view->getDiagramId());
            if (updatedDiagram) {
                view->refreshDiagram(updatedDiagram);
                m_diagramTabs->setTabText(i, QString::fromStdString(updatedDiagram->GetName()));
            } else {
                m_diagramTabs->removeTab(i);
                delete view;
            }
        }

        // If no diagram tab was open yet and the remote peer created one, open it automatically
        if (m_diagramTabs->count() == 0 && m_repo.GetCounts().diagrams > 0) {
            for (size_t i = 0; i < m_repo.GetModels().size(); ++i) {
                if (auto m = m_repo.GetModels().GetAt(static_cast<short>(i))) {
                    auto findFirst = [](auto&& self, const ea::Package& pkg) -> int {
                        if (!pkg.GetDiagrams().empty()) {
                            if (auto d = pkg.GetDiagrams().GetAt(0)) return d->GetDiagramID();
                        }
                        for (size_t k = 0; k < pkg.GetPackages().size(); ++k) {
                            if (auto sub = pkg.GetPackages().GetAt(static_cast<short>(k))) {
                                if (int id = self(self, *sub); id > 0) return id;
                            }
                        }
                        return 0;
                    };
                    if (int firstId = findFirst(findFirst, *m); firstId > 0) {
                        onDiagramDoubleClicked(firstId);
                        break;
                    }
                }
            }
        }
        if (m_collabWidget) m_collabWidget->refresh();
        if (m_plannerWidget && m_workspaceStack->currentIndex() == WorkspacePlanner) {
            m_plannerWidget->refreshFromRepository();
        }
    }
    m_applyingRemoteSync = false;
}

void MainWindow::scheduleTeamSyncPush() {
    if (m_applyingRemoteSync || !m_syncClient || !m_syncClient->isConnected()) return;
    if (m_syncPushTimer) m_syncPushTimer->start();
}

void MainWindow::updateTeamSyncBadge() {
    if (!m_cloudButton) return;
    if (m_syncClient && m_syncClient->isConnected()) {
        const int online = std::max(1, static_cast<int>(m_syncClient->peers().size()));
        const QString role = (m_syncServer && m_syncServer->isRunning()) ? "HOST" : "LAN";
        m_cloudButton->setText(QString("%1: rev #%2 (%3 online)").arg(role).arg(m_syncClient->currentRevision()).arg(online));
        m_cloudButton->setToolTip(QString("Conectados: %1").arg(m_syncClient->peers().join(", ")));
        ea::gui::setThemedStyleSheet(m_cloudButton, "QToolButton { color: @success; font-size: 10px; font-weight: bold; padding: 1px 6px; border: none; }");
    } else {
        m_cloudButton->setText("LAN: OFFLINE");
        m_cloudButton->setToolTip("Colaboración en red local / VPN (haz clic para conectar o hospedar)");
        ea::gui::setThemedStyleSheet(m_cloudButton, "QToolButton { color: @onSurfaceDisabled; font-size: 10px; font-weight: bold; padding: 1px 6px; border: none; } QToolButton:hover { color: @primary; }");
    }
}

void MainWindow::pushTeamSyncIfConnected() {
    if (m_applyingRemoteSync || !m_syncClient || !m_syncClient->isConnected()) return;
    // Persist any moved/resized diagram objects to SQLite before snapshotting (without clearing Undo)
    for (int i = 0; i < m_diagramTabs->count(); ++i) {
        if (auto* view = qobject_cast<DiagramView*>(m_diagramTabs->widget(i)); view && view->getDiagram()) {
            m_repo.SaveDiagram(*view->getDiagram());
        }
    }
    const QString localPath = m_syncClient->localWorkspacePath();
    if (!localPath.isEmpty() && QFile::exists(localPath)) {
        m_syncClient->pushLocalFile(localPath);
    }
}

void MainWindow::onServerConnectionTriggered() {
    const QString currentPath = m_repo.IsOpen() ? QString::fromStdString(m_repo.GetConnectionString()) : QString();
    const bool hosting = m_syncServer && m_syncServer->isRunning();
    const bool connected = m_syncClient && m_syncClient->isConnected();
    const quint16 activePort = hosting ? m_syncServer->port() : 7777;
    const QStringList peers = connected ? m_syncClient->peers() : (hosting ? m_syncServer->connectedUsers() : QStringList{});

    ea::gui::ServerConnectionDialog dlg(currentPath, hosting, connected, activePort, peers, this);
    if (dlg.exec() != QDialog::Accepted) return;

    using Action = ea::gui::ServerConnectionDialog::Action;
    if (dlg.chosenAction() == Action::Disconnect) {
        if (m_syncClient) m_syncClient->disconnectFromServer();
        if (m_syncServer) m_syncServer->stop();
        updateTeamSyncBadge();
    } else if (dlg.chosenAction() == Action::ConnectClient) {
        if (m_syncServer && m_syncServer->isRunning()) m_syncServer->stop();
        m_syncClient->connectToServer(dlg.hostAddress(), dlg.port(), dlg.username(), dlg.roomToken());
    } else if (dlg.chosenAction() == Action::HostServer) {
        const QString file = dlg.hostFilePath();
        if (file.isEmpty() || !QFile::exists(file)) {
            showWarning("Hospedar Proyecto", "Selecciona un archivo .qea válido antes de iniciar el servidor.");
            return;
        }
        const QString token = dlg.roomToken();
        if (!m_syncServer->start(dlg.port(), file, token)) {
            showWarning("No se pudo iniciar el servidor", m_syncServer->lastError());
            return;
        }
        m_syncClient->connectToServer("127.0.0.1", m_syncServer->port(), dlg.username(), token);
    }
}

void MainWindow::switchWorkspace(int index) {
    if (index < 0 || index >= m_workspaceStack->count()) return;
    m_workspaceStack->setCurrentIndex(index);

    if (index == WorkspaceStartPage) {
        m_startPageWidget->refreshRecentProjects();
    } else if (index == WorkspaceCodeGen) {
        m_codeGenWidget->refreshFromRepository();
    } else if (index == WorkspaceWhiteboard) {
        m_whiteboardWidget->refreshFromRepository();
    } else if (index == WorkspacePlanner) {
        m_plannerWidget->refreshFromRepository();
    }
}

DiagramView* MainWindow::currentDiagramView() const {
    return qobject_cast<DiagramView*>(m_diagramTabs->currentWidget());
}

namespace {
int findFirstDiagramId(const ea::Package& pkg) {
    if (!pkg.GetDiagrams().empty()) {
        if (auto d = pkg.GetDiagrams().GetAt(0)) {
            return d->GetDiagramID();
        }
    }
    for (size_t i = 0; i < pkg.GetPackages().size(); ++i) {
        if (auto sub = pkg.GetPackages().GetAt(static_cast<short>(i))) {
            if (int found = findFirstDiagramId(*sub); found > 0) {
                return found;
            }
        }
    }
    return 0;
}
} // namespace

bool MainWindow::openFile(const QString& filePath) {
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_statusLabel->setText(QString("Abriendo: %1 ...").arg(QFileInfo(filePath).fileName()));
    QApplication::processEvents();

    const bool opened = m_repo.OpenFile(filePath.toStdString());
    QApplication::restoreOverrideCursor();

    if (!opened) {
        m_statusLabel->setText("Error al abrir el repositorio.");
        QMessageBox::critical(this, "Error", QString("No se pudo abrir el repositorio:\n%1").arg(QString::fromStdString(m_repo.GetLastError())));
        return false;
    }

    ea::gui::StartPageWidget::addRecentProject(filePath);
    setWindowTitle(QString("onlyArchitect Studio — %1").arg(QFileInfo(filePath).fileName()));

    m_treeWidget->populateTree();
    m_activePackageId = 0;
    // Views of the previous repository (and their undo histories) are discarded
    while (m_diagramTabs->count() > 0) {
        auto* w = m_diagramTabs->widget(0);
        m_diagramTabs->removeTab(0);
        delete w;
    }
    m_propsWidget->clear();
    m_notesWidget->clear();
    if (m_modelEdits) m_modelEdits->undoStack()->clear(); // its commands refer to the closed repository

    auto counts = m_repo.GetCounts();
    m_statusLabel->setText(QString("Abierto: %1 (%2 elementos, %3 diagramas)")
                               .arg(QFileInfo(filePath).fileName())
                               .arg(counts.elements)
                               .arg(counts.diagrams));

    int firstDiagramId = 0;
    if (counts.diagrams > 0) {
        for (size_t i = 0; i < m_repo.GetModels().size(); ++i) {
            auto m = m_repo.GetModels().GetAt(static_cast<short>(i));
            if (m) {
                firstDiagramId = findFirstDiagramId(*m);
                if (firstDiagramId > 0) break;
            }
        }
    }

    if (firstDiagramId > 0) {
        onDiagramDoubleClicked(firstDiagramId);
    } else {
        switchWorkspace(WorkspaceDiagram);
    }

    if (m_browserDock) {
        m_browserDock->show();
        m_browserDock->raise();
    }

    if (m_repo.GetModels().size() > 0) {
        m_specWidget->setPackage(m_repo.GetModels().GetAt(0)->GetPackageID());
    }
    if (m_collabWidget) m_collabWidget->setCurrentElement(0);
    if (m_plannerWidget) m_plannerWidget->refreshFromRepository();
    return true;
}

void MainWindow::onOpenTriggered() {
    QString path = QFileDialog::getOpenFileName(this, "Abrir Repositorio", "",
        "Archivos de Repositorio (*.qea *.feap *.eapx *.eap);;Repositorios SQLite (*.qea);;Proyectos Heredados (*.eapx *.eap);;Todos los Archivos (*)");
    if (!path.isEmpty()) {
        if (ea::EapxImporter::IsEapxFile(path.toStdString())) {
            const auto fmt = ea::EapxImporter::DetectFormat(path.toStdString());
            QFileInfo fi(path);
            QString suggestedQea = fi.absolutePath() + "/" + fi.completeBaseName() + ".qea";
            auto btn = QMessageBox::question(this, "Proyecto EAP / EAPX Detectado",
                QString("El archivo <b>%1</b> usa el formato <b>%2</b>.<br><br>"
                        "¿Deseas migrarlo al formato moderno SQLite (.qea)?<br>"
                        "• <b>Sí</b>: Guardar como %3 y abrir.<br>"
                        "• <b>No</b>: Abrir mediante caché temporal sin modificar tu carpeta.")
                    .arg(fi.fileName(),
                         QString::fromStdString(ea::EapxImporter::FormatName(fmt)),
                         fi.completeBaseName() + ".qea"),
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
            if (btn == QMessageBox::Cancel) return;

            const std::string targetPath = (btn == QMessageBox::Yes) ? suggestedQea.toStdString() : std::string{};
            QProgressDialog progress("Analizando archivo...", QString(), 0, 100, this);
            progress.setWindowTitle("Migrando proyecto heredado");
            progress.setWindowModality(Qt::WindowModal);
            progress.setMinimumDuration(0);
            progress.setValue(5);
            progress.show();

            QEventLoop loop;
            ea::EapxImporter::ConversionResult res;
            std::thread worker([&]() {
                res = ea::EapxImporter::ConvertToSqliteDetailed(
                    path.toStdString(), targetPath,
                    [&](int current, int total, const std::string& msg) {
                        QString qmsg = QString::fromStdString(msg);
                        QMetaObject::invokeMethod(&progress, [&, current, total, qmsg]() {
                            progress.setMaximum(total);
                            progress.setValue(current);
                            progress.setLabelText(qmsg);
                        }, Qt::QueuedConnection);
                    });
                QMetaObject::invokeMethod(&loop, &QEventLoop::quit, Qt::QueuedConnection);
            });
            loop.exec();
            worker.join();
            progress.close();

            if (!res.ok) {
                QMessageBox::critical(this, "Error al abrir EAP/EAPX", QString::fromStdString(res.error));
                return;
            }
            if (openFile(QString::fromStdString(res.outputPath))) {
                switchWorkspace(WorkspaceDiagram);
            }
            return;
        }
        if (openFile(path)) {
            switchWorkspace(WorkspaceDiagram);
        }
    }
}

void MainWindow::onTransferEapToQeaTriggered() {
    EapToQeaTransferDialog dlg("", this);
    if (dlg.exec() == QDialog::Accepted) {
        QString generatedQea = dlg.getTargetQeaPath();
        if (!generatedQea.isEmpty() && QFile::exists(generatedQea)) {
            if (openFile(generatedQea)) {
                switchWorkspace(WorkspaceDiagram);
            }
        }
    }
}

void MainWindow::onNewTriggered() {
    QString path = QFileDialog::getSaveFileName(this, "Crear Nuevo Repositorio", "NuevoProyecto.qea", "Repositorios SQLite (*.qea)");
    if (!path.isEmpty()) {
        if (m_repo.CreateRepository(path.toStdString())) {
            openFile(path);
            switchWorkspace(WorkspaceDiagram);
            QMessageBox::information(this, "Éxito", "Repositorio UML (.qea) creado correctamente.");
        }
    }
}

void MainWindow::onSaveTriggered() {
    auto view = currentDiagramView();
    if (view && view->saveDiagramLayout()) {
        statusBar()->showMessage("Disposición del diagrama guardada.", 3000);
        pushTeamSyncIfConnected();
    }
}

void MainWindow::onExportSvgTriggered() {
    auto view = currentDiagramView();
    if (view) {
        QString path = QFileDialog::getSaveFileName(this, "Exportar a SVG", "diagram.svg", "SVG (*.svg)");
        if (!path.isEmpty()) view->exportToSvg(path);
    }
}

void MainWindow::onExportPngTriggered() {
    auto view = currentDiagramView();
    if (view) {
        QString path = QFileDialog::getSaveFileName(this, "Exportar a PNG", "diagram.png", "PNG (*.png)");
        if (!path.isEmpty()) view->exportToPng(path);
    }
}

void MainWindow::onExportPlantUmlTriggered() {
    auto view = currentDiagramView();
    if (view && view->getDiagram()) {
        QString path = QFileDialog::getSaveFileName(this, "Exportar a PlantUML", "diagram.puml", "PlantUML (*.puml)");
        if (!path.isEmpty()) {
            ea::PlantUMLExporter::ExportDiagramToFile(*view->getDiagram(), m_repo, path.toStdString());
        }
    }
}

void MainWindow::onExportJsonTriggered() {
    QString path = QFileDialog::getSaveFileName(this, "Exportar Repositorio a JSON", "repo.json", "JSON (*.json)");
    if (!path.isEmpty()) {
        ea::JSONExporter::ExportRepositoryToFile(m_repo, path.toStdString());
    }
}

void MainWindow::onExportXmiTriggered() {
    if (!m_repo.IsOpen()) return;
    const int packageId = defaultPackageForNewDiagram();
    auto pkg = m_repo.GetPackageByID(packageId);
    if (!pkg) return;
    const QString path = QFileDialog::getSaveFileName(this, "Exportar paquete a XMI", QString::fromStdString(pkg->GetName()) + ".xmi",
                                                      "XMI (*.xmi *.xml)");
    if (!path.isEmpty()) exportXmi(packageId, path);
}

void MainWindow::onImportXmiTriggered() {
    if (!m_repo.IsOpen()) return;
    const QString path = QFileDialog::getOpenFileName(this, "Importar XMI", QString(), "XMI (*.xmi *.xml);;Todos (*)");
    if (!path.isEmpty()) importXmi(defaultPackageForNewDiagram(), path);
}

bool MainWindow::exportXmi(int packageId, const QString& path) {
    auto pkg = m_repo.GetPackageByID(packageId);
    if (!pkg) return false;
    ea::xmi::ExportResult result;
    if (!ea::xmi::export_package_to_file(m_repo, *pkg, path.toStdString(), &result)) {
        showWarning("Exportar XMI", "No se pudo escribir " + path);
        return false;
    }
    QString summary = QString("XMI exportado: %1 paquetes, %2 elementos, %3 relaciones, %4 diagramas")
                          .arg(result.packages).arg(result.elements).arg(result.relationships).arg(result.diagrams);
    if (!result.warnings.empty()) summary += QString(" · %1").arg(QString::fromStdString(result.warnings.front()));
    statusBar()->showMessage(summary, 8000);
    return true;
}

bool MainWindow::importXmi(int packageId, const QString& path) {
    const auto result = ea::xmi::import_file(m_repo, packageId, path.toStdString());
    if (!result.ok) {
        if (result.repositoryReloadNeeded) openFile(QString::fromStdString(m_repo.GetConnectionString()));
        showWarning("Importar XMI", QString::fromStdString(result.error));
        return false;
    }
    onModelChanged();
    QString summary = QString("XMI importado: %1 paquetes, %2 elementos, %3 relaciones, %4 diagramas")
                          .arg(result.packages).arg(result.elements).arg(result.relationships).arg(result.diagrams);
    if (!result.warnings.empty()) {
        QStringList details;
        for (const auto& w : result.warnings) details << QString::fromStdString(w);
        showWarning("Importar XMI", summary + QString("\n\nSe omitieron partes del archivo:\n• ") + details.join("\n• "));
    }
    statusBar()->showMessage(summary, 8000);
    return true;
}

void MainWindow::onDiagramLayoutTriggered() {
    auto view = currentDiagramView();
    if (view) view->applyAutoLayout(); // undoable
}

void MainWindow::onAddClassTriggered() {
    auto view = currentDiagramView();
    if (view) view->addNewElement(ea::ObjectType::Class, "NewClass");
}

void MainWindow::onAddInterfaceTriggered() {
    auto view = currentDiagramView();
    if (view) view->addNewElement(ea::ObjectType::Interface, "INewInterface");
}

void MainWindow::onAddNoteTriggered() {
    auto view = currentDiagramView();
    if (view) view->addNewElement(ea::ObjectType::Note, "Note");
}

void MainWindow::onLinkElementsTriggered() {
    auto view = currentDiagramView();
    if (view) view->startLinkMode();
}

void MainWindow::onZoomInTriggered() {
    auto view = currentDiagramView();
    if (view) view->zoomIn();
}

void MainWindow::onZoomOutTriggered() {
    auto view = currentDiagramView();
    if (view) view->zoomOut();
}

void MainWindow::onResetZoomTriggered() {
    auto view = currentDiagramView();
    if (view) view->resetZoom();
}

void MainWindow::onFitInViewTriggered() {
    auto view = currentDiagramView();
    if (view) view->fitInView();
}

void MainWindow::onTogglePanTriggered(bool checked) {
    auto view = currentDiagramView();
    if (view) view->setPanMode(checked);
}

void MainWindow::onAboutTriggered() {
    QMessageBox::about(this, "onlyArchitect Studio",
        "<h3>onlyArchitect Studio 1.0</h3>"
        "<p>Suite nativa de modelado y arquitectura de software basada en C++20 y Qt 6.</p>"
        "<p>Herramienta completa de modelado con Ribbon, Start Page, Toolbox, Browser, Properties y Diagram Editor.</p>");
}

void MainWindow::onDiagramDoubleClicked(int diagramId) {
    for (int i = 0; i < m_diagramTabs->count(); ++i) {
        auto view = qobject_cast<DiagramView*>(m_diagramTabs->widget(i));
        if (view && view->getDiagramId() == diagramId) {
            m_diagramTabs->setCurrentIndex(i);
            switchWorkspace(WorkspaceDiagram);
            return;
        }
    }

    auto d = m_repo.GetDiagramByID(diagramId);
    if (d) {
        auto view = new DiagramView(d, m_repo, this);
        QString symbol = "schema";
        if (d->GetType() == ea::DiagramType::Component) symbol = "extension";
        else if (d->GetType() == ea::DiagramType::Sequence) symbol = "sync_alt";
        else if (d->GetType() == ea::DiagramType::Activity) symbol = "conversion_path";

        int tabIdx = m_diagramTabs->addTab(view, ea::gui::M3Icons::icon(symbol), QString::fromStdString(d->GetName()));
        m_diagramTabs->setCurrentIndex(tabIdx);

        connect(view, &DiagramView::elementSelected, this, &MainWindow::onElementSelected);
        connect(view, &DiagramView::zoomChanged, this, [this, view](int percent) {
            if (view != currentDiagramView()) return;
            const QSignalBlocker block(m_zoomSlider); // the slider follows; it must not zoom back
            m_zoomSlider->setValue(percent);
            statusBar()->showMessage(QString("Zoom %1 %").arg(percent), 1500);
        });
        connect(view, &DiagramView::modelChanged, this, &MainWindow::onModelChanged);
        connect(view, &DiagramView::refineUseCaseRequested, this, &MainWindow::refineUseCaseToSequence);
        connect(view, &DiagramView::diagramGeometryCommitted, this, &MainWindow::scheduleTeamSyncPush);
        connect(view, &DiagramView::cursorMovedInScene, this, [this](int diagId, const QPointF& pos) {
            if (!m_applyingRemoteSync && m_syncClient && m_syncClient->isConnected()) {
                m_syncClient->sendCursorMove(diagId, static_cast<float>(pos.x()), static_cast<float>(pos.y()));
            }
        });
        connect(view, &DiagramView::elementLiveMoved, this,
                [this](int diagId, int elemId, const ea::editor::ElementGeometry& g) {
                    if (!m_applyingRemoteSync && m_syncClient && m_syncClient->isConnected()) {
                        m_syncClient->sendElementMove(diagId, elemId, g.left, g.top, g.right, g.bottom);
                    }
                });
        connect(view, &DiagramView::editFailed, this, [this](const QString& message) {
            showWarning("No se pudo completar la edición", message);
        });
        connect(view, &DiagramView::editRejected, this, [this](const QString& reason) {
            statusBar()->showMessage("Relación no válida: " + reason, 6000);
            showWarning("Relación no válida", reason);
        });
        m_undoGroup->addStack(view->undoStack());
        m_undoGroup->setActiveStack(view->undoStack());
        m_propsWidget->showDiagram(diagramId);

        if (m_breadcrumbBar) {
            QString rootName = "Model";
            if (m_repo.GetModels().size() > 0 && m_repo.GetModels().GetAt(0)) {
                rootName = QString::fromStdString(m_repo.GetModels().GetAt(0)->GetName());
            }
            m_breadcrumbBar->setPath(rootName, QString::fromStdString(d->GetName()));
        }

        switchWorkspace(WorkspaceDiagram);
        if (!d->GetDiagramObjects().empty()) {
            QTimer::singleShot(0, view, [view]() { view->fitInView(); });
        }
    }
}

void MainWindow::onElementSelected(int elementId) {
    m_propsWidget->showElement(elementId);
    auto elem = m_repo.GetElementByID(elementId);
    if (elem) {
        m_notesWidget->setElement(elem);
    }
    m_codeGenWidget->generateForElement(elementId);

    auto view = currentDiagramView();
    m_traceWidget->inspectElement(elementId, view ? view->getDiagram().get() : nullptr);
    if (m_collabWidget) m_collabWidget->setCurrentElement(elementId);
}

void MainWindow::setupUndoRedo() {
    m_undoGroup = new QUndoGroup(this);

    auto* undoAct = m_undoGroup->createUndoAction(this, "Deshacer");
    undoAct->setShortcuts(QKeySequence::Undo);
    undoAct->setShortcutContext(Qt::WindowShortcut);
    addAction(undoAct);

    auto* redoAct = m_undoGroup->createRedoAction(this, "Rehacer");
    QList<QKeySequence> redoKeys = {QKeySequence(Qt::CTRL | Qt::Key_Y)};
    for (const auto& k : QKeySequence::keyBindings(QKeySequence::Redo)) {
        if (!redoKeys.contains(k)) redoKeys.append(k);
    }
    redoAct->setShortcuts(redoKeys);
    redoAct->setShortcutContext(Qt::WindowShortcut);
    addAction(redoAct);

    // Property Editor edits: undoable in the active diagram's history, or in the model's when none is open
    m_modelEdits = new ModelEditSession(m_repo, this);
    m_undoGroup->addStack(m_modelEdits->undoStack());
    m_undoGroup->setActiveStack(m_modelEdits->undoStack());
    connect(m_propsWidget, &PropertiesWidget::editRequested, this, &MainWindow::applyPropertyEdit);
    connect(m_modelEdits, &ModelEditSession::elementEdited, this, &MainWindow::onElementSelected);
    connect(m_modelEdits, &ModelEditSession::modelChanged, this, &MainWindow::onModelChanged);
    connect(m_modelEdits, &ModelEditSession::editFailed, this, [this](const QString& message) {
        showWarning("No se pudo completar la edición", message);
    });

    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::newDiagramClicked, this, [this]() { onNewDiagramTriggered(); });
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::validateModelClicked, this, [this]() { validateModel(); });
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::undoClicked, m_undoGroup, &QUndoGroup::undo);
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::redoClicked, m_undoGroup, &QUndoGroup::redo);

    // Ctrl+Z acts on the diagram in the active tab
    connect(m_diagramTabs, &QTabWidget::currentChanged, this, [this](int) {
        auto view = currentDiagramView();
        m_undoGroup->setActiveStack(activeUndoStack(view));
        if (view) {
            const QSignalBlocker block(m_zoomSlider);
            m_zoomSlider->setValue(static_cast<int>(std::lround(view->zoom() * 100)));
        }
        showPaletteFor(view);
    });

    // Feedback in the status bar after each undo/redo + trigger debounced LAN sync on any canvas command
    connect(m_undoGroup, &QUndoGroup::indexChanged, this, [this](int) {
        auto* stack = m_undoGroup->activeStack();
        if (!stack) return;
        QString msg = stack->canUndo() ? QString("Último cambio: %1").arg(stack->undoText()) : QString("Sin cambios pendientes");
        statusBar()->showMessage(msg, 2500);
        scheduleTeamSyncPush();
    });
}

int MainWindow::defaultPackageForNewDiagram() const {
    // Package selected in the Model Explorer, else the one of the diagram being edited, else the first model
    if (m_activePackageId > 0 && m_repo.IsOpen()) return m_activePackageId;
    if (auto view = currentDiagramView(); view && view->getDiagram()) return view->getDiagram()->GetPackageID();
    if (m_repo.GetModels().size() > 0) return m_repo.GetModels().GetAt(0)->GetPackageID();
    return 0;
}

void MainWindow::onNewDiagramTriggered(int packageId) {
    if (!m_repo.IsOpen()) {
        QMessageBox::information(this, "Nuevo diagrama", "Abre o crea un proyecto antes de crear un diagrama.");
        return;
    }
    if (m_repo.GetModels().size() == 0) {
        m_repo.CreateModel("Model"); // a repository needs at least one package to hold diagrams
        m_treeWidget->populateTree();
    }

    NewDiagramDialog dlg(m_repo, packageId > 0 ? packageId : defaultPackageForNewDiagram(), this);
    if (dlg.exec() != QDialog::Accepted) return;
    createDiagram(dlg.packageId(), dlg.diagramName(), dlg.kindKey());
}

int MainWindow::createDiagram(int packageId, const QString& name, const QString& kindKey) {
    const auto* kind = ea::find_uml_diagram_kind(kindKey.toStdString());
    if (!kind) return 0;

    auto diagram = m_repo.CreateDiagram(packageId, name.trimmed().toStdString(), kind->type, kind->stereotype);
    if (!diagram) {
        QMessageBox::warning(this, "Nuevo diagrama",
                             QString("No se pudo crear el diagrama:\n%1").arg(QString::fromStdString(m_repo.GetLastError())));
        return 0;
    }
    m_activePackageId = packageId;
    revealNewDiagram(*diagram, *kind);
    scheduleTeamSyncPush();
    return diagram->GetDiagramID();
}

int MainWindow::refineUseCaseToSequence(int useCaseElementId) {
    if (!m_repo.IsOpen() || useCaseElementId <= 0) return 0;
    auto uc = m_repo.GetElementByID(useCaseElementId);
    if (!uc) return 0;

    const int pkgId = uc->GetPackageID() > 0 ? uc->GetPackageID() : defaultPackageForNewDiagram();
    const QString seqName = QString("Secuencia - %1").arg(QString::fromStdString(uc->GetName()));

    // Reuse existing linked sequence diagram if already created
    if (auto pkg = m_repo.GetPackageByID(pkgId)) {
        for (size_t i = 0; i < pkg->GetDiagrams().size(); ++i) {
            if (auto d = pkg->GetDiagrams().GetAt(static_cast<short>(i))) {
                if (d->GetType() == ea::DiagramType::Sequence &&
                    QString::fromStdString(d->GetName()).compare(seqName, Qt::CaseInsensitive) == 0) {
                    onDiagramDoubleClicked(d->GetDiagramID());
                    return d->GetDiagramID();
                }
            }
        }
    }

    const int diagId = createDiagram(pkgId, seqName, "sequence");
    if (diagId <= 0) return 0;
    auto* view = diagramViewForId(diagId);
    if (!view) return diagId;

    // Auto-place Actors linked to this Use Case + a System Controller lifeline so the designer can trace messages immediately
    int x = 110;
    bool placedAnyActor = false;
    for (size_t i = 0; i < uc->GetConnectors().size(); ++i) {
        auto c = uc->GetConnectors().GetAt(static_cast<short>(i));
        if (!c) continue;
        const int otherId = (c->GetClientID() == useCaseElementId) ? c->GetSupplierID() : c->GetClientID();
        auto other = m_repo.GetElementByID(otherId);
        if (other && other->GetType() == ea::ObjectType::Actor) {
            view->placeElement(otherId, QPointF(x, 120));
            x += 230;
            placedAnyActor = true;
        }
    }
    if (!placedAnyActor) {
        view->addElement(ea::ObjectType::Actor, "Actor", QPointF(x, 60));
        x += 230;
    }

    QString ctrlName = QString::fromStdString(uc->GetName()).simplified();
    ctrlName.remove(' ');
    if (ctrlName.isEmpty()) ctrlName = "Sistema";
    view->addElement(ea::ObjectType::Class, ctrlName + "Controller", QPointF(x, 60), "control");
    view->addElement(ea::ObjectType::Class, "Repositorio", QPointF(x + 230, 60), "entity");

    statusBar()->showMessage(QString("Diagrama de secuencia \"%1\" listo para refinar el Caso de Uso.").arg(seqName), 6000);
    scheduleTeamSyncPush();
    return diagId;
}

void MainWindow::onNewPackageRequested(int parentPackageId) {
    if (!m_repo.IsOpen()) return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, "Nuevo Sub-paquete", "Nombre del paquete:", QLineEdit::Normal, "NuevoPaquete", &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    if (auto pkg = m_repo.CreatePackage(parentPackageId, name.toStdString())) {
        m_activePackageId = pkg->GetPackageID();
        m_treeWidget->populateTree();
        scheduleTeamSyncPush();
        statusBar()->showMessage(QString("Paquete \"%1\" creado.").arg(name), 4000);
    } else {
        showWarning("Nuevo paquete", QString::fromStdString(m_repo.GetLastError()));
    }
}

void MainWindow::onRenamePackageRequested(int packageId) {
    auto pkg = m_repo.GetPackageByID(packageId);
    if (!pkg) return;
    bool ok = false;
    const QString cur = QString::fromStdString(pkg->GetName());
    const QString next = QInputDialog::getText(this, "Renombrar Paquete", "Nuevo nombre:", QLineEdit::Normal, cur, &ok).trimmed();
    if (!ok || next.isEmpty() || next == cur) return;
    pkg->SetName(next.toStdString());
    if (m_repo.SavePackage(*pkg)) {
        m_treeWidget->populateTree();
        scheduleTeamSyncPush();
    }
}

void MainWindow::onRenameDiagramRequested(int diagramId) {
    auto diag = m_repo.GetDiagramByID(diagramId);
    if (!diag) return;
    bool ok = false;
    const QString cur = QString::fromStdString(diag->GetName());
    const QString next = QInputDialog::getText(this, "Renombrar Diagrama", "Nuevo nombre:", QLineEdit::Normal, cur, &ok).trimmed();
    if (!ok || next.isEmpty() || next == cur) return;
    diag->SetName(next.toStdString());
    if (m_repo.SaveDiagram(*diag)) {
        m_treeWidget->populateTree();
        if (auto* view = diagramViewForId(diagramId)) {
            view->refreshDiagram(diag);
            const int idx = m_diagramTabs->indexOf(view);
            if (idx >= 0) m_diagramTabs->setTabText(idx, next);
        }
        scheduleTeamSyncPush();
    }
}

void MainWindow::onRenameElementRequested(int elementId) {
    auto elem = m_repo.GetElementByID(elementId);
    if (!elem) return;
    bool ok = false;
    const QString cur = QString::fromStdString(elem->GetName());
    const QString next = QInputDialog::getText(this, "Renombrar Elemento", "Nuevo nombre:", QLineEdit::Normal, cur, &ok).trimmed();
    if (!ok || next.isEmpty() || next == cur) return;
    applyPropertyEdit(elementId, QString("Renombrar a \"%1\"").arg(next), [&next](ea::Element& e) {
        e.SetName(next.toStdString());
    });
}

void MainWindow::onElementEditRequested(int elementId) {
    if (auto* view = currentDiagramView(); view && view->elementItem(elementId)) {
        view->editElement(elementId);
        return;
    }
    auto elem = m_repo.GetElementByID(elementId);
    if (!elem) return;
    EditElementDialog dlg(elem, m_repo, this);
    if (dlg.exec() == QDialog::Accepted && dlg.isModified()) {
        onElementSelected(elementId);
        onModelChanged();
    }
}

void MainWindow::onDeleteElementRequested(int elementId) {
    auto elem = m_repo.GetElementByID(elementId);
    if (!elem) return;
    const auto btn = QMessageBox::question(
        this, "Eliminar del Modelo",
        QString("¿Eliminar permanentemente \"%1\" del repositorio y de todos los diagramas?").arg(QString::fromStdString(elem->GetName())),
        QMessageBox::Yes | QMessageBox::No);
    if (btn != QMessageBox::Yes) return;

    if (m_repo.DeleteElement(elementId)) {
        m_treeWidget->populateTree();
        for (int i = 0; i < m_diagramTabs->count(); ++i) {
            if (auto* view = qobject_cast<DiagramView*>(m_diagramTabs->widget(i))) {
                if (auto updated = m_repo.GetDiagramByID(view->getDiagramId())) {
                    view->refreshDiagram(updated);
                }
            }
        }
        m_propsWidget->clear();
        scheduleTeamSyncPush();
        statusBar()->showMessage("Elemento eliminado del modelo.", 4000);
    }
}

void MainWindow::revealNewDiagram(const ea::Diagram& diagram, const ea::UmlDiagramKind& kind) {
    m_treeWidget->populateTree();
    m_treeWidget->selectDiagram(diagram.GetDiagramID());
    onDiagramDoubleClicked(diagram.GetDiagramID()); // opens the empty canvas in a new tab
    statusBar()->showMessage(QString("Diagrama de %1 \"%2\" creado.")
                                 .arg(QString::fromStdString(kind.label), QString::fromStdString(diagram.GetName())),
                             4000);
}

void MainWindow::showWarning(const QString& title, const QString& text) {
    // Non-blocking: the canvas stays responsive and the edit flow (or a test) is not suspended
    auto* box = new QMessageBox(QMessageBox::Warning, title, text, QMessageBox::Ok, this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->open();
}

ea::validation::ValidationReport MainWindow::validateModel() {
    auto report = ea::validation::validate_model(m_repo.GetModels(), [this](int id) { return m_repo.GetElementByID(id); });
    m_validationWidget->showReport(report);
    showDock(m_validationDock);
    statusBar()->showMessage(validation_summary(report), 8000);
    return report;
}

void MainWindow::onModelChanged() {
    // The repository keeps its package tree in sync, so redrawing the Model Explorer is enough
    m_treeWidget->populateTree();
    scheduleTeamSyncPush();
}

void MainWindow::onTabCloseRequested(int index) {
    auto view = qobject_cast<DiagramView*>(m_diagramTabs->widget(index));
    if (view) {
        m_diagramTabs->removeTab(index);
        delete view;
    }
}

void MainWindow::onToolActivated(const ea::PaletteTool& tool) {
    auto view = currentDiagramView();
    if (!view) {
        statusBar()->showMessage("Abre o crea un diagrama para usar la Toolbox.", 4000);
        return;
    }
    view->applyToolAtCentre(tool);
    if (!tool.isElement()) {
        statusBar()->showMessage(QString("%1: haz clic en el origen y luego en el destino (Esc cancela).")
                                     .arg(QString::fromStdString(tool.label)), 6000);
    }
}

QUndoStack* MainWindow::activeUndoStack(DiagramView* view) const {
    return view ? view->undoStack() : (m_modelEdits ? m_modelEdits->undoStack() : nullptr);
}

void MainWindow::applyPropertyEdit(int elementId, const QString& description, const PropertiesWidget::Mutation& mutation) {
    if (auto view = currentDiagramView()) view->modifyElement(elementId, description, mutation);
    else m_modelEdits->modifyElement(elementId, description, mutation);
}

void MainWindow::showPaletteFor(DiagramView* view) {
    // guiaSecuencia §2.2 step 5: the Toolbox shows what is valid for the active diagram
    const auto diagram = view ? view->getDiagram() : nullptr;
    m_toolboxWidget->setPalette(diagram ? ea::palette_for(diagram->GetType(), diagram->GetStereotype())
                                        : ea::palette_for(nullptr));
}

namespace {
constexpr const char* ThemeSeedSetting = "theme/seed";
} // namespace

QColor MainWindow::savedThemeSeed() {
    const QColor seed(QSettings().value(ThemeSeedSetting).toString());
    return seed.isValid() ? seed : ea::gui::DarkTheme::defaultSeed();
}

void MainWindow::chooseThemeSeed() {
    const QColor seed = QColorDialog::getColor(ea::gui::m3().primary, this, tr("Visual Style: accent seed (M3 dynamic color)"));
    if (!seed.isValid()) return;
    QSettings().setValue(ThemeSeedSetting, seed.name());
    ea::gui::DarkTheme::apply(seed);
    statusBar()->showMessage(tr("Theme seed %1 applied").arg(seed.name()), 4000);
}
