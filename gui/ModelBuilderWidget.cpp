#include "ModelBuilderWidget.hpp"
#include "DarkTheme.hpp"
#include "M3Icons.hpp"
#include "ea/editor/ModelPatternService.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QMessageBox>

namespace ea::gui {

ModelBuilderWidget::ModelBuilderWidget(ea::Repository& repo, QWidget* parent)
    : QWidget(parent), m_repo(repo) {
    setupUi();
    populatePerspectivesTree();
}

void ModelBuilderWidget::setupUi() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 4);
    mainLayout->setSpacing(4);

    // Barra superior: Parent, Combine with Parent, Customise on Import, Create Model, Help
    auto topBar = new QWidget(this);
    ea::gui::setThemedStyleSheet(topBar, "background-color: @surfaceLow; border: 1px solid @outlineVariant; border-radius: 4px; padding: 2px;");
    auto topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(8, 4, 8, 4);
    topLayout->setSpacing(8);

    auto lblParent = new QLabel("Parent:", topBar);
    ea::gui::setThemedStyleSheet(lblParent, "color: @onSurfaceVariant; font-size: 11px;");
    topLayout->addWidget(lblParent);

    m_parentEdit = new QLineEdit("Getting Started", topBar);
    m_parentEdit->setReadOnly(true);
    ea::gui::setThemedStyleSheet(m_parentEdit, "background-color: @surface; color: @onSurface; border: 1px solid @outlineVariant; border-radius: 4px; padding: 3px 6px; font-size: 11px; width: 140px;");
    topLayout->addWidget(m_parentEdit);

    auto btnFolder = new QPushButton(topBar);
    ea::gui::M3Icons::decorate(btnFolder, "folder_open");
    btnFolder->setFixedWidth(28);
    ea::gui::setThemedStyleSheet(btnFolder, "background-color: @surfaceHigh; border: 1px solid @outlineVariant; border-radius: 4px; padding: 2px;");
    topLayout->addWidget(btnFolder);

    m_combineCheck = new QCheckBox("Combine with Parent", topBar);
    ea::gui::setThemedStyleSheet(m_combineCheck, "color: @onSurfaceMuted; font-size: 11px;");
    topLayout->addWidget(m_combineCheck);

    m_customizeCheck = new QCheckBox("Customise on Import", topBar);
    ea::gui::setThemedStyleSheet(m_customizeCheck, "color: @onSurfaceMuted; font-size: 11px;");
    topLayout->addWidget(m_customizeCheck);

    topLayout->addStretch();

    m_createBtn = new QPushButton("Create Model", topBar);
    ea::gui::setThemedStyleSheet(m_createBtn, "background-color: @primaryStrong; color: white; font-weight: bold; border-radius: 4px; padding: 5px 16px; font-size: 11.5px;");
    connect(m_createBtn, &QPushButton::clicked, this, &ModelBuilderWidget::onCreateModelClicked);
    topLayout->addWidget(m_createBtn);

    m_helpBtn = new QPushButton("Help", topBar);
    ea::gui::setThemedStyleSheet(m_helpBtn, "background-color: @surfaceHigh; color: @onSurfaceVariant; border: 1px solid @outlineVariant; border-radius: 4px; padding: 5px 12px; font-size: 11.5px;");
    topLayout->addWidget(m_helpBtn);

    mainLayout->addWidget(topBar);

    // Splitter Central: Árbol de Patrones de Modelado vs Previsualización
    auto splitter = new QSplitter(Qt::Horizontal, this);

    auto leftWidget = new QWidget(splitter);
    auto leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(4);

    // Encabezado de Perspectivas: All Perspectives, menú y filtro
    auto persHeader = new QHBoxLayout();
    auto lblPerspectives = new QLabel("All Perspectives", leftWidget);
    ea::gui::setThemedStyleSheet(lblPerspectives, "color: @primary; font-weight: bold; font-size: 11.5px;");
    persHeader->addWidget(lblPerspectives);

    auto btnMenu = new QPushButton(leftWidget);
    ea::gui::M3Icons::decorate(btnMenu, "menu");
    btnMenu->setFixedSize(22, 22);
    ea::gui::setThemedStyleSheet(btnMenu, "background: transparent; color: @onSurfaceMuted; border: none; font-size: 13px;");
    persHeader->addWidget(btnMenu);

    m_filterEdit = new QLineEdit(leftWidget);
    m_filterEdit->setPlaceholderText("Filter");
    ea::gui::M3Icons::addSearchIcon(m_filterEdit);
    m_filterEdit->setClearButtonEnabled(true);
    ea::gui::setThemedStyleSheet(m_filterEdit, "background-color: @surfaceLow; color: @onSurface; border: 1px solid @outlineVariant; border-radius: 4px; padding: 2px 6px; font-size: 11px; max-width: 120px;");
    persHeader->addWidget(m_filterEdit);

    leftLayout->addLayout(persHeader);

    m_tree = new QTreeWidget(leftWidget);
    m_tree->setHeaderHidden(true);
    m_tree->setAnimated(true);
    m_tree->setRootIsDecorated(true);
    ea::gui::setThemedStyleSheet(m_tree,
        "QTreeWidget { background-color: @surface; color: @onSurfaceVariant; border: 1px solid @outlineVariant; border-radius: 4px; font-size: 11.5px; }"
        "QTreeWidget::item { padding: 3px 0px; }"
        "QTreeWidget::item:hover { background-color: @surfaceHigh; color: @onSurface; }"
        "QTreeWidget::item:selected { background-color: @primaryContainer; color: @onPrimaryContainer; }"
    );
    leftLayout->addWidget(m_tree);
    splitter->addWidget(leftWidget);

    // Panel Derecho: Vista previa / Descripción del diagrama
    m_previewBrowser = new QTextBrowser(splitter);
    ea::gui::setThemedStyleSheet(m_previewBrowser, "background-color: @surface; color: @onSurface; border: 1px solid @outlineVariant; border-radius: 4px; padding: 18px; font-size: 12px; font-family: 'Segoe UI', 'Inter', sans-serif;");
    splitter->addWidget(m_previewBrowser);

    splitter->setSizes({380, 620});
    mainLayout->addWidget(splitter, 1);

    // Pestañas inferiores: Model Builder | Diagram Builder
    m_bottomTabs = new QTabWidget(this);
    m_bottomTabs->addTab(new QWidget(), "Model Builder");
    m_bottomTabs->addTab(new QWidget(), "Diagram Builder");
    m_bottomTabs->setFixedHeight(26);
    ea::gui::setThemedStyleSheet(m_bottomTabs,
        "QTabWidget::pane { border: none; background: transparent; }"
        "QTabBar::tab { background-color: @surfaceLow; color: @onSurfaceMuted; padding: 4px 14px; font-size: 11px; border: 1px solid @outlineVariant; border-bottom: none; border-top-left-radius: 4px; border-top-right-radius: 4px; margin-right: 2px; }"
        "QTabBar::tab:selected { background-color: @surfaceHigh; color: @primary; font-weight: bold; }"
    );
    mainLayout->addWidget(m_bottomTabs);

    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &ModelBuilderWidget::onItemSelectionChanged);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &ModelBuilderWidget::onFilterChanged);
}

void ModelBuilderWidget::populatePerspectivesTree() {
    m_tree->clear();

    auto addGroup = [this](const QString& title, bool expand = false) -> QTreeWidgetItem* {
        auto grp = new QTreeWidgetItem(m_tree);
        grp->setText(0, title);
        grp->setIcon(0, ea::gui::M3Icons::icon("folder"));
        QFont f = grp->font(0);
        f.setBold(true);
        grp->setFont(0, f);
        grp->setForeground(0, QBrush(ea::gui::m3().primary));
        grp->setExpanded(expand);
        return grp;
    };

    auto addPattern = [](QTreeWidgetItem* parent, const QString& title) {
        auto item = new QTreeWidgetItem(parent);
        item->setText(0, title);
        item->setIcon(0, ea::gui::M3Icons::icon("schema"));
        item->setData(0, Qt::UserRole, title);
    };

    // 1. Use Case Diagrams (proto3.jpeg)
    auto gUC = addGroup("Use Case Diagrams", true);
    addPattern(gUC, "Starter Use Case Model");
    addPattern(gUC, "Basic Use Case Model");
    addPattern(gUC, "Basic Use Case Model with include");
    addPattern(gUC, "Basic Use Case Model with Extend");
    addPattern(gUC, "Basic Use Case Model with Generalization");
    addPattern(gUC, "Basic Use Case Model with Collaboration");
    addPattern(gUC, "Basic Use Case Model with Test Cases");
    addPattern(gUC, "Structured Use Case Model");
    addPattern(gUC, "Use Case Model with Activity Diagram");
    addPattern(gUC, "Use Case Model with State Machine Diagram");

    // 2. Class Diagrams (proto3.jpeg)
    auto gClass = addGroup("Class Diagrams", true);
    addPattern(gClass, "Starter Class Diagram");
    addPattern(gClass, "Basic Class Diagram with Multiplicities");
    addPattern(gClass, "Basic Class Diagram with Roles");
    addPattern(gClass, "Basic Class Diagram with Attributes");
    addPattern(gClass, "Basic Class Diagram with Operations");
    addPattern(gClass, "Basic Class Diagram with Receptions");
    addPattern(gClass, "Basic Class Diagram with Attributes and Operations");
    addPattern(gClass, "One Level Class Type Hierarchy");
    addPattern(gClass, "Two Level Class Type Hierarchy");
    addPattern(gClass, "Two Level Class Type Hierarchy with Attributes");
    addPattern(gClass, "One Level Class Composition Hierarchy");
    addPattern(gClass, "Two Level Class Composition Hierarchy");
    addPattern(gClass, "Two Level Class Composition Hierarchy with Attributes");
    addPattern(gClass, "Composition with Substitution");
    addPattern(gClass, "Domain Model");

    // 3. Package Diagrams
    auto gPkg = addGroup("Package Diagrams", false);
    addPattern(gPkg, "Starter Package Diagram");
    addPattern(gPkg, "One Level Package Hierarchy");
    addPattern(gPkg, "Nested Package Hierarchy");
    addPattern(gPkg, "Package Dependencies");
    addPattern(gPkg, "Package Imports");
    addPattern(gPkg, "Pro Miner");

    // 4. Object Diagrams
    auto gObj = addGroup("Object Diagrams", false);
    addPattern(gObj, "Starter Object Diagram");
    addPattern(gObj, "Basic Object Diagram with Value Specification");
    addPattern(gObj, "Objects as Instances of Classes");
    addPattern(gObj, "Complete Object Diagram");

    // 5. Composite Structure Diagrams
    auto gCompStruct = addGroup("Composite Structure Diagrams", false);
    addPattern(gCompStruct, "Starter Composite Structure Diagram");
    addPattern(gCompStruct, "Composite Structure Diagram with Collaboration");
    addPattern(gCompStruct, "Complete Composite Structure Diagram");

    // 6. Communication Diagrams
    auto gComm = addGroup("Communication Diagrams", false);
    addPattern(gComm, "Starter Communication Diagram");
    addPattern(gComm, "Communication Diagram with Business Objects");
    addPattern(gComm, "Communication Diagram with Three Components");

    // 7. Sequence Diagrams
    auto gSeq = addGroup("Sequence Diagrams", false);
    addPattern(gSeq, "Starter Sequence Diagram");
    addPattern(gSeq, "Basic Sequence Diagram with Reply Message");
    addPattern(gSeq, "Basic Sequence Diagram with Synchronous Message");
    addPattern(gSeq, "Basic Sequence Diagram with Asynchronous Message");
    addPattern(gSeq, "Basic Sequence Diagram with Loop Fragment");
    addPattern(gSeq, "Basic Sequence Diagram with Boundary Control Entity");
    addPattern(gSeq, "Sequence Component Instances");
    addPattern(gSeq, "Sequence with Lost Message");
    addPattern(gSeq, "Sequence with Object Creation and Destruction");
    addPattern(gSeq, "Sequence with Component Ports and Interfaces");

    // 8. State Machine Diagrams
    auto gState = addGroup("State Machine Diagrams", false);
    addPattern(gState, "Starter State Machine");
    addPattern(gState, "Basic State Machine with Triggers");
    addPattern(gState, "Basic State Machine with Triggers and Guards");
    addPattern(gState, "Basic State Machine with Triggers Guards and Effects");
    addPattern(gState, "Basic State Machine with Entry Actions");

    // 9. Activity Diagrams
    auto gAct = addGroup("Activity Diagrams", false);
    addPattern(gAct, "Starter Activity Diagram");
    addPattern(gAct, "Basic Activity Diagram with Decisions");
    addPattern(gAct, "Basic Activity Diagram with Partitions");
    addPattern(gAct, "Basic Activity Diagram with Fork and Join");
    addPattern(gAct, "Basic Activity Diagram with Flow Final");
    addPattern(gAct, "Basic Activity Diagram with Pins");
    addPattern(gAct, "Basic Activity Diagram with Central Buffer Node");
    addPattern(gAct, "Basic Activity Diagram with Exception Handler");
    addPattern(gAct, "Basic Activity Diagram with Interruptible Region");
    addPattern(gAct, "Complete Activity Diagram with Activities and Actions");

    // 10. Component Diagrams
    auto gComp = addGroup("Component Diagrams", false);
    addPattern(gComp, "Starter Component Diagram");
    addPattern(gComp, "One Level Component Type Hierarchy");
    addPattern(gComp, "Two Level Component Type Hierarchy");
    addPattern(gComp, "One Level Component Composition Hierarchy");
    addPattern(gComp, "Two Level Component Composition Hierarchy");
    addPattern(gComp, "Component Interfaces with XML Payload");
    addPattern(gComp, "Component Interfaces with JSON Payload");

    if (gUC->childCount() > 0) {
        m_tree->setCurrentItem(gUC->child(0));
    }
}

void ModelBuilderWidget::onItemSelectionChanged() {
    auto current = m_tree->currentItem();
    if (!current) return;
    QString title = current->data(0, Qt::UserRole).toString();
    if (title.isEmpty()) title = current->text(0);

    QString html = ea::gui::themed(
        "<h2 style='color: @primary; margin-top: 0;'>%1</h2>"
        "<p style='color: @onSurfaceVariant; font-size: 13px; line-height: 1.5;'>"
        "Patrón de modelado estándar para <b>%1</b>.</p>"
        "<hr style='border: 0; border-top: 1px solid @outlineVariant; margin: 14px 0;'>"
        "<h3 style='color: @primary; font-size: 14px;'>Elementos incluidos en el patrón:</h3>"
        "<ul style='color: @onSurfaceMuted; line-height: 1.6; font-size: 12px;'>"
        "<li><b>Paquete de Modelado</b> instanciado bajo <code>Getting Started</code></li>"
        "<li><b>Diagrama UML</b> configurado con el estándar OMG correspondiente</li>"
        "<li><b>Conectores y Restricciones</b> pre-configurados para diseño ágil</li>"
        "</ul>"
        "<p style='color: @onSurfaceDisabled; font-size: 11px; margin-top: 20px;'>"
        "Haz clic en <b>[Create Model]</b> para añadir este patrón arquitectónico a tu repositorio."
        "</p>"
    ).arg(title);

    m_previewBrowser->setHtml(html);
}

void ModelBuilderWidget::onCreateModelClicked() {
    auto current = m_tree->currentItem();
    QString title = current ? current->data(0, Qt::UserRole).toString() : "UML Model";
    if (title.isEmpty() && current) title = current->text(0);
    QString category = (current && current->parent()) ? current->parent()->text(0) : title;

    const auto res = ea::editor::ModelPatternService::instantiatePattern(
        m_repo, 0, category.toStdString(), title.toStdString());
    if (res.ok) {
        emit modelCreated(res.packageId, res.diagramId);
        QMessageBox::information(this, "Modelo Creado",
            QString("El patrón <b>%1</b> se creó en el repositorio (%2 elementos, %3 relaciones).")
                .arg(title).arg(res.elementsCreated).arg(res.connectorsCreated));
    } else {
        QMessageBox::warning(this, "No se pudo crear el patrón", QString::fromStdString(res.error));
    }
}

void ModelBuilderWidget::onFilterChanged(const QString& text) {
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto grp = m_tree->topLevelItem(i);
        bool anyChildMatch = false;
        for (int c = 0; c < grp->childCount(); ++c) {
            auto child = grp->child(c);
            bool match = child->text(0).contains(text, Qt::CaseInsensitive);
            child->setHidden(!match && !text.isEmpty());
            if (match) anyChildMatch = true;
        }
        grp->setHidden(!anyChildMatch && !text.isEmpty());
        if (anyChildMatch && !text.isEmpty()) {
            grp->setExpanded(true);
        }
    }
}

} // namespace ea::gui
