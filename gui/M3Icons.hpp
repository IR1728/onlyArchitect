#pragma once

// Material Symbols Rounded from google/material-design-icons. Glyphs come from the variable
// font in gui/resources/fonts (FILL axis: outlined at rest, filled when checked/selected); the
// 24 px SVGs in gui/resources/icons are the fallback when the font cannot be loaded. Icons are
// monochrome and tinted with the M3 scheme, so they follow the dynamic color seed.

#include <QColor>
#include <QIcon>
#include <QString>

class QAbstractButton;
class QLabel;
class QLineEdit;
class QWidget;

namespace ea::gui {

struct M3Icons {
    static constexpr int SmallSize = 18;  // inline with Body / Label text
    static constexpr int LargeSize = 24;  // ribbon large buttons

    // Icon for a Material Symbol name (e.g. "search"); normal state uses `color`
    // (onSurfaceVariant by default), active / selected states use the primary color.
    // A null icon when the symbol is not bundled.
    static QIcon icon(const QString& symbol, const QColor& color = QColor());

    static bool has(const QString& symbol);

    // True when the Material Symbols variable font and its codepoints table were found
    // ($ONLYARCHITECT_FONT_DIR, <exe dir>/fonts or <exe dir>/../share/onlyarchitect/fonts)
    static bool symbolFontLoaded();
    static QString symbolFontFamily();

    // Button with the symbol as its icon (text kept, if any)
    static void decorate(QAbstractButton* button, const QString& symbol, int size = SmallSize);
    // Label showing only the tinted symbol
    static QLabel* label(const QString& symbol, QWidget* parent, int size = SmallSize, const QColor& color = QColor());
    // Leading "search" glyph inside a search / filter field
    static void addSearchIcon(QLineEdit* edit);
};

} // namespace ea::gui
