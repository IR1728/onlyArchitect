#include "ModelTreeWidget.hpp"
#include "DarkTheme.hpp"
#include "M3Icons.hpp"
#include "UmlIcons.hpp"
#include "ElementDrag.hpp"
#include <unordered_set>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QTreeWidgetItemIterator>

namespace {

// Explorer tree whose element items can be dragged onto a diagram (guiaSecuencia: drag from explorer)
class ElementDragTree : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;

protected:
    QMimeData* mimeData(const QList<QTreeWidgetItem*>& items) const override {
        for (const auto* item : items) {
            if (item->data(0, Qt::UserRole).toInt() == static_cast<int>(TreeItemType::Element)) {
                return ea::gui::encodeElementDrag(item->data(0, Qt::UserRole + 1).toInt());
            }
        }
        return nullptr; // packages and diagrams are not dropped on a canvas
    }
};

} // namespace

ModelTreeWidget::ModelTreeWidget(ea::Repository& repo, QWidget* parent)
    : QWidget(parent), m_repo(repo) {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(ea::gui::M3Spacing::Small, ea::gui::M3Spacing::Small, ea::gui::M3Spacing::Small, ea::gui::M3Spacing::Small);
    layout->setSpacing(ea::gui::M3Spacing::Small);

    // Barra superior de iconos del Browser (proto3.jpeg)
    auto topToolbar = new QWidget(this);
    ea::gui::setThemedStyleSheet(topToolbar, "background-color: @surface; border-bottom: 1px solid @outlineVariant; padding: 2px;");
    auto tbLayout = new QHBoxLayout(topToolbar);
    tbLayout->setContentsMargins(4, 2, 4, 2);
    tbLayout->setSpacing(8);

    tbLayout->addWidget(ea::gui::M3Icons::label("folder", topToolbar, ea::gui::M3Icons::SmallSize, ea::gui::m3().primary));
    auto lblProject = new QLabel("Explorador", topToolbar);
    ea::gui::setThemedStyleSheet(lblProject, "color: @primary; font-size: 11.5px; font-weight: bold;");
    tbLayout->addWidget(lblProject);
    tbLayout->addStretch();
    layout->addWidget(topToolbar);
    setMinimumSize(160, 120);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("Filter model elements...");
    ea::gui::M3Icons::addSearchIcon(m_filterEdit);
    m_filterEdit->setClearButtonEnabled(true);
    ea::gui::setThemedStyleSheet(m_filterEdit, "background-color: @surfaceContainer; color: @onSurface; border: 1px solid @outline; border-radius: 16px; padding: 4px 12px; font-size: 12px;");
    layout->addWidget(m_filterEdit);

    m_tree = new ElementDragTree(this);
    m_tree->setDragEnabled(true);
    m_tree->setDragDropMode(QAbstractItemView::DragOnly); // elements can be dragged onto a diagram
    m_tree->setHeaderHidden(true);
    m_tree->setAnimated(true);
    m_tree->setIndentation(ea::gui::M3Spacing::TreeIndentation); // 20 px per nesting level
    m_tree->setUniformRowHeights(true);
    m_tree->setIconSize(QSize(ea::gui::M3Spacing::TreeIconSize, ea::gui::M3Spacing::TreeIconSize));
    ea::gui::setThemedStyleSheet(m_tree,
        "QTreeWidget { background-color: @surfaceLow; color: @onSurfaceVariant; border: 1px solid @outlineVariant; border-radius: 12px; padding: 4px; font-size: 13px; }"
        "QTreeWidget::item { padding: 4px 4px; margin: 2px 0px; border-radius: 8px; }" // 32 px rows with a pill indicator
        "QTreeWidget::item:hover { background-color: @surfaceContainer; color: @onSurface; }"
        "QTreeWidget::item:selected { background-color: @primaryContainer; color: @onPrimaryContainer; font-weight: 600; }"
    );
    layout->addWidget(m_tree, 1);

    // Pestañas inferiores: Browser | Inspector (proto3.jpeg)
    auto bottomTabs = new QTabWidget(this);
    bottomTabs->addTab(new QWidget(), "Browser");
    bottomTabs->addTab(new QWidget(), "Inspector");
    bottomTabs->setFixedHeight(26);
    ea::gui::setThemedStyleSheet(bottomTabs,
        "QTabWidget::pane { border: none; background: transparent; }"
        "QTabBar::tab { background-color: @surfaceLow; color: @onSurfaceMuted; padding: 3px 12px; font-size: 11px; border: 1px solid @outlineVariant; border-bottom: none; border-top-left-radius: 3px; border-top-right-radius: 3px; margin-right: 2px; }"
        "QTabBar::tab:selected { background-color: @surfaceHigh; color: @primary; font-weight: bold; }"
    );
    layout->addWidget(bottomTabs);

    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &ModelTreeWidget::onItemDoubleClicked);
    connect(m_tree, &QTreeWidget::itemClicked, this, &ModelTreeWidget::onItemClicked);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &ModelTreeWidget::onFilterChanged);

    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &ModelTreeWidget::onContextMenuRequested);
}

void ModelTreeWidget::clear() {
    m_tree->clear();
}

void ModelTreeWidget::populateTree() {
    m_tree->setUpdatesEnabled(false);
    m_tree->clear();
    for (size_t i = 0; i < m_repo.GetModels().size(); ++i) {
        auto m = m_repo.GetModels().GetAt(static_cast<short>(i));
        if (!m) continue;

        auto rootItem = new QTreeWidgetItem(m_tree);
        rootItem->setText(0, QString::fromStdString(m->GetName()));
        rootItem->setIcon(0, ea::gui::M3Icons::icon("public", ea::gui::m3().primary));
        rootItem->setData(0, Qt::UserRole, static_cast<int>(TreeItemType::Model));
        rootItem->setData(0, Qt::UserRole + 1, m->GetPackageID());

        addPackageToTree(rootItem, *m);
        rootItem->setExpanded(true);
    }
    m_tree->setUpdatesEnabled(true);
}

void ModelTreeWidget::addPackageToTree(QTreeWidgetItem* parentItem, const ea::Package& pkg) {
    // 1. Add Diagrams
    for (size_t i = 0; i < pkg.GetDiagrams().size(); ++i) {
        auto d = pkg.GetDiagrams().GetAt(static_cast<short>(i));
        if (!d) continue;

        auto dItem = new QTreeWidgetItem(parentItem);
        dItem->setText(0, QString("%1 (%2)").arg(QString::fromStdString(d->GetName()), QString::fromStdString(to_string(d->GetType()))));
        dItem->setIcon(0, ea::gui::M3Icons::icon("schema", ea::gui::m3().primary));
        dItem->setData(0, Qt::UserRole, static_cast<int>(TreeItemType::Diagram));
        dItem->setData(0, Qt::UserRole + 1, d->GetDiagramID());
        dItem->setForeground(0, QBrush(ea::gui::m3().primaryStrong));
    }

    // 2. Add Elements
    // EA keeps a t_object row for each package (same GUID): the package node already shows it
    std::unordered_set<std::string> childPackageGuids;
    for (const auto& child : pkg.GetPackages()) if (child) childPackageGuids.insert(child->GetPackageGUID());
    for (size_t i = 0; i < pkg.GetElements().size(); ++i) {
        auto elem = pkg.GetElements().GetAt(static_cast<short>(i));
        if (!elem) continue;
        if (elem->GetType() == ea::ObjectType::Package && childPackageGuids.count(elem->GetElementGUID())) continue;

        auto eItem = new QTreeWidgetItem(parentItem);
        eItem->setIcon(0, ea::gui::umlElementIcon(elem->GetType()));
        eItem->setText(0, QString("%1 : %2").arg(QString::fromStdString(elem->GetName()), QString::fromStdString(to_string(elem->GetType()))));
        eItem->setData(0, Qt::UserRole, static_cast<int>(TreeItemType::Element));
        eItem->setData(0, Qt::UserRole + 1, elem->GetElementID());
        eItem->setFlags(eItem->flags() | Qt::ItemIsDragEnabled);
    }

    // 3. Add Sub-packages
    for (size_t i = 0; i < pkg.GetPackages().size(); ++i) {
        auto sub = pkg.GetPackages().GetAt(static_cast<short>(i));
        if (!sub) continue;

        auto pItem = new QTreeWidgetItem(parentItem);
        pItem->setText(0, QString::fromStdString(sub->GetName()));
        pItem->setIcon(0, ea::gui::M3Icons::icon("folder", ea::gui::m3().secondary));
        pItem->setData(0, Qt::UserRole, static_cast<int>(TreeItemType::Package));
        pItem->setData(0, Qt::UserRole + 1, sub->GetPackageID());

        addPackageToTree(pItem, *sub);
    }
}

void ModelTreeWidget::onItemDoubleClicked(QTreeWidgetItem* item, int /*column*/) {
    if (!item) return;
    int type = item->data(0, Qt::UserRole).toInt();
    int id = item->data(0, Qt::UserRole + 1).toInt();

    if (type == static_cast<int>(TreeItemType::Diagram)) {
        emit diagramDoubleClicked(id);
    } else if (type == static_cast<int>(TreeItemType::Element)) {
        emit elementSelected(id);
        emit elementEditRequested(id);
    }
}

void ModelTreeWidget::onItemClicked(QTreeWidgetItem* item, int /*column*/) {
    if (!item) return;
    int type = item->data(0, Qt::UserRole).toInt();
    int id = item->data(0, Qt::UserRole + 1).toInt();

    if (type == static_cast<int>(TreeItemType::Element)) {
        emit elementSelected(id);
    } else if (type == static_cast<int>(TreeItemType::Package) || type == static_cast<int>(TreeItemType::Model)) {
        emit packageSelected(id);
    }
}

void ModelTreeWidget::onContextMenuRequested(const QPoint& pos) {
    auto* item = m_tree->itemAt(pos);
    if (!item) return;
    const int type = item->data(0, Qt::UserRole).toInt();
    const int id = item->data(0, Qt::UserRole + 1).toInt();
    QMenu menu(this);

    if (type == static_cast<int>(TreeItemType::Package) || type == static_cast<int>(TreeItemType::Model)) {
        auto* newDiagramAct = menu.addAction(ea::gui::M3Icons::icon("schema"), "Nuevo diagrama...");
        auto* newPkgAct = menu.addAction(ea::gui::M3Icons::icon("create_new_folder"), "Nuevo sub-paquete...");
        menu.addSeparator();
        auto* renamePkgAct = menu.addAction(ea::gui::M3Icons::icon("edit"), "Renombrar paquete...");
        auto* chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (chosen == newDiagramAct) {
            emit packageSelected(id);
            emit newDiagramRequested(id);
        } else if (chosen == newPkgAct) {
            emit packageSelected(id);
            emit newPackageRequested(id);
        } else if (chosen == renamePkgAct) {
            emit renamePackageRequested(id);
        }
        return;
    }

    if (type == static_cast<int>(TreeItemType::Diagram)) {
        auto* openAct = menu.addAction(ea::gui::M3Icons::icon("schema"), "Abrir diagrama");
        auto* renameAct = menu.addAction(ea::gui::M3Icons::icon("edit"), "Renombrar diagrama...");
        auto* chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (chosen == openAct) emit diagramDoubleClicked(id);
        else if (chosen == renameAct) emit renameDiagramRequested(id);
        return;
    }

    if (type == static_cast<int>(TreeItemType::Element)) {
        auto elem = m_repo.GetElementByID(id);
        auto* editAct = menu.addAction(ea::gui::M3Icons::icon("edit"), "Editar elemento...");
        auto* renameAct = menu.addAction(ea::gui::M3Icons::icon("drive_file_rename_outline"), "Renombrar...");
        QAction* refineAct = nullptr;
        if (elem && elem->GetType() == ea::ObjectType::UseCase) {
            refineAct = menu.addAction(ea::gui::M3Icons::icon("sync_alt"), "Refinar con Diagrama de Secuencia...");
        }
        menu.addSeparator();
        auto* delAct = menu.addAction(ea::gui::M3Icons::icon("delete"), "Eliminar del modelo...");
        auto* chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
        if (chosen == editAct) emit elementEditRequested(id);
        else if (chosen == renameAct) emit renameElementRequested(id);
        else if (refineAct && chosen == refineAct) emit refineUseCaseRequested(id);
        else if (chosen == delAct) emit deleteElementRequested(id);
    }
}

bool ModelTreeWidget::selectDiagram(int diagramId) {
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        auto* item = *it;
        if (item->data(0, Qt::UserRole).toInt() == static_cast<int>(TreeItemType::Diagram) &&
            item->data(0, Qt::UserRole + 1).toInt() == diagramId) {
            for (auto* p = item->parent(); p; p = p->parent()) p->setExpanded(true);
            m_tree->setCurrentItem(item);
            m_tree->scrollToItem(item);
            return true;
        }
        ++it;
    }
    return false;
}

void ModelTreeWidget::onFilterChanged(const QString& text) {
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        filterTreeItem(m_tree->topLevelItem(i), text);
    }
}

bool ModelTreeWidget::filterTreeItem(QTreeWidgetItem* item, const QString& filter) {
    if (!item) return false;
    bool match = item->text(0).contains(filter, Qt::CaseInsensitive);

    bool childMatch = false;
    for (int i = 0; i < item->childCount(); ++i) {
        if (filterTreeItem(item->child(i), filter)) {
            childMatch = true;
        }
    }

    bool visible = match || childMatch || filter.isEmpty();
    item->setHidden(!visible);
    if (visible && !filter.isEmpty()) {
        item->setExpanded(true);
    }
    return visible;
}
