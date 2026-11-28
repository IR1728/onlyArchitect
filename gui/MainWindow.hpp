#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QStackedWidget>
#include <QDockWidget>
#include <QLabel>
#include <QSlider>
#include <QToolButton>
#include <QUndoGroup>
#include "ea/repository/Repository.hpp"
#include "ModelTreeWidget.hpp"
#include "PropertiesWidget.hpp"
#include "SqlConsoleWidget.hpp"
#include "DiagramView.hpp"
#include "RibbonBarWidget.hpp"
#include "BreadcrumbBarWidget.hpp"
#include "StartPageWidget.hpp"
#include "ToolboxWidget.hpp"
#include "NotesWidget.hpp"
#include "ModelBuilderWidget.hpp"
#include "CodeGeneratorWidget.hpp"
#include "SimulationWidget.hpp"
#include "WhiteboardWidget.hpp"
#include "DesktopToolsWidgets.hpp"
#include "WorkspaceLayouts.hpp"
#include "ValidationResultsWidget.hpp"
#include "ModelEditSession.hpp"
#include "TeamSyncServer.hpp"
#include "TeamSyncClient.hpp"
#include "CollaborationWidgets.hpp"
#include "ea/diagram/UmlDiagramKinds.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    enum WorkspaceIndex {
        WorkspaceStartPage = 0,
        WorkspaceDiagram = 1,
        WorkspaceModelBuilder = 2,
        WorkspaceCodeGen = 3,
        WorkspaceSimulation = 4,
        WorkspaceWhiteboard = 5,
        WorkspacePlanner = 6
    };

    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    bool openFile(const QString& filePath);

    // M3 dynamic color: the seed chosen with Ribbon > Inicio > Visual Style (Nord 8 by default)
    static QColor savedThemeSeed();

    // Creates an empty diagram of a UML kind (see ea::uml_diagram_kinds()) in a package,
    // shows it in the Model Explorer and opens it in a new tab. Returns its ID, or 0 on failure.
    int createDiagram(int packageId, const QString& name, const QString& kindKey);
    // Refines a Use Case element into a linked Sequence Diagram pre-populated with its Actors + System Controller.
    int refineUseCaseToSequence(int useCaseElementId);
    // XMI interchange of a package (guiaSecuencia: export/import XMI). Results go to the status bar;
    // failures to a non-blocking warning. Import refreshes the Model Explorer.
    bool exportXmi(int packageId, const QString& path);
    bool importXmi(int packageId, const QString& path);

public slots:
    void switchWorkspace(int index);
    void openWindowSelector(int selector);
    void runWindowCommand(const QString& command);
    bool applyWorkspaceLayout(const QString& name);
    void saveCurrentLayoutAs(const QString& name);
    void openWorkspaceLayoutDialog();
    void chooseThemeSeed();

private slots:
    void onOpenTriggered();
    void onNewTriggered();
    void onSaveTriggered();
    void onExportSvgTriggered();
    void onExportPngTriggered();
    void onExportPlantUmlTriggered();
    void onExportJsonTriggered();
    void onExportXmiTriggered();
    void onImportXmiTriggered();
    void onDiagramLayoutTriggered();
    void onAddClassTriggered();
    void onAddInterfaceTriggered();
    void onAddNoteTriggered();
    void onLinkElementsTriggered();
    void onZoomInTriggered();
    void onZoomOutTriggered();
    void onResetZoomTriggered();
    void onFitInViewTriggered();
    void onTogglePanTriggered(bool checked);
    void onTransferEapToQeaTriggered();
    void onServerConnectionTriggered();
    void onAboutTriggered();

    void onDiagramDoubleClicked(int diagramId);
    void onElementSelected(int elementId);
    void onTabCloseRequested(int index);
    void onNewDiagramTriggered(int packageId = 0);
    void onNewPackageRequested(int parentPackageId);
    void onRenamePackageRequested(int packageId);
    void onRenameElementRequested(int elementId);
    void onDeleteElementRequested(int elementId);
    void onRenameDiagramRequested(int diagramId);
    void onElementEditRequested(int elementId);

public:
    // Runs the UML integrity check over the open model and shows the result (dock + status bar)
    ea::validation::ValidationReport validateModel();

private slots:

    void onToolActivated(const ea::PaletteTool& tool);
    void onModelChanged();

private:
    void createRibbon();
    void createDocks();
    void createAnalysisDocks();
    void connectRibbonCommands();
    void showDock(QDockWidget* dock);
    void setupWindowSelectorShortcuts();
    void populateWorkspaceMenu();
    void promptSaveWorkspace();
    void openSimulator();
    void setupStatusBar();
    void setupTeamSync();
    void updateTeamSyncBadge();
    void scheduleTeamSyncPush();
    void pushTeamSyncIfConnected();
    void reloadFromRemoteSync(const QString& localPath);
    DiagramView* diagramViewForId(int diagramId) const;
    void setupUndoRedo();
    DiagramView* currentDiagramView() const;
    int defaultPackageForNewDiagram() const;
    void showPaletteFor(DiagramView* view);
    void applyPropertyEdit(int elementId, const QString& description, const PropertiesWidget::Mutation& mutation);
    QUndoStack* activeUndoStack(DiagramView* view) const;
    void revealNewDiagram(const ea::Diagram& diagram, const ea::UmlDiagramKind& kind);
    void showWarning(const QString& title, const QString& text);

    ea::Repository m_repo;

    // Undo/Redo: one QUndoStack per open diagram; the group follows the active tab
    QUndoGroup* m_undoGroup{nullptr};
    ModelEditSession* m_modelEdits{nullptr}; // edits made with no diagram open (Property Editor)

    // Package last selected in the Model Explorer: where "Nuevo diagrama" puts the diagram
    int m_activePackageId{0};

    // Ribbon Bar & Breadcrumbs de proto3.jpeg
    ea::gui::RibbonBarWidget* m_ribbonBar{nullptr};
    ea::gui::BreadcrumbBarWidget* m_breadcrumbBar{nullptr};

    // Stack central de espacios de trabajo
    QStackedWidget* m_workspaceStack{nullptr};
    ea::gui::StartPageWidget* m_startPageWidget{nullptr};
    QTabWidget* m_diagramTabs{nullptr}; // Pestañas de diagramas (ej: Components Diagram)
    ea::gui::ModelBuilderWidget* m_modelBuilderWidget{nullptr};
    CodeGeneratorWidget* m_codeGenWidget{nullptr};
    ea::SimulationWidget* m_simWidget{nullptr};
    ea::WhiteboardWidget* m_whiteboardWidget{nullptr};
    ea::gui::ProjectPlannerWidget* m_plannerWidget{nullptr};

    // Docks del prototipo (Browser, Toolbox, Properties, Notes, SQL)
    ModelTreeWidget* m_treeWidget{nullptr};
    ea::gui::ToolboxWidget* m_toolboxWidget{nullptr};
    PropertiesWidget* m_propsWidget{nullptr};
    ea::gui::NotesWidget* m_notesWidget{nullptr};
    SqlConsoleWidget* m_sqlWidget{nullptr};

    QDockWidget* m_browserDock{nullptr};
    QDockWidget* m_toolboxDock{nullptr};
    QDockWidget* m_propertiesDock{nullptr};
    QDockWidget* m_notesDock{nullptr};
    QDockWidget* m_bottomDock{nullptr};

    // Ventanas de análisis (Find in Project, Traceability, Matrix, Specification)
    ea::gui::ModelSearchWidget* m_searchWidget{nullptr};
    ea::gui::FocusAndTraceabilityWidget* m_traceWidget{nullptr};
    ea::gui::RelationshipMatrixWidget* m_matrixWidget{nullptr};
    ea::gui::SpecificationManagerWidget* m_specWidget{nullptr};
    QDockWidget* m_searchDock{nullptr};
    QDockWidget* m_traceDock{nullptr};
    QDockWidget* m_matrixDock{nullptr};
    QDockWidget* m_specDock{nullptr};
    ValidationResultsWidget* m_validationWidget{nullptr};
    QDockWidget* m_validationDock{nullptr};
    ea::gui::TeamChatWidget* m_chatWidget{nullptr};
    QDockWidget* m_chatDock{nullptr};
    ea::gui::CollabHubWidget* m_collabWidget{nullptr};
    QDockWidget* m_collabDock{nullptr};

    // Workspace layouts: dock arrangement the system layouts start from
    ea::gui::WorkspaceLayoutStore m_layoutStore;
    QByteArray m_baseDockState;

    // Barra de estado estilo proto2.jpeg
    QLabel* m_statusLabel{nullptr};
    QLabel* m_perspectivesStatusLabel{nullptr};
    QSlider* m_zoomSlider{nullptr};
    QLabel* m_capLockLabel{nullptr};
    QLabel* m_numLockLabel{nullptr};
    QLabel* m_scrlLockLabel{nullptr};
    QToolButton* m_cloudButton{nullptr};

    // Colaboración LAN / VPN
    ea::gui::TeamSyncServer* m_syncServer{nullptr};
    ea::gui::TeamSyncClient* m_syncClient{nullptr};
    QTimer* m_syncPushTimer{nullptr};
    bool m_applyingRemoteSync{false};
};
