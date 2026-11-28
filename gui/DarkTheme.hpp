#pragma once

// Material Design 3 theme engine on the Nord palette with Gemini-style accents
// (diseño/guía de implementación M3 + Nord + Gemini, step 1):
//   * M3Spacing     - 4 px grid, indentations and shape (corner radius) tokens
//   * M3Typography  - the M3 type scale in logical pixels
//   * M3ColorScheme - tonal surfaces + dynamic color derived from a seed (Nord 8 by default)
//   * DarkTheme     - applies palette + QSS to the application; widgets style themselves with
//                     setThemedStyleSheet(), whose "@token" placeholders follow the current scheme.

#include <QColor>
#include <QFont>
#include <QString>

class QWidget;

namespace ea::gui {

// ============================================================================
// 1. Spacing, indentation and shape tokens (M3 4dp / 8dp grid)
// ============================================================================
struct M3Spacing {
    static constexpr int None = 0;
    static constexpr int ExtraSmall = 4;  // micro gaps, inner borders
    static constexpr int Small = 8;       // vertical input padding, gap between buttons
    static constexpr int Medium = 12;     // horizontal padding of lists and UML compartments
    static constexpr int Large = 16;      // panel margins (Properties, dialogs)
    static constexpr int ExtraLarge = 24; // section / card separation on the start page
    static constexpr int XxLarge = 32;    // hero header margins

    static constexpr int TreeIndentation = 20;     // per nesting level in the model tree
    static constexpr int TreeIconSize = 18;
    static constexpr int UmlCompartmentPadX = 12;  // left / right indentation inside UML nodes
    static constexpr int UmlCompartmentPadY = 8;   // top / bottom padding of a compartment
    static constexpr int UmlVisibilityGutter = 16; // column for +, -, #, ~
    static constexpr int UmlFeatureLineH = 20;     // line height of attributes / operations
    static constexpr int UmlHeaderHeight = 36;     // name only
    static constexpr int UmlHeaderHeightStereotyped = 44; // «stereotype» + name

    static constexpr qreal ShapeExtraSmall = 4.0;
    static constexpr qreal ShapeSmall = 8.0;
    static constexpr qreal ShapeMedium = 12.0;  // UML nodes, cards, floating panels
    static constexpr qreal ShapeLarge = 16.0;   // dialogs and main containers
    static constexpr qreal ShapeFullPill = 999.0; // buttons, chips, breadcrumbs
};

// ============================================================================
// 2. M3 type scale (pixel sizes, so the canvas and the SVG export agree across DPIs)
// ============================================================================
struct M3Typography {
    static QString sansFamily(); // Inter, Plus Jakarta Sans, ... Noto Sans, Cantarell
    static QString monoFamily(); // JetBrains Mono, Fira Code, ... system fixed font

    static QFont headlineMedium(); // 24/32 DemiBold  - start page titles
    static QFont titleLarge();     // 18/24 DemiBold  - dialog headers
    static QFont titleMedium();    // 14/20 DemiBold  - UML element names
    static QFont titleSmall();     // 13/18 Medium    - panel titles
    static QFont bodyMedium();     // 13/20 Regular   - trees, tables, notes
    static QFont bodySmallMono();  // 12/18 Mono      - UML attributes / operations, consoles
    static QFont labelLarge();     // 13/18 Medium    - buttons, active tabs
    static QFont labelMedium();    // 11/16 Medium    - «stereotypes», badges
    static QFont labelSmall();     // 10/14 DemiBold  - section headers (uppercase), multiplicities
};

// ============================================================================
// 3. Dynamic M3 color scheme on Nord (Polar Night surfaces, Frost / Aurora accents)
// ============================================================================
struct M3ColorScheme {
    // Tonal surfaces (Polar Night + tonal elevation)
    QColor surfaceLowest{"#1E222A"};    // infinite canvas
    QColor surface{"#2E3440"};          // nord0 - main window, base bars
    QColor surfaceLow{"#333A47"};       // side panels (model tree, toolbox, notes)
    QColor surfaceContainer{"#3B4252"}; // nord1 - UML node body, cards
    QColor surfaceHigh{"#434C5E"};      // nord2 - UML node header, ribbon, dialogs
    QColor surfaceHighest{"#4C566A"};   // nord3 - hover, active inputs

    // Dynamic accents derived from the seed (Frost and Aurora)
    QColor primary{"#88C0D0"};          // nord8 - focus, cursors, selection
    QColor primaryStrong{"#5E81AC"};    // nord10 - filled primary actions
    QColor onPrimary{"#1E222A"};
    QColor primaryContainer;            // selection background in trees, active pills (derived)
    QColor onPrimaryContainer{"#ECEFF4"};
    QColor secondary{"#81A1C1"};        // nord9 - active connectors, ports, secondary icons
    QColor tertiary{"#B48EAD"};         // nord15 - Gemini AI accent, stereotypes, interfaces
    QColor tertiaryContainer;           // derived

    // Text and outlines (Snow Storm)
    QColor onSurface{"#ECEFF4"};         // nord6 - primary text
    QColor onSurfaceVariant{"#D8DEE9"};  // nord4 - property labels, breadcrumbs
    QColor onSurfaceMuted{"#A3ABBA"};    // secondary text (nord4 towards nord3)
    QColor onSurfaceDisabled{"#7B88A1"}; // captions, hints, disabled text
    QColor outline{"#4C566A"};           // nord3 - node borders, active dividers
    QColor outlineVariant{"#3D4555"};    // compartment lines, grid, subtle dividers

    // Aurora semantics
    QColor error{"#BF616A"};   // nord11
    QColor warning{"#EBCB8B"}; // nord13
    QColor success{"#A3BE8C"}; // nord14
    QColor errorContainer;   // derived: surface tinted with error
    QColor successContainer; // derived: surface tinted with success

    M3ColorScheme(); // canonical Nord scheme (seed Nord 8)

    // Scheme whose accents and tonal surfaces follow `seed`; Nord 8 gives the canonical Nord scheme
    static M3ColorScheme fromSeed(const QColor& seed);

    // Color for an "@token" name (the field names above); invalid QColor when unknown
    QColor token(const QString& name) const;
};

inline QColor withAlpha(QColor color, int alpha) {
    color.setAlpha(alpha);
    return color;
}

// Diagonal Gemini gradient stops (primary -> secondary -> tertiary) for focus rings and selection
struct GeminiGradient {
    QColor start, middle, end;
};

class DarkTheme {
public:
    static QColor defaultSeed(); // Nord 8 (#88C0D0)

    // Fusion style + M3 palette + global QSS for `seed`; re-applies widget styles set through
    // setThemedStyleSheet() so the whole UI follows a new seed at runtime
    static void apply(const QColor& seed = defaultSeed());

    static const M3ColorScheme& scheme();
    static GeminiGradient geminiGradient();
    static QString styleSheet(); // global QSS for the current scheme
};

inline const M3ColorScheme& m3() { return DarkTheme::scheme(); }

// Replaces "@token" placeholders (M3ColorScheme field names) with the current scheme's colors
QString themed(const QString& qssTemplate);

// widget->setStyleSheet(themed(qssTemplate)), remembering the template for DarkTheme::apply()
void setThemedStyleSheet(QWidget* widget, const QString& qssTemplate);

// Compatibility with the previous Dark Studio theme
inline QString GetStudioDarkStyleSheet() { return DarkTheme::styleSheet(); }

} // namespace ea::gui
