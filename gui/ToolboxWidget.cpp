#include "ToolboxWidget.hpp"
#include <QHeaderView>

namespace ea::gui {

ToolboxWidget::ToolboxWidget(QWidget* parent)
    : QWidget(parent) {
    setupUi();
    populateToolbox();
}

void ToolboxWidget::setupUi() {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("🔍 Search toolbox items...");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setStyleSheet(
        "QLineEdit { background-color: #1a1f2b; color: #f1f5f9; border: 1px solid #283042; border-radius: 4px; padding: 4px 8px; font-size: 11px; }"
        "QLineEdit:focus { border-color: #38bdf8; }"
    );
    layout->addWidget(m_searchEdit);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setAnimated(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setStyleSheet(
        "QTreeWidget { background-color: #171c26; color: #cbd5e1; border: 1px solid #283042; border-radius: 4px; font-size: 11.5px; }"
        "QTreeWidget::item { padding: 3px 0px; }"
        "QTreeWidget::item:hover { background-color: #242c3d; color: #ffffff; }"
        "QTreeWidget::item:selected { background-color: #007acc; color: #ffffff; }"
    );
    layout->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemClicked, this, &ToolboxWidget::onItemClicked);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ToolboxWidget::onSearchChanged);
}

void ToolboxWidget::populateToolbox() {
    m_tree->clear();

    auto addCategory = [this](const QString& title, bool expanded = true) -> QTreeWidgetItem* {
        auto cat = new QTreeWidgetItem(m_tree);
        cat->setText(0, title);
        QFont f = cat->font(0);
        f.setBold(true);
        cat->setFont(0, f);
        cat->setForeground(0, QBrush(QColor(147, 197, 253))); // #93c5fd
        cat->setExpanded(expanded);
        return cat;
    };

    auto addItem = [](QTreeWidgetItem* parent, const QString& label, const QString& iconEmoji, int itemType, int enumVal, const QString& st = "") {
        auto it = new QTreeWidgetItem(parent);
        it->setText(0, QString("%1  %2").arg(iconEmoji, label));
        it->setData(0, Qt::UserRole, itemType); // 1 = Element, 2 = Connector
        it->setData(0, Qt::UserRole + 1, enumVal);
        it->setData(0, Qt::UserRole + 2, st);
    };

    // 1. Component (Exacto a proto2.jpeg)
    auto catComp = addCategory("Component", true);
    addItem(catComp, "Packaging Component", "📦", 1, static_cast<int>(ea::ObjectType::Component), "packaging");
    addItem(catComp, "Component", "🧩", 1, static_cast<int>(ea::ObjectType::Component));
    addItem(catComp, "Class", "🟦", 1, static_cast<int>(ea::ObjectType::Class));
    addItem(catComp, "Interface", "🟢", 1, static_cast<int>(ea::ObjectType::Interface));
    addItem(catComp, "Object", "🔲", 1, static_cast<int>(ea::ObjectType::Class), "object");
    addItem(catComp, "Port", "⚪", 1, static_cast<int>(ea::ObjectType::Port));
    addItem(catComp, "Expose Interface", "🔌", 1, static_cast<int>(ea::ObjectType::Interface), "exposed");

    // 2. Component Relationships (Exacto a proto2.jpeg)
    auto catRels = addCategory("Component Relationships", true);
    addItem(catRels, "Assembly", "🔗", 2, static_cast<int>(ea::ConnectorType::Assembly));
    addItem(catRels, "Delegate", "⤵️", 2, static_cast<int>(ea::ConnectorType::Delegate));
    addItem(catRels, "Associate", "➡️", 2, static_cast<int>(ea::ConnectorType::Association));
    addItem(catRels, "Realize", "⤴️", 2, static_cast<int>(ea::ConnectorType::Realization));
    addItem(catRels, "Generalise", "🔼", 2, static_cast<int>(ea::ConnectorType::Generalization));

    // 3. UML Standard Profile
    auto catStd = addCategory("UML Standard Profile", false);
    addItem(catStd, "Build Component", "🏗️", 1, static_cast<int>(ea::ObjectType::Component), "build");
    addItem(catStd, "Entity", "📜", 1, static_cast<int>(ea::ObjectType::Class), "entity");
    addItem(catStd, "Implement", "⚙️", 2, static_cast<int>(ea::ConnectorType::Realization));
    addItem(catStd, "Realisation", "✨", 2, static_cast<int>(ea::ConnectorType::Realization));
    addItem(catStd, "Specification", "📋", 1, static_cast<int>(ea::ObjectType::Class), "specification");
    addItem(catStd, "Subsystem", "🏢", 1, static_cast<int>(ea::ObjectType::Package), "subsystem");

    // 4. Common Elements
    auto catCommon = addCategory("Common Elements", false);
    addItem(catCommon, "Note", "📝", 1, static_cast<int>(ea::ObjectType::Note));
    addItem(catCommon, "Text", "🔤", 1, static_cast<int>(ea::ObjectType::Text));
    addItem(catCommon, "Boundary", "🔲", 1, static_cast<int>(ea::ObjectType::Boundary));

    // 5. Common Relationships
    auto catCommRel = addCategory("Common Relationships", false);
    addItem(catCommRel, "Note Link", "┄", 2, static_cast<int>(ea::ConnectorType::NoteLink));
    addItem(catCommRel, "Dependency", "⤏", 2, static_cast<int>(ea::ConnectorType::Dependency));

    // 6. Otros grupos colapsables de proto2.jpeg
    addCategory("Documents", false);
    addCategory("Dynamic Viewpoints", false);
    addCategory("Charts", false);
    addCategory("Simulation", false);
}

void ToolboxWidget::onItemClicked(QTreeWidgetItem* item, int /*column*/) {
    if (!item) return;
    int itemType = item->data(0, Qt::UserRole).toInt();
    int enumVal = item->data(0, Qt::UserRole + 1).toInt();
    QString st = item->data(0, Qt::UserRole + 2).toString();

    if (itemType == 1) { // Element
        emit elementToolSelected(static_cast<ea::ObjectType>(enumVal), st);
    } else if (itemType == 2) { // Connector
        emit connectorToolSelected(static_cast<ea::ConnectorType>(enumVal), st);
    }
}

void ToolboxWidget::onSearchChanged(const QString& text) {
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto cat = m_tree->topLevelItem(i);
        bool anyChildMatch = false;
        for (int c = 0; c < cat->childCount(); ++c) {
            auto child = cat->child(c);
            bool match = child->text(0).contains(text, Qt::CaseInsensitive);
            child->setHidden(!match && !text.isEmpty());
            if (match) anyChildMatch = true;
        }
        cat->setHidden(!anyChildMatch && !text.isEmpty());
        if (anyChildMatch && !text.isEmpty()) {
            cat->setExpanded(true);
        }
    }
}

} // namespace ea::gui
