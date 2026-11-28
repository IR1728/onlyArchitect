#ifdef NDEBUG
#undef NDEBUG
#endif
// M3 / Nord / Gemini theme (diseño/ guide): dynamic color, "@token" stylesheets, Material Symbols
// variable font icons, UML glyphs and the exported SVG symbols.
#include <QApplication>
#include <QImage>
#include <QLabel>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

#include "ea/diagram/DiagramRenderer.hpp"
#include "ea/repository/Repository.hpp"
#include "gui/DarkTheme.hpp"
#include "gui/M3Icons.hpp"
#include "gui/UmlIcons.hpp"

namespace {

using namespace ea::gui;

void testColorScheme() {
    DarkTheme::apply();
    assert(m3().primary == QColor("#88C0D0"));
    assert(m3().tertiary == QColor("#B48EAD"));
    assert(M3ColorScheme::fromSeed(DarkTheme::defaultSeed()).surfaceLow == M3ColorScheme().surfaceLow);

    const QColor seed("#A3BE8C");
    const M3ColorScheme green = M3ColorScheme::fromSeed(seed);
    assert(green.primary == seed);
    assert(green.surfaceLow != M3ColorScheme().surfaceLow); // tonal surfaces follow the seed
    std::cout << "  [ok] color scheme from seed\n";
}

void testThemedStyleSheets() {
    DarkTheme::apply();
    assert(themed("color: @primary;") == "color: #88c0d0;");
    assert(themed("border: 1px solid @unknownToken;") == "border: 1px solid @unknownToken;");

    QLabel label;
    setThemedStyleSheet(&label, "color: @primary;");
    DarkTheme::apply(QColor("#B48EAD"));
    assert(label.styleSheet() == "color: #b48ead;"); // re-skinned at runtime
    DarkTheme::apply();
    assert(label.styleSheet() == "color: #88c0d0;");
    std::cout << "  [ok] @token stylesheets follow the seed\n";
}

void testSymbolFont() {
    assert(M3Icons::symbolFontLoaded());
    assert(M3Icons::symbolFontFamily().contains("Material Symbols"));
    assert(M3Icons::has("search"));
    assert(M3Icons::has("workflow")); // only in the font, not among the bundled SVGs
    assert(!M3Icons::has("no_such_symbol"));

    const QIcon icon = M3Icons::icon("folder");
    assert(!icon.isNull());
    const QImage outlined = icon.pixmap(M3Icons::LargeSize, QIcon::Normal, QIcon::Off).toImage();
    const QImage filled = icon.pixmap(M3Icons::LargeSize, QIcon::Normal, QIcon::On).toImage();
    assert(!outlined.isNull() && !filled.isNull());
    assert(outlined != filled); // FILL axis + primary tint when checked
    std::cout << "  [ok] Material Symbols variable font\n";
}

void testUmlGlyphs() {
    for (int t = static_cast<int>(ea::ObjectType::Unknown); t <= static_cast<int>(ea::ObjectType::TimeLine); ++t) {
        const auto type = static_cast<ea::ObjectType>(t);
        assert(M3Icons::has(umlElementGlyph(type).symbol));
        assert(!umlElementIcon(type).isNull());
    }
    for (int t = static_cast<int>(ea::ConnectorType::Unknown); t <= static_cast<int>(ea::ConnectorType::CommunicationMessage); ++t) {
        assert(!umlRelationshipIcon(static_cast<ea::ConnectorType>(t)).isNull());
    }
    std::cout << "  [ok] UML element / relationship glyphs\n";
}

int addElement(ea::Repository& repo, int pkg, ea::ObjectType type, const std::string& name, const std::string& notes = "") {
    ea::Element e;
    e.SetType(type);
    e.SetName(name);
    e.SetNotes(notes);
    e.SetPackageID(pkg);
    return repo.AddElement(e)->GetElementID();
}

void place(ea::Repository& repo, ea::Diagram& diagram, int elementId, int left, int top, int width, int height) {
    auto o = std::make_shared<ea::DiagramObject>();
    o->SetElementID(elementId);
    o->SetLeft(left);
    o->SetTop(-top);
    o->SetRight(left + width);
    o->SetBottom(-(top + height));
    repo.AddElementToDiagram(diagram, o);
}

size_t count(const std::string& text, const std::string& needle) {
    size_t n = 0;
    for (size_t pos = text.find(needle); pos != std::string::npos; pos = text.find(needle, pos + needle.size())) ++n;
    return n;
}

void testSvgSymbols() {
    const auto db = std::filesystem::temp_directory_path() / "onlyarchitect_test_theme.qea";
    std::filesystem::remove(db);
    ea::Repository repo;
    assert(repo.CreateRepository(db.string()));
    const int pkg = repo.GetModels().GetAt(0)->GetPackageID();

    const std::string longNote = "Los pedidos confirmados no se pueden editar. Cualquier cambio posterior requiere "
                                 "una nota de credito y un pedido nuevo, revisado por el area de facturacion.";
    const int note = addElement(repo, pkg, ea::ObjectType::Note, "nota", longNote);
    const int actor = addElement(repo, pkg, ea::ObjectType::Actor, "Vendedor");
    const int component = addElement(repo, pkg, ea::ObjectType::Component, "Facturacion");

    ea::Diagram d;
    d.SetName("Simbolos");
    d.SetType(ea::DiagramType::Logical);
    d.SetPackageID(pkg);
    repo.SaveDiagram(d);
    auto diagram = repo.GetDiagramByID(d.GetDiagramID());
    place(repo, *diagram, note, 40, 80, 220, 90);
    place(repo, *diagram, actor, 300, 80, 120, 120);
    place(repo, *diagram, component, 460, 80, 220, 90);

    const std::string svg = ea::DiagramRenderer().RenderToSVG(*diagram, repo);
    // note: wrapped on several lines that fit the 90 px symbol, cut with an ellipsis
    assert(svg.find("Los pedidos confirmados no") != std::string::npos);
    assert(svg.find(longNote) == std::string::npos);
    assert(svg.find("…") != std::string::npos);
    // actor: stick figure (head + 4 limbs) above its name; component: UML 2 glyph with two tabs
    assert(svg.find("<circle") != std::string::npos);
    assert(svg.find(">Vendedor</text>") != std::string::npos);
    assert(count(svg, "rx=\"1\" />") >= 2);
    // background covers the whole canvas (no "100%", which SVG Tiny viewers read as 100 units)
    assert(svg.find("width=\"100%\"") == std::string::npos);

    repo.CloseFile();
    std::filesystem::remove(db);
    std::cout << "  [ok] SVG note wrapping, actor and component symbols\n";
}

} // namespace

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    std::cout << "Running M3 theme tests\n";
    testColorScheme();
    testThemedStyleSheets();
    testSymbolFont();
    testUmlGlyphs();
    testSvgSymbols();
    std::cout << "All M3 theme tests passed\n";
    return 0;
}
