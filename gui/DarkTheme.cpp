#include "DarkTheme.hpp"

#include <QApplication>
#include <QFontDatabase>
#include <QPalette>
#include <QRegularExpression>
#include <QStyleFactory>
#include <QWidget>
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace ea::gui {

namespace {

// ---------------------------------------------------------------------------
// Typography helpers
// ---------------------------------------------------------------------------
QString firstInstalled(const QStringList& candidates, const QString& fallback) {
    const QStringList installed = QFontDatabase::families();
    for (const auto& family : candidates) {
        if (installed.contains(family, Qt::CaseInsensitive)) return family;
    }
    return fallback;
}

QFont makeFont(const QString& family, int pixelSize, QFont::Weight weight, qreal tracking = 0.0) {
    QFont font(family);
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    if (tracking != 0.0) font.setLetterSpacing(QFont::AbsoluteSpacing, tracking);
    return font;
}

// ---------------------------------------------------------------------------
// Color helpers (M3 tonal elevation: (1 - a) * surface + a * tint)
// ---------------------------------------------------------------------------
QColor blend(const QColor& base, const QColor& tint, float alpha) {
    auto mix = [alpha](int b, int t) { return static_cast<int>(std::lround(b * (1.0f - alpha) + t * alpha)); };
    return QColor(mix(base.red(), tint.red()), mix(base.green(), tint.green()), mix(base.blue(), tint.blue()));
}

float rotateHue(float hue, float turns) {
    float h = std::fmod(std::max(hue, 0.0f) + turns, 1.0f);
    return h < 0 ? h + 1.0f : h;
}

constexpr float ContainerTint = 0.28f; // primary container over the base surface
constexpr float AccentContainerTint = 0.25f;

void deriveContainers(M3ColorScheme& s) {
    s.primaryContainer = blend(s.surface, s.primary, ContainerTint);
    s.tertiaryContainer = blend(s.surface, s.tertiary, AccentContainerTint);
    s.errorContainer = blend(s.surface, s.error, AccentContainerTint);
    s.successContainer = blend(s.surface, s.success, AccentContainerTint);
}

using ColorField = QColor M3ColorScheme::*;
constexpr std::array<std::pair<const char*, ColorField>, 28> TokenTable{{
    {"surfaceLowest", &M3ColorScheme::surfaceLowest},
    {"surface", &M3ColorScheme::surface},
    {"surfaceLow", &M3ColorScheme::surfaceLow},
    {"surfaceContainer", &M3ColorScheme::surfaceContainer},
    {"surfaceHigh", &M3ColorScheme::surfaceHigh},
    {"surfaceHighest", &M3ColorScheme::surfaceHighest},
    {"primary", &M3ColorScheme::primary},
    {"primaryStrong", &M3ColorScheme::primaryStrong},
    {"onPrimary", &M3ColorScheme::onPrimary},
    {"primaryContainer", &M3ColorScheme::primaryContainer},
    {"onPrimaryContainer", &M3ColorScheme::onPrimaryContainer},
    {"secondary", &M3ColorScheme::secondary},
    {"tertiary", &M3ColorScheme::tertiary},
    {"tertiaryContainer", &M3ColorScheme::tertiaryContainer},
    {"onSurface", &M3ColorScheme::onSurface},
    {"onSurfaceVariant", &M3ColorScheme::onSurfaceVariant},
    {"onSurfaceMuted", &M3ColorScheme::onSurfaceMuted},
    {"onSurfaceDisabled", &M3ColorScheme::onSurfaceDisabled},
    {"outline", &M3ColorScheme::outline},
    {"outlineVariant", &M3ColorScheme::outlineVariant},
    {"error", &M3ColorScheme::error},
    {"warning", &M3ColorScheme::warning},
    {"success", &M3ColorScheme::success},
    {"errorContainer", &M3ColorScheme::errorContainer},
    {"successContainer", &M3ColorScheme::successContainer},
    // aliases used by the guide's token names
    {"surfaceContainerLow", &M3ColorScheme::surfaceLow},
    {"surfaceContainerHigh", &M3ColorScheme::surfaceHigh},
    {"surfaceContainerHighest", &M3ColorScheme::surfaceHighest},
}};

constexpr const char* QssTemplateProperty = "m3QssTemplate";

M3ColorScheme& currentScheme() {
    static M3ColorScheme scheme;
    return scheme;
}

void applyPalette(const M3ColorScheme& c) {
    QPalette p;
    p.setColor(QPalette::Window, c.surface);
    p.setColor(QPalette::WindowText, c.onSurface);
    p.setColor(QPalette::Base, c.surfaceLow);
    p.setColor(QPalette::AlternateBase, c.surfaceContainer);
    p.setColor(QPalette::ToolTipBase, c.surfaceHigh);
    p.setColor(QPalette::ToolTipText, c.onSurface);
    p.setColor(QPalette::PlaceholderText, c.onSurfaceDisabled);
    p.setColor(QPalette::Text, c.onSurface);
    p.setColor(QPalette::Button, c.surfaceContainer);
    p.setColor(QPalette::ButtonText, c.onSurface);
    p.setColor(QPalette::BrightText, c.primary);
    p.setColor(QPalette::Link, c.primary);
    p.setColor(QPalette::Highlight, c.primaryContainer);
    p.setColor(QPalette::HighlightedText, c.onPrimaryContainer);
    p.setColor(QPalette::Disabled, QPalette::Text, c.onSurfaceDisabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, c.onSurfaceDisabled);
    p.setColor(QPalette::Disabled, QPalette::WindowText, c.onSurfaceDisabled);
    qApp->setPalette(p);
}

void reapplyWidgetStyles() {
    for (QWidget* widget : QApplication::allWidgets()) {
        const QVariant tpl = widget->property(QssTemplateProperty);
        if (tpl.isValid()) widget->setStyleSheet(themed(tpl.toString()));
        widget->update();
    }
}

// Global QSS: Gemini-style pills, M3 tonal surfaces, 4 px grid paddings
const char* const GlobalQss = R"(
    /* ===== Base ===== */
    QMainWindow, QDialog, QWidget#CentralHost { background-color: @surface; color: @onSurface; }
    QWidget { color: @onSurface; selection-background-color: @primaryContainer; selection-color: @onPrimaryContainer; }
    QToolTip { background-color: @surfaceHigh; color: @onSurface; border: 1px solid @outline; border-radius: 8px; padding: 4px 8px; }

    /* ===== Workspace mode selector (pills) ===== */
    QTabBar#WorkspaceTabBar { background-color: @surfaceLowest; qproperty-drawBase: 0; }
    QTabBar#WorkspaceTabBar::tab {
        background-color: transparent; color: @onSurfaceMuted; font-weight: 600;
        padding: 8px 20px; margin: 4px 4px 4px 0px; border-radius: 16px; border: 1px solid transparent;
    }
    QTabBar#WorkspaceTabBar::tab:hover { background-color: @surfaceContainer; color: @onSurface; }
    QTabBar#WorkspaceTabBar::tab:selected { background-color: @primaryContainer; color: @onPrimaryContainer; border: 1px solid @primary; }

    /* ===== Standard tabs ===== */
    QTabWidget::pane { border: 1px solid @outlineVariant; background-color: @surfaceLow; border-radius: 12px; top: -1px; }
    QTabBar::tab {
        background-color: transparent; color: @onSurfaceMuted; padding: 8px 16px; margin-right: 4px;
        border-top-left-radius: 12px; border-top-right-radius: 12px; border: none; border-bottom: 2px solid transparent;
    }
    QTabBar::tab:hover { background-color: @surfaceContainer; color: @onSurface; }
    QTabBar::tab:selected { color: @primary; font-weight: 600; border-bottom: 2px solid @primary; background-color: @surfaceLow; }

    /* ===== Menu bar and menus ===== */
    QMenuBar { background-color: @surface; border-bottom: 1px solid @outlineVariant; padding: 4px 8px; font-weight: 500; }
    QMenuBar::item { background: transparent; padding: 4px 12px; border-radius: 12px; }
    QMenuBar::item:selected { background-color: @surfaceHigh; color: @primary; }
    QMenu { background-color: @surfaceContainer; border: 1px solid @outline; border-radius: 12px; padding: 4px; }
    QMenu::item { padding: 8px 24px 8px 12px; border-radius: 8px; margin: 2px 4px; }
    QMenu::item:selected { background-color: @primaryContainer; color: @onPrimaryContainer; }
    QMenu::item:disabled { color: @onSurfaceDisabled; }
    QMenu::separator { height: 1px; background-color: @outlineVariant; margin: 4px 12px; }

    /* ===== Toolbars ===== */
    QToolBar { background-color: @surface; border-bottom: 1px solid @outlineVariant; spacing: 4px; padding: 4px 8px; }
    QToolBar::separator { width: 1px; background-color: @outlineVariant; margin: 4px 8px; }
    QToolButton { background-color: transparent; color: @onSurfaceVariant; border: 1px solid transparent; border-radius: 16px; padding: 4px 12px; }
    QToolButton:hover { background-color: @surfaceHigh; color: @onSurface; }
    QToolButton:pressed, QToolButton:checked { background-color: @primaryContainer; color: @onPrimaryContainer; }

    /* ===== Dock widgets ===== */
    QDockWidget { color: @onSurface; font-weight: 600; titlebar-close-icon: url(); titlebar-normal-icon: url(); }
    QDockWidget::title {
        background-color: @surfaceLow; padding: 8px 16px; border-bottom: 1px solid @outlineVariant;
        border-top-left-radius: 12px; border-top-right-radius: 12px;
    }

    /* ===== Trees and lists (M3 indentation and pill selection) ===== */
    QTreeView, QListView, QTableView {
        background-color: @surfaceLow; color: @onSurface; border: 1px solid @outlineVariant; border-radius: 12px;
        gridline-color: @outlineVariant; outline: none; padding: 4px;
    }
    QTreeView::item, QListView::item { padding: 4px 8px; margin: 2px 0px; border-radius: 8px; }
    QTreeView::item:hover, QListView::item:hover { background-color: @surfaceContainer; }
    QTreeView::item:selected, QListView::item:selected { background-color: @primaryContainer; color: @onPrimaryContainer; }
    QHeaderView::section {
        background-color: @surfaceContainer; color: @onSurfaceMuted; padding: 4px 8px; border: none;
        border-right: 1px solid @outlineVariant; border-bottom: 1px solid @outlineVariant; font-weight: 600;
    }

    /* ===== Inputs (12 px horizontal / 8 px vertical padding) ===== */
    QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox, QDoubleSpinBox {
        background-color: @surfaceContainer; color: @onSurface; border: 1px solid @outline; border-radius: 8px;
        padding: 4px 12px; selection-background-color: @primaryContainer;
    }
    QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QComboBox:focus, QSpinBox:focus {
        border: 2px solid @primary; background-color: @surfaceHigh;
    }
    QLineEdit:disabled, QComboBox:disabled { color: @onSurfaceDisabled; }
    QComboBox::drop-down { border: none; width: 24px; }
    QComboBox QAbstractItemView { background-color: @surfaceContainer; border: 1px solid @outline; border-radius: 8px; selection-background-color: @primaryContainer; }

    /* ===== Gemini pill buttons ===== */
    QPushButton {
        background-color: @surfaceContainer; color: @onSurface; border: 1px solid @outline; border-radius: 16px;
        padding: 8px 20px; font-weight: 600;
    }
    QPushButton:hover { background-color: @surfaceHigh; border-color: @primary; }
    QPushButton:pressed { background-color: @primaryContainer; }
    QPushButton:disabled { color: @onSurfaceDisabled; border-color: @outlineVariant; }
    QPushButton[primary="true"], QPushButton#PrimaryBtn {
        background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 @primary, stop:0.5 @secondary, stop:1 @tertiary);
        color: @onPrimary; border: none;
    }
    QPushButton[primary="true"]:hover, QPushButton#PrimaryBtn:hover { border: 1px solid @onSurface; }

    QCheckBox, QRadioButton { spacing: 8px; }
    QGroupBox { border: 1px solid @outlineVariant; border-radius: 12px; margin-top: 16px; padding: 12px; }
    QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0px 4px; color: @onSurfaceMuted; }

    /* ===== Status bar ===== */
    QStatusBar { background-color: @surface; color: @onSurfaceMuted; border-top: 1px solid @outlineVariant; }

    /* ===== Minimal scrollbars ===== */
    QScrollBar:vertical { background: transparent; width: 8px; margin: 4px 2px; }
    QScrollBar::handle:vertical { background: @outline; border-radius: 4px; min-height: 24px; }
    QScrollBar:horizontal { background: transparent; height: 8px; margin: 2px 4px; }
    QScrollBar::handle:horizontal { background: @outline; border-radius: 4px; min-width: 24px; }
    QScrollBar::handle:hover { background: @primary; }
    QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
    QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

    /* ===== Progress and sliders ===== */
    QProgressBar { background-color: @surfaceContainer; border: none; border-radius: 8px; text-align: center; color: @onSurface; }
    QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 @primary, stop:1 @tertiary); border-radius: 8px; }
    QSlider::groove:horizontal { height: 4px; background: @outline; border-radius: 2px; }
    QSlider::sub-page:horizontal { background: @primary; border-radius: 2px; }
    QSlider::handle:horizontal { background: @primary; width: 16px; margin: -6px 0; border-radius: 8px; }
)";

} // namespace

// ============================================================================
// M3Typography
// ============================================================================
QString M3Typography::sansFamily() {
    static const QString family = firstInstalled(
        {"Inter", "Plus Jakarta Sans", "Google Sans", "Segoe UI Variable Display", "Segoe UI", "Noto Sans", "Cantarell"},
        QFontDatabase::systemFont(QFontDatabase::GeneralFont).family());
    return family;
}

QString M3Typography::monoFamily() {
    static const QString family = firstInstalled(
        {"JetBrains Mono", "Fira Code", "Cascadia Code", "Fira Mono", "Noto Sans Mono", "DejaVu Sans Mono", "Consolas"},
        QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
    return family;
}

QFont M3Typography::headlineMedium() { return makeFont(sansFamily(), 24, QFont::DemiBold, -0.2); }
QFont M3Typography::titleLarge() { return makeFont(sansFamily(), 18, QFont::DemiBold); }
QFont M3Typography::titleMedium() { return makeFont(sansFamily(), 14, QFont::DemiBold, 0.15); }
QFont M3Typography::titleSmall() { return makeFont(sansFamily(), 13, QFont::Medium, 0.1); }
QFont M3Typography::bodyMedium() { return makeFont(sansFamily(), 13, QFont::Normal, 0.25); }
QFont M3Typography::bodySmallMono() { return makeFont(monoFamily(), 12, QFont::Normal); }
QFont M3Typography::labelLarge() { return makeFont(sansFamily(), 13, QFont::Medium, 0.1); }
QFont M3Typography::labelMedium() { return makeFont(sansFamily(), 11, QFont::Medium, 0.5); }
QFont M3Typography::labelSmall() { return makeFont(sansFamily(), 10, QFont::DemiBold, 0.6); }

// ============================================================================
// M3ColorScheme
// ============================================================================
M3ColorScheme::M3ColorScheme() { deriveContainers(*this); }

M3ColorScheme M3ColorScheme::fromSeed(const QColor& seed) {
    M3ColorScheme s;
    if (!seed.isValid() || seed.rgb() == DarkTheme::defaultSeed().rgb()) return s;

    constexpr float SecondaryHueShift = 0.06f; // harmonious neighbours of the seed (Gemini gradient)
    constexpr float TertiaryHueShift = 0.22f;
    const QColor nord0("#2E3440"), nord1("#3B4252"), nord2("#434C5E"), nord3("#4C566A");

    s.primary = seed;
    s.surfaceLow = blend(nord0, seed, 0.05f);
    s.surfaceContainer = blend(nord1, seed, 0.07f);
    s.surfaceHigh = blend(nord2, seed, 0.09f);
    s.surfaceHighest = blend(nord3, seed, 0.12f);
    s.primaryStrong = seed.darker(140);
    s.secondary = QColor::fromHslF(rotateHue(seed.hslHueF(), SecondaryHueShift),
                                   std::min(1.0f, seed.hslSaturationF() * 0.85f), 0.65f);
    s.tertiary = QColor::fromHslF(rotateHue(seed.hslHueF(), TertiaryHueShift), 0.45f, 0.68f);
    deriveContainers(s);
    return s;
}

QColor M3ColorScheme::token(const QString& name) const {
    for (const auto& [key, field] : TokenTable) {
        if (name == QLatin1String(key)) return this->*field;
    }
    return {};
}

// ============================================================================
// DarkTheme
// ============================================================================
QColor DarkTheme::defaultSeed() { return QColor(0x88, 0xC0, 0xD0); }

const M3ColorScheme& DarkTheme::scheme() { return currentScheme(); }

GeminiGradient DarkTheme::geminiGradient() {
    const auto& c = scheme();
    return {c.primary, c.secondary, c.tertiary};
}

QString DarkTheme::styleSheet() { return themed(QString::fromUtf8(GlobalQss)); }

void DarkTheme::apply(const QColor& seed) {
    currentScheme() = M3ColorScheme::fromSeed(seed);
    qApp->setStyle(QStyleFactory::create("Fusion"));
    qApp->setFont(M3Typography::bodyMedium());
    applyPalette(currentScheme());
    qApp->setStyleSheet(styleSheet());
    reapplyWidgetStyles();
}

QString themed(const QString& qssTemplate) {
    static const QRegularExpression placeholder(QStringLiteral("@([A-Za-z]+)"));
    const auto& scheme = DarkTheme::scheme();
    QString result;
    result.reserve(qssTemplate.size());
    qsizetype last = 0;
    for (auto it = placeholder.globalMatch(qssTemplate); it.hasNext();) {
        const auto match = it.next();
        const QColor color = scheme.token(match.captured(1));
        result += QStringView(qssTemplate).mid(last, match.capturedStart() - last);
        result += color.isValid() ? color.name() : match.captured(0);
        last = match.capturedEnd();
    }
    result += QStringView(qssTemplate).mid(last);
    return result;
}

void setThemedStyleSheet(QWidget* widget, const QString& qssTemplate) {
    if (!widget) return;
    widget->setProperty(QssTemplateProperty, qssTemplate);
    widget->setStyleSheet(themed(qssTemplate));
}

} // namespace ea::gui
