#pragma once

#include <QWidget>
#include <optional>
#include <vector>
#include "ea/diagram/DiagramPalette.hpp"

class QLabel;
class QLineEdit;
class QMimeData;
class QTreeWidget;
class QTreeWidgetItem;

namespace ea::gui {

// Drag & drop payload of a palette tool (Toolbox → DiagramView)
inline constexpr const char* PaletteToolMimeType = "application/x-onlyarchitect-palette-tool";
QMimeData* encodePaletteTool(const ea::PaletteTool& tool);
std::optional<ea::PaletteTool> decodePaletteTool(const QMimeData* mime);

// Contextual toolbox: shows the palette of the active diagram (guiaSecuencia.md §2.2 step 5).
// Element tools can be clicked (create at the centre) or dragged onto the canvas;
// relationship tools are clicked, then the user picks source and target on the canvas.
class ToolboxWidget : public QWidget {
    Q_OBJECT
public:
    explicit ToolboxWidget(QWidget* parent = nullptr);

    void setPalette(const ea::Palette& palette);
    const ea::Palette& palette() const { return m_palette; }

signals:
    void toolActivated(const ea::PaletteTool& tool);

private:
    void setupUi();
    void populate();
    void onItemClicked(QTreeWidgetItem* item);
    void onSearchChanged(const QString& text);

    ea::Palette m_palette;
    QLabel* m_title{nullptr};
    QLineEdit* m_searchEdit{nullptr};
    QTreeWidget* m_tree{nullptr};
};

} // namespace ea::gui
