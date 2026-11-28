#include "DesktopToolsWidgets.hpp"
#include "DarkTheme.hpp"
#include "M3Icons.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QMenu>
#include <QGroupBox>
#include <QToolButton>
#include "WorkspaceLayouts.hpp"

namespace ea::gui {

RelationshipMatrixWidget::RelationshipMatrixWidget(ea::Repository& repo, QWidget* parent)
    : QWidget(parent), m_repo(repo) {
    auto* layout = new QVBoxLayout(this);
    auto* topBar = new QHBoxLayout();

    m_srcPkgCombo = new QComboBox(this);
    m_dstPkgCombo = new QComboBox(this);
    m_srcTypeCombo = new QComboBox(this);
    m_srcTypeCombo->addItems({"<All>", "Class", "Interface", "Component", "UseCase", "Actor"});
    m_dstTypeCombo = new QComboBox(this);
    m_dstTypeCombo->addItems({"<All>", "Class", "Interface", "Component", "UseCase", "Actor"});
    // <All> x <All> on a large model (10k+ elements) builds an unusable grid
    m_srcTypeCombo->setCurrentText("Class");
    m_dstTypeCombo->setCurrentText("Class");
    m_connTypeCombo = new QComboBox(this);
    m_connTypeCombo->addItems({"<All>", "Association", "Generalization", "Realization", "Dependency", "Assembly"});
    m_dirCombo = new QComboBox(this);
    m_dirCombo->addItems({"Both", "Source -> Target", "Target -> Source"});
    m_overlayCombo = new QComboBox(this);
    m_overlayCombo->addItems({"<None>", "CRUD (Create/Read/Update/Delete)"});

    topBar->addWidget(new QLabel("Source Type:", this));
    topBar->addWidget(m_srcTypeCombo);
    topBar->addWidget(new QLabel("Target Type:", this));
    topBar->addWidget(m_dstTypeCombo);
    topBar->addWidget(new QLabel("Connector:", this));
    topBar->addWidget(m_connTypeCombo);
    topBar->addWidget(new QLabel("Direction:", this));
    topBar->addWidget(m_dirCombo);
    topBar->addWidget(new QLabel("Overlay:", this));
    topBar->addWidget(m_overlayCombo);

    auto* btnRefresh = new QPushButton("Refresh Matrix", this);
    ea::gui::M3Icons::decorate(btnRefresh, "sync");
    connect(btnRefresh, &QPushButton::clicked, this, &RelationshipMatrixWidget::refreshMatrix);
    topBar->addWidget(btnRefresh);
    topBar->addStretch();
    layout->addLayout(topBar);

    m_matrixTable = new QTableWidget(this);
    m_matrixTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_matrixTable, &QTableWidget::customContextMenuRequested, this, &RelationshipMatrixWidget::onCellContextMenu);
    layout->addWidget(m_matrixTable, 1);
}

void RelationshipMatrixWidget::refreshMatrix() {
    auto dir = ea::desktop::MatrixDirectionFilter::Both;
    if (m_dirCombo->currentIndex() == 1) dir = ea::desktop::MatrixDirectionFilter::SourceToTarget;
    else if (m_dirCombo->currentIndex() == 2) dir = ea::desktop::MatrixDirectionFilter::TargetToSource;

    m_lastResult = ea::desktop::DesktopAnalysisService::BuildRelationshipMatrix(
        0, 0,
        m_srcTypeCombo->currentText().toStdString(),
        m_dstTypeCombo->currentText().toStdString(),
        m_connTypeCombo->currentText().toStdString(),
        dir, m_overlays, m_repo);

    m_matrixTable->clear();
    m_matrixTable->setRowCount(static_cast<int>(m_lastResult.sourceElements.size()));
    m_matrixTable->setColumnCount(static_cast<int>(m_lastResult.targetElements.size()));

    QStringList rowHeaders, colHeaders;
    for (const auto& s : m_lastResult.sourceElements) rowHeaders << QString::fromStdString(s->GetName());
    for (const auto& t : m_lastResult.targetElements) colHeaders << QString::fromStdString(t->GetName());
    m_matrixTable->setVerticalHeaderLabels(rowHeaders);
    m_matrixTable->setHorizontalHeaderLabels(colHeaders);

    for (size_t r = 0; r < m_lastResult.cells.size(); ++r) {
        for (size_t c = 0; c < m_lastResult.cells[r].size(); ++c) {
            auto* item = new QTableWidgetItem(QString::fromStdString(m_lastResult.cells[r][c]));
            item->setTextAlignment(Qt::AlignCenter);
            m_matrixTable->setItem(static_cast<int>(r), static_cast<int>(c), item);
        }
    }
}

void RelationshipMatrixWidget::onCellContextMenu(const QPoint& pos) {
    auto* item = m_matrixTable->itemAt(pos);
    if (!item) return;
    int r = item->row();
    int c = item->column();
    if (r < 0 || c < 0 ||
        r >= static_cast<int>(m_lastResult.sourceElements.size()) ||
        c >= static_cast<int>(m_lastResult.targetElements.size())) return;

    int srcId = m_lastResult.sourceElements[r]->GetElementID();
    int dstId = m_lastResult.targetElements[c]->GetElementID();

    QMenu menu(this);
    auto* actCreate = menu.addAction(ea::gui::M3Icons::icon("link"), "Create New Relationship");
    auto* actOverlayC = menu.addAction("Overlay: C (Create)");
    auto* actOverlayR = menu.addAction("Overlay: R (Read)");
    auto* actOverlayU = menu.addAction("Overlay: U (Update)");
    auto* actOverlayD = menu.addAction("Overlay: D (Delete)");
    auto* actClear = menu.addAction(ea::gui::M3Icons::icon("cleaning_services"), "Clear Overlay");

    auto* chosen = menu.exec(m_matrixTable->viewport()->mapToGlobal(pos));
    if (chosen == actCreate && srcId != dstId) {
        auto conn = std::make_shared<ea::Connector>();
        conn->SetClientID(srcId);
        conn->SetSupplierID(dstId);
        conn->SetType(ea::ConnectorType::Dependency);
        m_repo.SaveConnector(*conn);
        m_lastResult.sourceElements[r]->GetConnectors().AddExisting(conn);
        refreshMatrix();
    } else if (chosen == actOverlayC) { m_overlays[{srcId, dstId}] = "C"; refreshMatrix(); }
    else if (chosen == actOverlayR) { m_overlays[{srcId, dstId}] = "R"; refreshMatrix(); }
    else if (chosen == actOverlayU) { m_overlays[{srcId, dstId}] = "U"; refreshMatrix(); }
    else if (chosen == actOverlayD) { m_overlays[{srcId, dstId}] = "D"; refreshMatrix(); }
    else if (chosen == actClear) { m_overlays.erase({srcId, dstId}); refreshMatrix(); }
}

GapAnalysisMatrixWidget::GapAnalysisMatrixWidget(ea::Repository& repo, QWidget* parent)
    : QWidget(parent), m_repo(repo) {
    auto* layout = new QVBoxLayout(this);
    m_gapTable = new QTableWidget(this);
    layout->addWidget(m_gapTable, 1);
}
void GapAnalysisMatrixWidget::refreshMatrix() {}

SpecificationManagerWidget::SpecificationManagerWidget(ea::Repository& repo, QWidget* parent)
    : QWidget(parent), m_repo(repo) {
    auto* layout = new QVBoxLayout(this);
    auto* topBar = new QHBoxLayout();
    m_filterBarEdit = new QLineEdit(this);
    m_filterBarEdit->setPlaceholderText("Filter specification rows...");
    ea::gui::M3Icons::addSearchIcon(m_filterBarEdit);
    m_chkLevelNumbering = new QCheckBox("Enable Level Numbering (1.0, 1.1)", this);
    m_chkLevelNumbering->setChecked(true);
    topBar->addWidget(m_filterBarEdit, 1);
    topBar->addWidget(m_chkLevelNumbering);
    layout->addLayout(topBar);

    m_specTable = new QTableWidget(this);
    m_specTable->setColumnCount(6);
    m_specTable->setHorizontalHeaderLabels({"Level", "Element", "Type", "Status", "Version", "Notes"});
    m_specTable->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(m_specTable, 1);

    connect(m_chkLevelNumbering, &QCheckBox::toggled, this, [this](bool checked) {
        m_levelNumbering = checked;
        refreshSpecification();
    });
}
void SpecificationManagerWidget::setPackage(int packageId) {
    m_packageId = packageId;
    refreshSpecification();
}
void SpecificationManagerWidget::refreshSpecification() {
    auto pkgs = ea::desktop::DesktopAnalysisService::CollectAllPackages(m_repo);
    if (pkgs.empty()) return;
    std::shared_ptr<ea::Package> target = pkgs.front();
    for (const auto& p : pkgs) {
        if (p && p->GetPackageID() == m_packageId) { target = p; break; }
    }
    if (!target) return;

    auto rows = ea::desktop::DesktopAnalysisService::BuildPackageSpecification(*target, m_levelNumbering);
    m_specTable->setRowCount(static_cast<int>(rows.size()));
    for (size_t i = 0; i < rows.size(); ++i) {
        int r = static_cast<int>(i);
        m_specTable->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(rows[i].levelNumber)));
        m_specTable->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(rows[i].name)));
        m_specTable->setItem(r, 2, new QTableWidgetItem(QString::fromStdString(rows[i].type)));
        m_specTable->setItem(r, 3, new QTableWidgetItem(QString::fromStdString(rows[i].status)));
        m_specTable->setItem(r, 4, new QTableWidgetItem(QString::fromStdString(rows[i].version)));
        m_specTable->setItem(r, 5, new QTableWidgetItem(QString::fromStdString(rows[i].notes)));
    }
}

ModelSearchWidget::ModelSearchWidget(ea::Repository& repo, QWidget* parent)
    : QWidget(parent), m_repo(repo) {
    auto* layout = new QVBoxLayout(this);
    auto* topBar = new QHBoxLayout();
    m_searchTypeCombo = new QComboBox(this);
    m_searchTypeCombo->addItems({
        "Simple",
        "Recently Modified Elements",
        "Recently Modified Diagrams",
        "Classes & Interfaces",
        "Components & Nodes",
        "Use Cases & Actors",
        "Unconnected Elements",
        "Elements with Notes"
    });
    m_searchTermEdit = new QLineEdit(this);
    m_searchTermEdit->setPlaceholderText("<Search Term>...");
    ea::gui::M3Icons::addSearchIcon(m_searchTermEdit);
    auto* btnRun = new QPushButton("▶ Run Search", this);
    topBar->addWidget(m_searchTypeCombo);
    topBar->addWidget(m_searchTermEdit, 1);
    topBar->addWidget(btnRun);
    layout->addLayout(topBar);

    m_resultsTable = new QTableWidget(this);
    m_resultsTable->setColumnCount(6);
    m_resultsTable->setHorizontalHeaderLabels({"Object", "Type", "Stereotype", "Status", "Author", "Package"});
    m_resultsTable->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(m_resultsTable, 1);

    connect(btnRun, &QPushButton::clicked, this, [this]() {
        runSearch(static_cast<ea::desktop::PredefinedSearchType>(m_searchTypeCombo->currentIndex()),
                  m_searchTermEdit->text());
    });
    connect(m_searchTermEdit, &QLineEdit::returnPressed, btnRun, &QPushButton::click);
    connect(m_resultsTable, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        auto* item = m_resultsTable->item(row, 0);
        if (!item) return;
        int id = item->data(Qt::UserRole).toInt();
        if (item->data(Qt::UserRole + 1).toBool()) emit diagramDoubleClicked(id);
        else emit elementDoubleClicked(id);
    });
}

void ModelSearchWidget::runSearch(ea::desktop::PredefinedSearchType type, const QString& term) {
    auto rows = ea::desktop::DesktopAnalysisService::RunModelSearch(type, term.toStdString(), m_repo);
    m_resultsTable->setRowCount(static_cast<int>(rows.size()));
    for (size_t i = 0; i < rows.size(); ++i) {
        int r = static_cast<int>(i);
        auto* nameItem = new QTableWidgetItem(QString::fromStdString(rows[i].name));
        nameItem->setData(Qt::UserRole, rows[i].id);
        nameItem->setData(Qt::UserRole + 1, rows[i].isDiagram);
        m_resultsTable->setItem(r, 0, nameItem);
        m_resultsTable->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(rows[i].objectType)));
        m_resultsTable->setItem(r, 2, new QTableWidgetItem(QString::fromStdString(rows[i].stereotype)));
        m_resultsTable->setItem(r, 3, new QTableWidgetItem(QString::fromStdString(rows[i].status)));
        m_resultsTable->setItem(r, 4, new QTableWidgetItem(QString::fromStdString(rows[i].author)));
        m_resultsTable->setItem(r, 5, new QTableWidgetItem(QString::fromStdString(rows[i].packageName)));
    }
}

FocusAndTraceabilityWidget::FocusAndTraceabilityWidget(ea::Repository& repo, QWidget* parent)
    : QWidget(parent), m_repo(repo) {
    auto* layout = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);
    m_traceTree = new QTreeWidget(this);
    m_traceTree->setHeaderLabels({"Element / Relationship", "Type"});
    m_relationshipsTable = new QTableWidget(this);
    m_relationshipsTable->setColumnCount(6);
    m_relationshipsTable->setHorizontalHeaderLabels({"Relationship", "Direction", "Source", "Target", "Target Type", "View"});
    m_summaryEdit = new QTextEdit(this);
    m_summaryEdit->setReadOnly(true);

    m_tabs->addTab(m_traceTree, "Traceability");
    m_tabs->addTab(m_relationshipsTable, "Relationships");
    m_tabs->addTab(m_summaryEdit, "Summary");
    layout->addWidget(m_tabs);
}

void FocusAndTraceabilityWidget::inspectElement(int elementId, const ea::Diagram* activeDiagram) {
    m_traceTree->clear();
    auto elem = m_repo.GetElementByID(elementId);
    if (!elem) return;

    auto traceRoot = ea::desktop::DesktopAnalysisService::BuildTraceabilityTree(elementId, m_repo, 2);
    auto* rootItem = new QTreeWidgetItem(m_traceTree);
    rootItem->setText(0, QString::fromStdString(traceRoot.elementName));
    rootItem->setText(1, QString::fromStdString(traceRoot.elementType));
    populateTraceSubtree(rootItem, traceRoot);
    rootItem->setExpanded(true);

    auto rels = ea::desktop::DesktopAnalysisService::BuildElementRelationships(*elem, activeDiagram, m_repo);
    m_relationshipsTable->setRowCount(static_cast<int>(rels.size()));
    for (size_t i = 0; i < rels.size(); ++i) {
        int r = static_cast<int>(i);
        m_relationshipsTable->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(rels[i].relationshipType)));
        m_relationshipsTable->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(rels[i].direction)));
        m_relationshipsTable->setItem(r, 2, new QTableWidgetItem(QString::fromStdString(rels[i].sourceName)));
        m_relationshipsTable->setItem(r, 3, new QTableWidgetItem(QString::fromStdString(rels[i].targetName)));
        m_relationshipsTable->setItem(r, 4, new QTableWidgetItem(QString::fromStdString(rels[i].targetType)));
        m_relationshipsTable->setItem(r, 5, new QTableWidgetItem(QString::fromStdString(rels[i].viewStatus)));
    }

    m_summaryEdit->setPlainText(QString::fromStdString(
        ea::desktop::DesktopAnalysisService::BuildSummaryText(elem.get())));
}

void FocusAndTraceabilityWidget::showSummary() {
    m_tabs->setCurrentWidget(m_summaryEdit);
}

void FocusAndTraceabilityWidget::recordRevisitDiagram(int, const QString&) {}

void FocusAndTraceabilityWidget::populateTraceSubtree(QTreeWidgetItem* parentItem, const ea::desktop::TraceNode& node) {
    for (const auto& child : node.children) {
        auto* item = new QTreeWidgetItem(parentItem);
        item->setText(0, QString("[%1] %2").arg(QString::fromStdString(child.relationLabel),
                                                QString::fromStdString(child.elementName)));
        item->setText(1, QString::fromStdString(child.elementType));
        populateTraceSubtree(item, child);
        item->setExpanded(true);
    }
}

PortalsBarWidget::PortalsBarWidget(QWidget* parent) : QWidget(parent) {}
void PortalsBarWidget::loadPortalItems(int) {}

namespace {

// Window Selectors catalog. An empty command marks a planned panel not yet implemented.
struct SelectorItem { QString label; QString command; };
struct SelectorGroup { QString title; std::vector<SelectorItem> items; };
struct SelectorPage { QString title; std::vector<SelectorGroup> groups; };

std::vector<SelectorPage> windowSelectorCatalog() {
    return {
        {"Design", {
            {"Explore", {{"Browse", "browser"}, {"Inspect", ""}, {"Focus", ""}, {"Trace", "traceability"},
                         {"Search", "search"}, {"Navigate", ""}, {"Summary", "summary"}, {"System Output", ""}}},
            {"Create", {{"Model Builder", "model_builder"}, {"New Package", ""}, {"New Diagram", "new_diagram"},
                        {"New Element", ""}, {"Properties", "properties"}, {"Notes", "notes"}, {"Document", ""}}},
            {"Tools", {{"Timelines", ""}, {"Specification Manager", "spec_manager"}, {"Database Builder", ""},
                       {"Schema Composer", ""}, {"DMN Editor", ""}, {"Decision Table", ""},
                       {"External Systems", ""}, {"Package Matrix", "relationship_matrix"}, {"Validate Model", "validate_model"}, {"Gap Analysis", ""}}},
            {"Diagram", {{"Toolbox", "toolbox"}, {"Layout Tools", ""}, {"Diagram Layers", ""},
                         {"Diagram Filters", ""}, {"Insert Related Elements", ""}}},
        }},
        {"Properties", {
            {"General", {{"Properties", "properties"}, {"Notes", "notes"}, {"Document", ""},
                         {"Tagged Values", ""}, {"Properties Dialog", ""}}},
            {"Element Features", {{"Features", ""}, {"Attributes", "attributes"}, {"Operations", "operations"},
                                  {"Receptions", ""}, {"Parts & Ports", ""}, {"Interaction Points", ""}}},
            {"Responsibilities", {{"Requirements", ""}, {"Constraints", ""}, {"Scenarios", ""}}},
            {"Element Browser", {{"Parents & Interfaces", ""}, {"Template Parameters", ""}}},
        }},
        {"Collaborate", {
            {"Discuss & Review Elements", {{"Journal", ""}, {"Discuss", ""}, {"Review", ""}}},
            {"Histories", {{"Journals", ""}, {"Discussions", ""}, {"Reviews", ""}, {"Manage Reviews", ""}}},
            {"Personal", {{"Chat", ""}, {"Mail", ""}, {"My Workbench", ""}, {"Conversations", ""}}},
            {"Model", {{"Home Diagram", ""}, {"Model Library", ""}, {"Calendar of Events", ""}, {"Glossary", ""}}},
        }},
        {"Execute", {
            {"Simulation", {{"Simulator", "simulator"}, {"Sim Events", ""}, {"Sim Break Points", ""},
                            {"Sim Variables", ""}, {"Sim Call Stack", ""}, {"Decision Management", ""},
                            {"BPSim", ""}, {"Modelica", ""}, {"Executable State Machines", ""}}},
            {"Source Code", {{"Script Library", ""}, {"Open Source File", ""}, {"Generate Code", "generate_code"},
                             {"Synchronize Code", ""}, {"Import VS Project", ""}, {"Import Eclipse", ""}}},
            {"Analyze", {{"Analyzer Scripts", ""}, {"Analyzer Services", ""}, {"Profiler", ""},
                         {"Recorder", ""}, {"Tester", ""}, {"XSLT Debugger", ""}, {"File Search", ""}}},
            {"Execute", {{"Debugger", ""}, {"Breakpoints", ""}, {"Call Stack", ""}, {"Local Variables", ""},
                         {"Watches", ""}, {"Modules", ""}, {"Memory Viewer", ""}, {"JavaScript Console", ""}}},
        }},
        {"Construct", {
            {"Resources", {{"Resource Allocation", ""}, {"Resource History", ""}, {"Project Kanban", ""},
                           {"Personal Kanban", ""}, {"Gantt View", ""}, {"Active Tasks", ""}}},
            {"Testing", {{"Test Overview", ""}, {"Test History", ""}, {"Unit", ""}, {"Integration", ""},
                         {"System", ""}, {"Acceptance", ""}, {"Scenario", ""}, {"Inspection", ""}}},
            {"Change", {{"Maintenance Overview", ""}, {"Features", ""}, {"Changes", ""}, {"Documents", ""},
                        {"Issues", ""}, {"Defects", ""}, {"Tasks", ""}}},
            {"Project", {{"Decisions", ""}, {"Events", ""}, {"Risks", ""}, {"Metrics", ""}, {"Effort", ""}}},
        }},
        {"Publish", {
            {"Report Generator", {{"Generate Documentation", ""}, {"Templates", ""}}},
            {"Custom and Dynamic Reports", {{"Create a Custom Document", ""}, {"Show Dynamic Preview", ""},
                                            {"Open a Document File", ""}, {"New Document File", ""}}},
            {"Model Interchange", {{"RAS", ""}, {"Project Transfer", ""}, {"Export Package to XMI", "xmi_export"},
                                   {"Import Package from XMI", "xmi_import"}, {"CSV Import/Export", ""}, {"Glossary", ""}}},
            {"Model Control", {{"Simple Package Control", ""}, {"Model Version Control", ""},
                               {"Package Version Control", ""}, {"Auditing", ""}}},
        }},
    };
}

SelectorPage workspaceSelectorPage(const QStringList& userLayouts) {
    SelectorPage page{"Workspace", {}};
    for (const QString set : {"Design and Construct", "Software & Simulation"}) {
        SelectorGroup group{set, {}};
        for (const auto& l : systemWorkspaceLayouts()) {
            if (l.set == set) group.items.push_back({l.name, l.implemented ? "workspace:" + l.name : QString()});
        }
        page.groups.push_back(group);
    }
    page.groups.push_back({"Workspace Tools", {{"Workspaces", "workspace_dialog"}, {"My Workspaces", ""},
                                               {"My Ribbon Sets", ""}, {"Save Workspace", "save_workspace"},
                                               {"Working Sets", ""}}});
    SelectorGroup user{"User Layouts", {}};
    for (const auto& name : userLayouts) user.items.push_back({name, "workspace:" + name});
    if (user.items.empty()) user.items.push_back({"<empty>", ""});
    page.groups.push_back(user);
    return page;
}

} // namespace

WindowSelectorDialog::WindowSelectorDialog(int initialTabIndex, const QStringList& userLayouts, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Window Selector");
    setMinimumWidth(960);
    // Windows not yet available in onlyArchitect stay listed but visibly dimmed
    ea::gui::setThemedStyleSheet(this, "QToolButton { color: @onSurface; text-align: left; }"
                  "QToolButton:disabled { color: @onSurfaceDisabled; }");
    auto* layout = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);
    layout->addWidget(tabs);

    auto pages = windowSelectorCatalog();
    pages.push_back(workspaceSelectorPage(userLayouts));
    for (size_t p = 0; p < pages.size(); ++p) {
        auto* page = new QWidget(tabs);
        auto* columns = new QHBoxLayout(page);
        for (const auto& group : pages[p].groups) {
            auto* box = new QGroupBox(group.title, page);
            auto* items = new QVBoxLayout(box);
            for (const auto& item : group.items) {
                auto* button = new QToolButton(box);
                button->setText(item.label);
                button->setAutoRaise(true);
                button->setEnabled(!item.command.isEmpty());
                if (item.command.isEmpty()) {
                    button->setToolTip("Todavía no disponible en onlyArchitect.");
                } else {
                    connect(button, &QToolButton::clicked, this, [this, cmd = item.command]() {
                        emit commandSelected(cmd);
                        accept();
                    });
                }
                items->addWidget(button);
            }
            items->addStretch();
            columns->addWidget(box, 0, Qt::AlignTop);
        }
        tabs->addTab(page, QString("%1 (Alt+%2)").arg(pages[p].title).arg(p + 1));
    }
    tabs->setCurrentIndex(qBound(0, initialTabIndex, tabs->count() - 1));
}

UserPreferencesDialog::UserPreferencesDialog(QWidget* parent) : QDialog(parent) {}

} // namespace ea::gui
