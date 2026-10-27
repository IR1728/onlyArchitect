#include "ModelTreeWidget.hpp"
#include <QHeaderView>
#include <QLabel>

ModelTreeWidget::ModelTreeWidget(ea::Repository& repo, QWidget* parent)
    : QWidget(parent), m_repo(repo) {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);

    // Barra superior de iconos del Browser (proto3.jpeg)
    auto topToolbar = new QWidget(this);
    topToolbar->setStyleSheet("background-color: #171c26; border-bottom: 1px solid #283042; padding: 2px;");
    auto tbLayout = new QHBoxLayout(topToolbar);
    tbLayout->setContentsMargins(4, 2, 4, 2);
    tbLayout->setSpacing(8);

    auto lblProject = new QLabel("📁 Project", topToolbar);
    lblProject->setStyleSheet("color: #38bdf8; font-size: 11px; font-weight: bold;");
    tbLayout->addWidget(lblProject);

    auto lblContext = new QLabel("Context", topToolbar);
    lblContext->setStyleSheet("color: #94a3b8; font-size: 11px;");
    tbLayout->addWidget(lblContext);

    auto lblDiag = new QLabel("Diagram", topToolbar);
    lblDiag->setStyleSheet("color: #94a3b8; font-size: 11px;");
    tbLayout->addWidget(lblDiag);

    auto lblRes = new QLabel("Resources", topToolbar);
    lblRes->setStyleSheet("color: #94a3b8; font-size: 11px;");
    tbLayout->addWidget(lblRes);

    tbLayout->addStretch();
    layout->addWidget(topToolbar);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("🔍 Filter model elements...");
    m_filterEdit->setClearButtonEnabled(true);
    m_filterEdit->setStyleSheet("background-color: #1a1f2b; color: #f1f5f9; border: 1px solid #283042; border-radius: 4px; padding: 3px 6px; font-size: 11px;");
    layout->addWidget(m_filterEdit);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setAnimated(true);
    m_tree->setStyleSheet(
        "QTreeWidget { background-color: #1a202d; color: #cbd5e1; border: 1px solid #283042; font-size: 11.5px; }"
        "QTreeWidget::item { padding: 2px 0px; }"
        "QTreeWidget::item:hover { background-color: #242c3d; color: #ffffff; }"
        "QTreeWidget::item:selected { background-color: #007acc; color: #ffffff; }"
    );
    layout->addWidget(m_tree, 1);

    // Pestañas inferiores: Browser | Inspector (proto3.jpeg)
    auto bottomTabs = new QTabWidget(this);
    bottomTabs->addTab(new QWidget(), "Browser");
    bottomTabs->addTab(new QWidget(), "Inspector");
    bottomTabs->setFixedHeight(26);
    bottomTabs->setStyleSheet(
        "QTabWidget::pane { border: none; background: transparent; }"
        "QTabBar::tab { background-color: #1a1f2b; color: #94a3b8; padding: 3px 12px; font-size: 11px; border: 1px solid #283042; border-bottom: none; border-top-left-radius: 3px; border-top-right-radius: 3px; margin-right: 2px; }"
        "QTabBar::tab:selected { background-color: #242c3d; color: #38bdf8; font-weight: bold; }"
    );
    layout->addWidget(bottomTabs);

    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &ModelTreeWidget::onItemDoubleClicked);
    connect(m_tree, &QTreeWidget::itemClicked, this, &ModelTreeWidget::onItemClicked);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &ModelTreeWidget::onFilterChanged);
}

void ModelTreeWidget::clear() {
    m_tree->clear();
}

void ModelTreeWidget::populateTree() {
    m_tree->clear();
    for (size_t i = 0; i < m_repo.GetModels().size(); ++i) {
        auto m = m_repo.GetModels().GetAt(static_cast<short>(i));
        if (!m) continue;

        auto rootItem = new QTreeWidgetItem(m_tree);
        rootItem->setText(0, QString("🌐 %1").arg(QString::fromStdString(m->GetName())));
        rootItem->setData(0, Qt::UserRole, static_cast<int>(TreeItemType::Model));
        rootItem->setData(0, Qt::UserRole + 1, m->GetPackageID());

        addPackageToTree(rootItem, *m);
        rootItem->setExpanded(true);
    }
}

void ModelTreeWidget::addPackageToTree(QTreeWidgetItem* parentItem, const ea::Package& pkg) {
    // 1. Add Diagrams
    for (size_t i = 0; i < pkg.GetDiagrams().size(); ++i) {
        auto d = pkg.GetDiagrams().GetAt(static_cast<short>(i));
        if (!d) continue;

        auto dItem = new QTreeWidgetItem(parentItem);
        dItem->setText(0, QString("📊 %1 (%2)").arg(QString::fromStdString(d->GetName()), QString::fromStdString(to_string(d->GetType()))));
        dItem->setData(0, Qt::UserRole, static_cast<int>(TreeItemType::Diagram));
        dItem->setData(0, Qt::UserRole + 1, d->GetDiagramID());
        dItem->setForeground(0, QBrush(QColor(0, 102, 204)));
    }

    // 2. Add Elements
    for (size_t i = 0; i < pkg.GetElements().size(); ++i) {
        auto elem = pkg.GetElements().GetAt(static_cast<short>(i));
        if (!elem) continue;

        auto eItem = new QTreeWidgetItem(parentItem);
        QString icon = "📦";
        if (elem->GetType() == ea::ObjectType::Class) icon = "🟦";
        else if (elem->GetType() == ea::ObjectType::Interface) icon = "🟢";
        else if (elem->GetType() == ea::ObjectType::Enumeration) icon = "🟧";
        else if (elem->GetType() == ea::ObjectType::UseCase) icon = "🔵";
        else if (elem->GetType() == ea::ObjectType::Component) icon = "🟪";
        else if (elem->GetType() == ea::ObjectType::Note) icon = "📝";

        eItem->setText(0, QString("%1 %2 : %3").arg(icon, QString::fromStdString(elem->GetName()), QString::fromStdString(to_string(elem->GetType()))));
        eItem->setData(0, Qt::UserRole, static_cast<int>(TreeItemType::Element));
        eItem->setData(0, Qt::UserRole + 1, elem->GetElementID());
    }

    // 3. Add Sub-packages
    for (size_t i = 0; i < pkg.GetPackages().size(); ++i) {
        auto sub = pkg.GetPackages().GetAt(static_cast<short>(i));
        if (!sub) continue;

        auto pItem = new QTreeWidgetItem(parentItem);
        pItem->setText(0, QString("📁 %1").arg(QString::fromStdString(sub->GetName())));
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
