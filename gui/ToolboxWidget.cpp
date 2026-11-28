#include "ToolboxWidget.hpp"
#include "DarkTheme.hpp"
#include "M3Icons.hpp"
#include "UmlIcons.hpp"
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace ea::gui {

namespace {

constexpr int ToolGroupRole = Qt::UserRole;     // index of the group in the palette
constexpr int ToolIndexRole = Qt::UserRole + 1; // index of the tool in its group

// Same glyph as the project browser shows for the element / relationship kind
QIcon iconFor(const ea::PaletteTool& tool) {
    return tool.isElement() ? umlElementIcon(tool.element) : umlRelationshipIcon(tool.relationship);
}

// Tree whose items carry palette tools as drag payload
class PaletteTree : public QTreeWidget {
public:
    PaletteTree(const ea::Palette& palette, QWidget* parent) : QTreeWidget(parent), m_palette(palette) {}

protected:
    QMimeData* mimeData(const QList<QTreeWidgetItem*>& items) const override {
        if (items.isEmpty()) return nullptr;
        const auto* item = items.first();
        const auto group = item->data(0, ToolGroupRole);
        const auto index = item->data(0, ToolIndexRole);
        if (!group.isValid() || !index.isValid()) return nullptr;
        return encodePaletteTool(m_palette.groups.at(group.toUInt()).tools.at(index.toUInt()));
    }

private:
    const ea::Palette& m_palette; // owned by the ToolboxWidget
};

} // namespace

QMimeData* encodePaletteTool(const ea::PaletteTool& tool) {
    // kind;type;ntype;stereotype;label  (label last: it is the only free-text field besides stereotype)
    const int type = tool.isElement() ? static_cast<int>(tool.element) : static_cast<int>(tool.relationship);
    const QStringList fields = {tool.isElement() ? "E" : "R", QString::number(type), QString::number(tool.ntype),
                                QString::fromStdString(tool.stereotype), QString::fromStdString(tool.label)};
    auto* mime = new QMimeData();
    mime->setData(PaletteToolMimeType, fields.join(QChar('\x1f')).toUtf8());
    return mime;
}

std::optional<ea::PaletteTool> decodePaletteTool(const QMimeData* mime) {
    if (!mime || !mime->hasFormat(PaletteToolMimeType)) return std::nullopt;
    const QStringList fields = QString::fromUtf8(mime->data(PaletteToolMimeType)).split(QChar('\x1f'));
    if (fields.size() != 5) return std::nullopt;
    ea::PaletteTool tool;
    tool.kind = fields[0] == "E" ? ea::PaletteTool::Kind::Element : ea::PaletteTool::Kind::Relationship;
    if (tool.isElement()) tool.element = static_cast<ea::ObjectType>(fields[1].toInt());
    else tool.relationship = static_cast<ea::ConnectorType>(fields[1].toInt());
    tool.ntype = fields[2].toInt();
    tool.stereotype = fields[3].toStdString();
    tool.label = fields[4].toStdString();
    return tool;
}

ToolboxWidget::ToolboxWidget(QWidget* parent) : QWidget(parent), m_palette(ea::palette_for(nullptr)) {
    setupUi();
    populate();
}

void ToolboxWidget::setupUi() {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_title = new QLabel(this);
    ea::gui::setThemedStyleSheet(m_title, "color: @primary; font-weight: bold; font-size: 11px; padding: 2px;");
    layout->addWidget(m_title);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search toolbox items...");
    ea::gui::M3Icons::addSearchIcon(m_searchEdit);
    m_searchEdit->setClearButtonEnabled(true);
    ea::gui::setThemedStyleSheet(m_searchEdit,
        "QLineEdit { background-color: @surfaceLow; color: @onSurface; border: 1px solid @outlineVariant; border-radius: 4px; padding: 4px 8px; font-size: 11px; }"
        "QLineEdit:focus { border-color: @primary; }"
    );
    layout->addWidget(m_searchEdit);

    m_tree = new PaletteTree(m_palette, this);
    m_tree->setHeaderHidden(true);
    m_tree->setIconSize(QSize(M3Icons::SmallSize, M3Icons::SmallSize));
    m_tree->setAnimated(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setDragEnabled(true);
    m_tree->setDragDropMode(QAbstractItemView::DragOnly);
    ea::gui::setThemedStyleSheet(m_tree,
        "QTreeWidget { background-color: @surface; color: @onSurfaceVariant; border: 1px solid @outlineVariant; border-radius: 4px; font-size: 11.5px; }"
        "QTreeWidget::item { padding: 3px 0px; }"
        "QTreeWidget::item:hover { background-color: @surfaceHigh; color: @onSurface; }"
        "QTreeWidget::item:selected { background-color: @primaryContainer; color: @onPrimaryContainer; }"
    );
    layout->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item) { onItemClicked(item); });
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ToolboxWidget::onSearchChanged);
}

void ToolboxWidget::setPalette(const ea::Palette& palette) {
    if (palette.title == m_palette.title) return; // same diagram kind: keep the user's expanded/filter state
    m_palette = palette;
    populate();
}

void ToolboxWidget::populate() {
    m_title->setText(QString("Toolbox · %1").arg(QString::fromStdString(m_palette.title)));
    m_tree->clear();
    for (size_t g = 0; g < m_palette.groups.size(); ++g) {
        const auto& group = m_palette.groups[g];
        auto category = new QTreeWidgetItem(m_tree);
        category->setText(0, QString::fromStdString(group.title));
        category->setFlags(Qt::ItemIsEnabled);
        QFont f = category->font(0);
        f.setBold(true);
        category->setFont(0, f);
        category->setForeground(0, QBrush(ea::gui::m3().primary));
        for (size_t t = 0; t < group.tools.size(); ++t) {
            const auto& tool = group.tools[t];
            auto item = new QTreeWidgetItem(category);
            item->setText(0, QString::fromStdString(tool.label));
            item->setIcon(0, iconFor(tool));
            item->setData(0, ToolGroupRole, static_cast<uint>(g));
            item->setData(0, ToolIndexRole, static_cast<uint>(t));
            auto flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
            if (tool.isElement()) flags |= Qt::ItemIsDragEnabled;
            item->setFlags(flags);
            item->setToolTip(0, tool.isElement() ? "Clic: crear en el centro · Arrastrar: colocar en el lienzo"
                                                 : "Clic y luego elige origen y destino en el lienzo");
        }
        category->setExpanded(true);
    }
    onSearchChanged(m_searchEdit->text());
}

void ToolboxWidget::onItemClicked(QTreeWidgetItem* item) {
    if (!item) return;
    const auto group = item->data(0, ToolGroupRole);
    const auto index = item->data(0, ToolIndexRole);
    if (!group.isValid() || !index.isValid()) return; // a category header
    emit toolActivated(m_palette.groups.at(group.toUInt()).tools.at(index.toUInt()));
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
        if (anyChildMatch && !text.isEmpty()) cat->setExpanded(true);
    }
}

} // namespace ea::gui
