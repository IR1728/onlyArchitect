#include "MainWindow.hpp"
#include "EapToQeaTransferDialog.hpp"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "ea/diagram/DiagramLayout.hpp"
#include "ea/export/Exporters.hpp"
#include "ea/repository/SqliteStorageEngine.hpp"
#include "ea/repository/EapxImporter.hpp"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("QtUML EA-Lite - 20 Day Trial");
    resize(1480, 940);

    createRibbon();

    // Contenedor principal con Stack de espacios de trabajo
    m_workspaceStack = new QStackedWidget(this);

    // 0. Start Page (Página de Inicio de proto2.jpeg)
    m_startPageWidget = new ea::gui::StartPageWidget(this);
    m_workspaceStack->addWidget(m_startPageWidget);

    // 1. Editor de Diagramas (con pestañas: Start Page | Components, etc.)
    m_diagramTabs = new QTabWidget(this);
    m_diagramTabs->setTabsClosable(true);
    m_diagramTabs->setMovable(true);
    m_diagramTabs->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #283042; background-color: #161c28; }"
        "QTabBar::tab { background-color: #1a1f2b; color: #94a3b8; padding: 6px 14px; font-size: 11.5px; border-top-left-radius: 4px; border-top-right-radius: 4px; margin-right: 2px; border: 1px solid #283042; border-bottom: none; }"
        "QTabBar::tab:selected { background-color: #222938; color: #38bdf8; font-weight: bold; border-top: 2px solid #38bdf8; }"
    );
    m_workspaceStack->addWidget(m_diagramTabs);

    // 2. Model Builder (Creador de Modelos de proto2.jpeg)
    m_modelBuilderWidget = new ea::gui::ModelBuilderWidget(m_repo, this);
    m_workspaceStack->addWidget(m_modelBuilderWidget);

    // 3. Generador de Código y Documentación
    m_codeGenWidget = new CodeGeneratorWidget(m_repo, this);
    m_workspaceStack->addWidget(m_codeGenWidget);

    // 4. Simulación y Depuración
    m_simWidget = new ea::SimulationWidget(m_repo, this);
    m_workspaceStack->addWidget(m_simWidget);

    // 5. Modo Boceto / Pizarra de Colaboración
    m_whiteboardWidget = new ea::WhiteboardWidget(m_repo, this);
    m_workspaceStack->addWidget(m_whiteboardWidget);

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

    createDocks();
    setupStatusBar();

    // Intentar abrir repositorio por defecto
    QString sampleDb = "/home/iran/EA/re/bin/EA_Trial/EAExample.qea";
    if (QFile::exists(sampleDb)) {
        if (openFile(sampleDb)) {
            onDiagramDoubleClicked(11);
            switchWorkspace(WorkspaceDiagram);
        }
    } else {
        switchWorkspace(WorkspaceStartPage);
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
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::showDesignClicked, this, [this]() {
        switchWorkspace(WorkspaceDiagram);
    });
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::showPropertiesClicked, this, [this]() {
        if (m_propertiesDock) m_propertiesDock->setVisible(true);
        if (m_notesDock) m_notesDock->setVisible(true);
    });
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::openFileClicked, this, &MainWindow::onOpenTriggered);
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::newFileClicked, this, &MainWindow::onNewTriggered);
    connect(m_ribbonBar, &ea::gui::RibbonBarWidget::saveFileClicked, this, &MainWindow::onSaveTriggered);

    connect(m_breadcrumbBar, &ea::gui::BreadcrumbBarWidget::searchPackage, this, [this](const QString& q) {
        if (m_treeWidget) m_treeWidget->findChild<QLineEdit*>()->setText(q);
    });
}

void MainWindow::createDocks() {
    // 1. Left Dock: Browser (Project Browser)
    m_browserDock = new QDockWidget("Browser", this);
    m_browserDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_treeWidget = new ModelTreeWidget(m_repo, this);
    m_browserDock->setWidget(m_treeWidget);
    addDockWidget(Qt::LeftDockWidgetArea, m_browserDock);

    connect(m_treeWidget, &ModelTreeWidget::diagramDoubleClicked, this, &MainWindow::onDiagramDoubleClicked);
    connect(m_treeWidget, &ModelTreeWidget::elementSelected, this, &MainWindow::onElementSelected);

    // 2. Left Dock 2: Toolbox (Caja de herramientas de proto2.jpeg)
    m_toolboxDock = new QDockWidget("Toolbox", this);
    m_toolboxDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_toolboxWidget = new ea::gui::ToolboxWidget(this);
    m_toolboxDock->setWidget(m_toolboxWidget);
    addDockWidget(Qt::LeftDockWidgetArea, m_toolboxDock);

    // Tabificar Browser y Toolbox juntos como en proto2.jpeg
    tabifyDockWidget(m_browserDock, m_toolboxDock);
    m_toolboxDock->raise();

    connect(m_toolboxWidget, &ea::gui::ToolboxWidget::elementToolSelected, this, &MainWindow::onToolboxElementSelected);
    connect(m_toolboxWidget, &ea::gui::ToolboxWidget::connectorToolSelected, this, &MainWindow::onToolboxConnectorSelected);

    // 3. Right Dock Top: Properties
    m_propertiesDock = new QDockWidget("Properties", this);
    m_propertiesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_propsWidget = new PropertiesWidget(m_repo, this);
    m_propertiesDock->setWidget(m_propsWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);

    // 4. Right Dock Bottom: Notes (con formato WYSIWYG)
    m_notesDock = new QDockWidget("Notes", this);
    m_notesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_notesWidget = new ea::gui::NotesWidget(m_repo, this);
    m_notesDock->setWidget(m_notesWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_notesDock);

    // 5. Bottom Dock: SQL Console
    m_bottomDock = new QDockWidget("SQL Console & Output", this);
    m_bottomDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    m_sqlWidget = new SqlConsoleWidget(m_repo, this);
    m_bottomDock->setWidget(m_sqlWidget);
    addDockWidget(Qt::BottomDockWidgetArea, m_bottomDock);
    m_bottomDock->hide(); // Oculto por defecto para coincidir con la vista limpia de proto2.jpeg
}

void MainWindow::setupStatusBar() {
    auto bar = statusBar();
    bar->setStyleSheet("QStatusBar { background-color: #141720; color: #94a3b8; font-size: 11px; border-top: 1px solid #283042; }");

    m_statusLabel = new QLabel("Component Diagram Components: created: 11/25/2016 16:01:05  modified: 13/15/2015 16:34:34  125% 7915 x 1134", this);
    bar->addWidget(m_statusLabel, 1);

    m_perspectivesStatusLabel = new QLabel("All Perspectives  ", this);
    m_perspectivesStatusLabel->setStyleSheet("color: #cbd5e1;");
    bar->addPermanentWidget(m_perspectivesStatusLabel);

    // Zoom slider
    auto zoomMinus = new QLabel("—", this);
    zoomMinus->setStyleSheet("color: #64748b; font-weight: bold;");
    bar->addPermanentWidget(zoomMinus);

    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setRange(20, 200);
    m_zoomSlider->setValue(100);
    m_zoomSlider->setFixedWidth(90);
    connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int val) {
        auto view = currentDiagramView();
        if (view) {
            double factor = val / 100.0;
            view->setTransform(QTransform::fromScale(factor, factor));
        }
    });
    bar->addPermanentWidget(m_zoomSlider);

    auto zoomPlus = new QLabel("+  ", this);
    zoomPlus->setStyleSheet("color: #64748b; font-weight: bold;");
    bar->addPermanentWidget(zoomPlus);

    // Badges de estado de teclado / nube como en proto2.jpeg
    m_capLockLabel = new QLabel("CAP  ", this);
    m_capLockLabel->setStyleSheet("color: #64748b; font-size: 10px;");
    bar->addPermanentWidget(m_capLockLabel);

    m_numLockLabel = new QLabel("NUM  ", this);
    m_numLockLabel->setStyleSheet("color: #38bdf8; font-size: 10px; font-weight: bold;");
    bar->addPermanentWidget(m_numLockLabel);

    m_scrlLockLabel = new QLabel("SCRL  ", this);
    m_scrlLockLabel->setStyleSheet("color: #64748b; font-size: 10px;");
    bar->addPermanentWidget(m_scrlLockLabel);

    m_cloudLabel = new QLabel("CLOUD", this);
    m_cloudLabel->setStyleSheet("color: #64748b; font-size: 10px;");
    bar->addPermanentWidget(m_cloudLabel);
}

void MainWindow::switchWorkspace(int index) {
    if (index < 0 || index >= m_workspaceStack->count()) return;
    m_workspaceStack->setCurrentIndex(index);

    if (index == WorkspaceStartPage) {
        m_startPageWidget->refreshRecentProjects();
    } else if (index == WorkspaceCodeGen) {
        m_codeGenWidget->refreshFromRepository();
    }
}

DiagramView* MainWindow::currentDiagramView() const {
    return qobject_cast<DiagramView*>(m_diagramTabs->currentWidget());
}

bool MainWindow::openFile(const QString& filePath) {
    if (!m_repo.OpenFile(filePath.toStdString())) {
        QMessageBox::critical(this, "Error", QString("No se pudo abrir el repositorio:\n%1").arg(QString::fromStdString(m_repo.GetLastError())));
        return false;
    }

    m_treeWidget->populateTree();
    m_diagramTabs->clear();
    m_propsWidget->clear();
    m_notesWidget->clear();

    auto counts = m_repo.GetCounts();
    m_statusLabel->setText(QString("Opened: %1 (%2 elements, %3 diagrams)")
                               .arg(filePath)
                               .arg(counts.elements)
                               .arg(counts.diagrams));

    if (counts.diagrams > 0) {
        for (size_t i = 0; i < m_repo.GetModels().size(); ++i) {
            auto m = m_repo.GetModels().GetAt(static_cast<short>(i));
            if (m && !m->GetDiagrams().empty()) {
                auto d = m->GetDiagrams().GetAt(0);
                if (d) {
                    onDiagramDoubleClicked(d->GetDiagramID());
                    break;
                }
            }
        }
    }

    m_codeGenWidget->refreshFromRepository();
    return true;
}

void MainWindow::onOpenTriggered() {
    QString path = QFileDialog::getOpenFileName(this, "Abrir Repositorio Enterprise Architect", "",
        "Archivos de Repositorio EA (*.qea *.feap *.eapx *.eap);;Repositorios SQLite (*.qea);;Proyectos Heredados (*.eapx *.eap);;Todos los Archivos (*)");
    if (!path.isEmpty()) {
        if (ea::EapxImporter::IsEapxFile(path.toStdString())) {
            QFileInfo fi(path);
            QString suggestedQea = fi.absolutePath() + "/" + fi.completeBaseName() + ".qea";
            auto btn = QMessageBox::question(this, "Proyecto EAPX Detectado",
                QString("El archivo <b>%1</b> es un proyecto en formato MS Access heredado (.eapx/.eap).<br><br>"
                        "¿Deseas migrarlo al formato moderno SQLite (.qea)?<br>"
                        "• <b>Sí</b>: Guardar como %2 y abrir.<br>"
                        "• <b>No</b>: Abrir mediante caché temporal.").arg(fi.fileName(), fi.completeBaseName() + ".qea"),
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
            if (btn == QMessageBox::Cancel) return;
            if (btn == QMessageBox::Yes) {
                std::string qea = ea::EapxImporter::ConvertToSqlite(path.toStdString(), suggestedQea.toStdString());
                if (!qea.empty()) {
                    if (openFile(QString::fromStdString(qea))) {
                        switchWorkspace(WorkspaceDiagram);
                    }
                    return;
                }
            }
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
        ea::SqliteStorageEngine engine;
        if (engine.CreateNewRepository(path.toStdString())) {
            openFile(path);
            switchWorkspace(WorkspaceDiagram);
            QMessageBox::information(this, "Éxito", "Repositorio creado correctamente con esquema compatible Enterprise Architect.");
        }
    }
}

void MainWindow::onSaveTriggered() {
    auto view = currentDiagramView();
    if (view && view->saveDiagramLayout()) {
        statusBar()->showMessage("Disposición del diagrama guardada.", 3000);
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

void MainWindow::onDiagramLayoutTriggered() {
    auto view = currentDiagramView();
    if (view && view->getDiagram()) {
        ea::DiagramLayout::Layout(*view->getDiagram(), m_repo, ea::LayoutAlgorithm::ForceDirected);
        view->reloadDiagram();
    }
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
    QMessageBox::about(this, "QtUML EA-Lite",
        "<h3>QtUML EA-Lite 17.2</h3>"
        "<p>Recreación nativa de Enterprise Architect basada en C++20 y Qt 6.</p>"
        "<p>Fiel al prototipo oficial (Ribbon, Start Page, Toolbox, Browser, Properties, Notes y Model Builder).</p>");
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
        QString icon = "📊";
        if (d->GetType() == ea::DiagramType::Component) icon = "🧩";
        else if (d->GetType() == ea::DiagramType::Sequence) icon = "🔄";
        else if (d->GetType() == ea::DiagramType::Activity) icon = "⚡";

        int tabIdx = m_diagramTabs->addTab(view, QString("%1 %2").arg(icon, QString::fromStdString(d->GetName())));
        m_diagramTabs->setCurrentIndex(tabIdx);

        connect(view, &DiagramView::elementSelected, this, &MainWindow::onElementSelected);
        m_propsWidget->showDiagram(diagramId);

        if (m_breadcrumbBar) {
            m_breadcrumbBar->setPath("Example Model", QString::fromStdString(d->GetName()));
        }

        switchWorkspace(WorkspaceDiagram);
    }
}

void MainWindow::onElementSelected(int elementId) {
    m_propsWidget->showElement(elementId);
    auto elem = m_repo.GetElementByID(elementId);
    if (elem) {
        m_notesWidget->setElement(elem);
    }
    m_codeGenWidget->generateForElement(elementId);
}

void MainWindow::onTabCloseRequested(int index) {
    auto view = qobject_cast<DiagramView*>(m_diagramTabs->widget(index));
    if (view) {
        m_diagramTabs->removeTab(index);
        delete view;
    }
}

void MainWindow::onToolboxElementSelected(ea::ObjectType type, const QString& stereotype) {
    auto view = currentDiagramView();
    if (view) {
        QString defaultName = "NewItem";
        if (type == ea::ObjectType::Component) defaultName = "Component";
        else if (type == ea::ObjectType::Class) defaultName = "Class";
        else if (type == ea::ObjectType::Interface) defaultName = "Interface";
        else if (type == ea::ObjectType::Port) defaultName = "Port";
        view->addNewElement(type, defaultName);
    }
}

void MainWindow::onToolboxConnectorSelected(ea::ConnectorType /*type*/, const QString& /*stereotype*/) {
    auto view = currentDiagramView();
    if (view) {
        view->startLinkMode();
    }
}
