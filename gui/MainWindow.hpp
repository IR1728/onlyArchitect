#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QStackedWidget>
#include <QDockWidget>
#include <QLabel>
#include <QSlider>
#include <QToolButton>
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

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    enum WorkspaceIndex {
        WorkspaceStartPage = 0,
        WorkspaceDiagram = 1,
        WorkspaceModelBuilder = 2,
        WorkspaceCodeGen = 3,
        WorkspaceSimulation = 4,
        WorkspaceWhiteboard = 5
    };

    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    bool openFile(const QString& filePath);

public slots:
    void switchWorkspace(int index);

private slots:
    void onOpenTriggered();
    void onNewTriggered();
    void onSaveTriggered();
    void onExportSvgTriggered();
    void onExportPngTriggered();
    void onExportPlantUmlTriggered();
    void onExportJsonTriggered();
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
    void onAboutTriggered();

    void onDiagramDoubleClicked(int diagramId);
    void onElementSelected(int elementId);
    void onTabCloseRequested(int index);

    void onToolboxElementSelected(ea::ObjectType type, const QString& stereotype);
    void onToolboxConnectorSelected(ea::ConnectorType type, const QString& stereotype);

private:
    void createRibbon();
    void createDocks();
    void setupStatusBar();
    DiagramView* currentDiagramView() const;

    ea::Repository m_repo;

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

    // Barra de estado estilo proto2.jpeg
    QLabel* m_statusLabel{nullptr};
    QLabel* m_perspectivesStatusLabel{nullptr};
    QSlider* m_zoomSlider{nullptr};
    QLabel* m_capLockLabel{nullptr};
    QLabel* m_numLockLabel{nullptr};
    QLabel* m_scrlLockLabel{nullptr};
    QLabel* m_cloudLabel{nullptr};
};
