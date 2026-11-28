#include "M3Icons.hpp"
#include "DarkTheme.hpp"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <QTextStream>

#include <algorithm>

static void ensureOnlyArchitectIconsResource() {
    static const bool initialized = []() {
        Q_INIT_RESOURCE(icons);
        return true;
    }();
    (void)initialized;
}

namespace ea::gui {

namespace {

constexpr qreal HighDpi = 2.0;

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
// Axis values of the Material Symbols variable font (M3 defaults: weight 400, grade 0)
constexpr float RegularWeight = 400.0f;
constexpr float NeutralGrade = 0.0f;
constexpr float OpticalSizeMin = 20.0f;
constexpr float OpticalSizeMax = 48.0f;
#endif

enum class Fill { Outlined, Filled };

// ---------------------------------------------------------------------------------------------
// Material Symbols Rounded variable font + its codepoints table (gui/resources/fonts)
// ---------------------------------------------------------------------------------------------
class SymbolFont {
public:
    static const SymbolFont& instance() {
        static const SymbolFont font;
        return font;
    }

    bool isLoaded() const { return !m_family.isEmpty() && !m_codepoints.isEmpty(); }
    const QString& family() const { return m_family; }
    QString directory() const { return m_directory; }

    char32_t codepoint(const QString& symbol) const { return m_codepoints.value(symbol, 0); }

private:
    static constexpr auto FontFile = "MaterialSymbolsRounded.ttf";
    static constexpr auto CodepointsFile = "MaterialSymbolsRounded.codepoints";

    SymbolFont() {
        ensureOnlyArchitectIconsResource();
        for (const QString& dir : candidateDirectories()) {
            if (load(dir)) return;
        }
    }

    static QStringList candidateDirectories() {
        QStringList dirs;
        if (const QString env = qEnvironmentVariable("ONLYARCHITECT_FONT_DIR"); !env.isEmpty()) dirs << env;
        if (QCoreApplication::instance()) {
            const QString app = QCoreApplication::applicationDirPath();
            dirs << app + QStringLiteral("/fonts")
                 << app + QStringLiteral("/../fonts")
                 << app + QStringLiteral("/../Resources/fonts")
                 << app + QStringLiteral("/../share/onlyarchitect/fonts");
        }
        return dirs;
    }

    bool load(const QString& dir) {
        const QDir base(dir);
        QHash<QString, char32_t> codepoints = readCodepoints(base.filePath(QString::fromLatin1(CodepointsFile)));
        if (codepoints.isEmpty()) return false;

        const int id = QFontDatabase::addApplicationFont(base.filePath(QString::fromLatin1(FontFile)));
        if (id < 0) return false;
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        if (families.isEmpty()) return false;

        m_family = families.first();
        m_codepoints = std::move(codepoints);
        m_directory = base.absolutePath();
        return true;
    }

    // One "name hex" pair per line, e.g. "search e8b6"
    static QHash<QString, char32_t> readCodepoints(const QString& path) {
        QHash<QString, char32_t> table;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return table;
        constexpr int ExpectedSymbols = 4400;
        table.reserve(ExpectedSymbols);
        QTextStream in(&file);
        QString name, hex;
        while (!in.atEnd()) {
            in >> name >> hex;
            bool ok = false;
            const uint value = hex.toUInt(&ok, 16);
            if (ok && !name.isEmpty()) table.insert(name, static_cast<char32_t>(value));
        }
        return table;
    }

    QString m_family;
    QString m_directory;
    QHash<QString, char32_t> m_codepoints;
};

QPixmap transparentPixmap(int size) {
    QPixmap pixmap(QSize(size, size) * HighDpi);
    pixmap.setDevicePixelRatio(HighDpi);
    pixmap.fill(Qt::transparent);
    return pixmap;
}

QFont symbolFont(int size, Fill fill) {
    QFont font(SymbolFont::instance().family());
    font.setPixelSize(size);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    font.setVariableAxis(QFont::Tag("FILL"), fill == Fill::Filled ? 1.0f : 0.0f);
    font.setVariableAxis(QFont::Tag("wght"), RegularWeight);
    font.setVariableAxis(QFont::Tag("GRAD"), NeutralGrade);
    font.setVariableAxis(QFont::Tag("opsz"), std::clamp(static_cast<float>(size), OpticalSizeMin, OpticalSizeMax));
#else
    (void)fill;
#endif
    return font;
}

QPixmap renderGlyph(char32_t codepoint, const QColor& color, int size, Fill fill) {
    QPixmap pixmap = transparentPixmap(size);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(symbolFont(size, fill));
    painter.setPen(color);
    painter.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, QString::fromUcs4(&codepoint, 1));
    return pixmap;
}

// Fallback when the font is not installed: the bundled 24 px SVGs (outlined only)
QString svgPath(const QString& symbol) { return QStringLiteral(":/icons/m3/%1.svg").arg(symbol); }

QPixmap renderSvg(const QString& symbol, const QColor& color, int size) {
    QFile file(svgPath(symbol));
    if (!file.open(QIODevice::ReadOnly)) return {};
    QByteArray svg = file.readAll();
    // Material Symbols carry no fill (black by default): give the root element the tint
    svg.replace("<svg ", "<svg fill=\"" + color.name().toLatin1() + "\" ");

    QPixmap pixmap = transparentPixmap(size);
    QSvgRenderer renderer(svg);
    QPainter painter(&pixmap);
    renderer.render(&painter, QRectF(0, 0, size, size));
    return pixmap;
}

QPixmap renderSymbol(const QString& symbol, const QColor& color, int size, Fill fill) {
    const SymbolFont& font = SymbolFont::instance();
    if (font.isLoaded()) {
        if (const char32_t cp = font.codepoint(symbol)) return renderGlyph(cp, color, size, fill);
    }
    return renderSvg(symbol, color, size);
}

void addState(QIcon& icon, const QString& symbol, const QColor& color, Fill fill, QIcon::Mode mode, QIcon::State state) {
    for (int size : {M3Icons::SmallSize, M3Icons::LargeSize}) {
        icon.addPixmap(renderSymbol(symbol, color, size, fill), mode, state);
    }
}

} // namespace

bool M3Icons::has(const QString& symbol) {
    const SymbolFont& font = SymbolFont::instance();
    return (font.isLoaded() && font.codepoint(symbol) != 0) || QFile::exists(svgPath(symbol));
}

bool M3Icons::symbolFontLoaded() { return SymbolFont::instance().isLoaded(); }

QString M3Icons::symbolFontFamily() { return SymbolFont::instance().family(); }

QIcon M3Icons::icon(const QString& symbol, const QColor& color) {
    const auto& scheme = m3();
    const QColor normal = color.isValid() ? color : scheme.onSurfaceVariant;
    const QString key = symbol + normal.name() + scheme.primary.name();

    static QHash<QString, QIcon> cache;
    if (auto it = cache.constFind(key); it != cache.constEnd()) return *it;
    if (!has(symbol)) return {};

    // M3: outlined at rest, filled once checked / selected (the FILL axis of the variable font)
    QIcon icon;
    addState(icon, symbol, normal, Fill::Outlined, QIcon::Normal, QIcon::Off);
    addState(icon, symbol, scheme.primary, Fill::Outlined, QIcon::Active, QIcon::Off);
    addState(icon, symbol, scheme.primary, Fill::Filled, QIcon::Normal, QIcon::On);
    addState(icon, symbol, scheme.primary, Fill::Filled, QIcon::Active, QIcon::On);
    addState(icon, symbol, scheme.primary, Fill::Filled, QIcon::Selected, QIcon::Off);
    addState(icon, symbol, scheme.onSurfaceDisabled, Fill::Outlined, QIcon::Disabled, QIcon::Off);
    cache.insert(key, icon);
    return icon;
}

void M3Icons::decorate(QAbstractButton* button, const QString& symbol, int size) {
    if (!button) return;
    button->setIcon(icon(symbol));
    button->setIconSize(QSize(size, size));
}

QLabel* M3Icons::label(const QString& symbol, QWidget* parent, int size, const QColor& color) {
    auto* l = new QLabel(parent);
    l->setPixmap(icon(symbol, color).pixmap(size, size));
    l->setFixedSize(size, size);
    return l;
}

void M3Icons::addSearchIcon(QLineEdit* edit) {
    if (edit) edit->addAction(icon(QStringLiteral("search"), m3().onSurfaceMuted), QLineEdit::LeadingPosition);
}

} // namespace ea::gui
