#include "ea/diagram/DiagramRenderer.hpp"
#include "ea/diagram/CoordinateSystem.hpp"
#include "ea/core/Utils.hpp"
#include <sstream>
#include <fstream>
#include <cmath>
#include <numbers>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <utility>
#include <string_view>
#include <vector>
#include <iostream>
#include <cairo.h>

namespace ea {

namespace {

// Material Design 3 on Nord for exported diagrams (diseño/ guide): the same tokens and 4 px grid
// metrics as the interactive canvas (gui/DarkTheme.hpp), kept here because the core has no Qt.
namespace nord {
constexpr const char* SurfaceLowest = "#1E222A";    // canvas
constexpr const char* Surface = "#2E3440";          // package body
constexpr const char* SurfaceContainer = "#3B4252"; // node body
constexpr const char* SurfaceHigh = "#434C5E";      // node header (gradient start)
constexpr const char* HeaderEnd = "#3E485B";        // node header (gradient end)
constexpr const char* Outline = "#4C566A";
constexpr const char* OutlineVariant = "#3D4555";
constexpr const char* OnSurface = "#ECEFF4";
constexpr const char* OnSurfaceVariant = "#D8DEE9";
constexpr const char* Primary = "#88C0D0";
constexpr const char* Secondary = "#81A1C1";
constexpr const char* Tertiary = "#B48EAD";
constexpr const char* Error = "#BF616A";
constexpr const char* Warning = "#EBCB8B";
constexpr const char* Success = "#A3BE8C";
constexpr const char* NoteBody = "#3E4250";
constexpr const char* PrimaryContainer = "#475B68";
} // namespace nord

constexpr const char* SansFamily = "Inter, 'Plus Jakarta Sans', 'Segoe UI', 'Noto Sans', Cantarell, sans-serif";
constexpr const char* MonoFamily = "'JetBrains Mono', 'Fira Code', 'Cascadia Code', 'Noto Sans Mono', monospace";

namespace m3 {
constexpr double CornerRadius = 12.0;   // shape medium
constexpr double PadX = 12.0;           // compartment indentation
constexpr double PadY = 8.0;            // compartment padding
constexpr double Gutter = 16.0;         // visibility column
constexpr double LineH = 20.0;          // feature line height
constexpr double HeaderH = 36.0;        // name only
constexpr double HeaderStereoH = 44.0;  // «stereotype» + name
constexpr double TextBaseline = 14.0;   // baseline inside a 20 px line (12 px mono)
} // namespace m3

struct NodeStyle {
    const char* body;
    const char* accent;
};

NodeStyle node_style(ObjectType type) {
    switch (type) {
        case ObjectType::Package: return {nord::Surface, nord::Secondary};
        case ObjectType::Note: return {nord::NoteBody, nord::Warning};
        case ObjectType::Interface: return {nord::SurfaceContainer, nord::Tertiary};
        case ObjectType::Enumeration: return {nord::SurfaceContainer, nord::Secondary};
        case ObjectType::UseCase: return {nord::SurfaceContainer, nord::Primary};
        case ObjectType::Component: return {nord::PrimaryContainer, nord::Primary};
        case ObjectType::Actor: return {nord::SurfaceContainer, nord::Success};
        default: return {nord::SurfaceContainer, nord::Outline};
    }
}

char visibility_char(Scope scope) {
    switch (scope) {
        case Scope::Private: return '-';
        case Scope::Protected: return '#';
        case Scope::Package: return '~';
        case Scope::Public: break;
    }
    return '+';
}

const char* visibility_color(Scope scope) {
    switch (scope) {
        case Scope::Private: return nord::Error;
        case Scope::Protected: return nord::Warning;
        case Scope::Package: return nord::Secondary;
        case Scope::Public: break;
    }
    return nord::Success;
}

// One feature line: visibility at x + 12, signature at x + 28, type in the secondary accent
void svg_feature(std::ostringstream& svg, const ScreenRect& r, double top, Scope scope, const std::string& signature,
                 const std::string& type, const char* signature_color) {
    const double baseline = top + m3::TextBaseline;
    svg << "<text x=\"" << (r.x + m3::PadX) << "\" y=\"" << baseline << "\" font-family=\"" << MonoFamily
        << "\" font-size=\"12\" fill=\"" << visibility_color(scope) << "\">" << visibility_char(scope) << "</text>\n";
    svg << "<text x=\"" << (r.x + m3::PadX + m3::Gutter) << "\" y=\"" << baseline << "\" font-family=\"" << MonoFamily
        << "\" font-size=\"12\" fill=\"" << signature_color << "\">" << utils::escape_xml(signature);
    if (!type.empty()) svg << "<tspan fill=\"" << nord::Secondary << "\"> : " << utils::escape_xml(type) << "</tspan>";
    svg << "</text>\n";
}

// ---- Note text wrapping (no font metrics in the core: average glyph advance of the sans face)
constexpr double NoteFontSize = 13.0;
constexpr double NoteLineH = 20.0;           // body medium line height
constexpr double SansAdvanceEm = 0.55;       // average advance of Inter / Noto Sans, in em
constexpr const char* Ellipsis = "\u2026";

size_t utf8_length(std::string_view text) {
    size_t n = 0;
    for (unsigned char ch : text) n += (ch & 0xC0) != 0x80;
    return n;
}

// Byte offset of the first `count` code points of `text`
size_t utf8_prefix(std::string_view text, size_t count) {
    size_t i = 0;
    for (size_t seen = 0; i < text.size(); ++i) {
        if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80 && seen++ == count) break;
    }
    return i;
}

// Greedy word wrap at `max_chars` per line; explicit line breaks are kept and words longer than
// a line are split. At most `max_lines`, the last one ending in an ellipsis when text is cut.
std::vector<std::string> wrap_text(std::string_view text, size_t max_chars, size_t max_lines) {
    std::vector<std::string> lines;
    if (max_chars == 0 || max_lines == 0) return lines;
    bool truncated = false;

    auto push = [&](std::string line) {
        if (lines.size() == max_lines) { truncated = true; return; }
        lines.push_back(std::move(line));
    };

    size_t start = 0;
    while (start <= text.size() && !truncated) {
        size_t end = text.find('\n', start);
        if (end == std::string_view::npos) end = text.size();
        std::string_view paragraph = text.substr(start, end - start);
        if (!paragraph.empty() && paragraph.back() == '\r') paragraph.remove_suffix(1);

        std::string line;
        size_t pos = 0;
        while (pos < paragraph.size() && !truncated) {
            const size_t space = paragraph.find(' ', pos);
            std::string_view word = paragraph.substr(pos, space == std::string_view::npos ? std::string_view::npos : space - pos);
            pos = space == std::string_view::npos ? paragraph.size() : space + 1;
            if (word.empty()) continue;

            const size_t needed = utf8_length(line) + (line.empty() ? 0 : 1) + utf8_length(word);
            if (needed <= max_chars) {
                if (!line.empty()) line += ' ';
                line += word;
                continue;
            }
            if (!line.empty()) push(std::exchange(line, std::string()));
            while (utf8_length(word) > max_chars && !truncated) { // a word wider than the note
                const size_t cut = utf8_prefix(word, max_chars);
                push(std::string(word.substr(0, cut)));
                word.remove_prefix(cut);
            }
            line = std::string(word);
        }
        if (!truncated) push(std::move(line));
        start = end + 1;
    }

    if (truncated && !lines.empty()) {
        std::string& last = lines.back();
        const size_t keep = max_chars > 1 ? max_chars - 1 : 0;
        if (utf8_length(last) > keep) last.resize(utf8_prefix(last, keep));
        last += Ellipsis;
    }
    while (!lines.empty() && lines.back().empty()) lines.pop_back();
    return lines;
}

void svg_note_text(std::ostringstream& svg, const ScreenRect& r, const std::string& text, double fold) {
    const double width = r.width - 2 * m3::PadX - fold / 2.0;
    const double height = r.height - 2 * m3::PadY;
    const auto max_chars = static_cast<size_t>(std::max(0.0, width / (NoteFontSize * SansAdvanceEm)));
    const auto max_lines = static_cast<size_t>(std::max(0.0, height / NoteLineH));
    const std::vector<std::string> lines = wrap_text(text, max_chars, max_lines);
    if (lines.empty()) return;

    // One <text> per line: SVG Tiny viewers (QtSvg) ignore tspan dy, so lines must be positioned absolutely
    const double x = r.x + m3::PadX;
    double baseline = r.y + m3::PadY + m3::TextBaseline;
    for (const std::string& line : lines) {
        svg << "<text x=\"" << x << "\" y=\"" << baseline << "\" font-family=\"" << SansFamily << "\" font-size=\"" << NoteFontSize
            << "\" fill=\"" << nord::OnSurfaceVariant << "\">" << utils::escape_xml(line) << "</text>\n";
        baseline += NoteLineH;
    }
}

// UML actor: stick figure filling the symbol above its name (UML 2.5 §18.1.4)
void svg_actor(std::ostringstream& svg, const ScreenRect& r, const std::string& name, const char* accent) {
    constexpr double NameBand = 20.0;
    const double cx = r.x + r.width / 2.0;
    const double figure_h = std::max(0.0, r.height - NameBand);
    const double head = std::min(r.width, figure_h) * 0.13;
    const double head_cy = r.y + head + 1.0;
    const double neck = head_cy + head;
    const double hip = r.y + figure_h * 0.62;
    const double arms = neck + (hip - neck) * 0.3;
    const double span = std::min(r.width / 2.0, figure_h * 0.25);
    const double foot = r.y + figure_h - 1.0;

    svg << "<g fill=\"none\" stroke=\"" << accent << "\" stroke-width=\"1.5\" stroke-linecap=\"round\">"
        << "<circle cx=\"" << cx << "\" cy=\"" << head_cy << "\" r=\"" << head << "\" fill=\"" << nord::SurfaceContainer << "\" />"
        << "<line x1=\"" << cx << "\" y1=\"" << neck << "\" x2=\"" << cx << "\" y2=\"" << hip << "\" />"
        << "<line x1=\"" << (cx - span) << "\" y1=\"" << arms << "\" x2=\"" << (cx + span) << "\" y2=\"" << arms << "\" />"
        << "<line x1=\"" << cx << "\" y1=\"" << hip << "\" x2=\"" << (cx - span * 0.8) << "\" y2=\"" << foot << "\" />"
        << "<line x1=\"" << cx << "\" y1=\"" << hip << "\" x2=\"" << (cx + span * 0.8) << "\" y2=\"" << foot << "\" />"
        << "</g>\n";
    svg << "<text x=\"" << cx << "\" y=\"" << (r.y + r.height - 5.0) << "\" font-family=\"" << SansFamily
        << "\" font-size=\"13\" font-weight=\"500\" text-anchor=\"middle\" fill=\"" << nord::OnSurface << "\">"
        << utils::escape_xml(name) << "</text>\n";
}

// UML 2 component glyph (box with two tabs) in the top-right corner of the header
void svg_component_glyph(std::ostringstream& svg, const ScreenRect& r, const char* accent) {
    constexpr double W = 14.0, H = 16.0, TabW = 8.0, TabH = 4.0;
    const double x = r.x + r.width - m3::PadX - W + TabW / 2.0;
    const double y = r.y + m3::PadY + 2.0;
    svg << "<g fill=\"" << nord::SurfaceContainer << "\" stroke=\"" << accent << "\" stroke-width=\"1\">"
        << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << W << "\" height=\"" << H << "\" rx=\"1.5\" />";
    for (double tab_y : {y + 3.0, y + H - 3.0 - TabH}) {
        svg << "<rect x=\"" << (x - TabW / 2.0) << "\" y=\"" << tab_y << "\" width=\"" << TabW << "\" height=\"" << TabH << "\" rx=\"1\" />";
    }
    svg << "</g>\n";
}

void svg_divider(std::ostringstream& svg, const ScreenRect& r, double y) {
    svg << "<line x1=\"" << r.x << "\" y1=\"" << y << "\" x2=\"" << (r.x + r.width) << "\" y2=\"" << y
        << "\" stroke=\"" << nord::OutlineVariant << "\" stroke-width=\"1\" />\n";
}

void cairo_hex(cairo_t* cr, const char* hex) {
    if (!hex || hex[0] != '#' || std::strlen(hex) < 7) {
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        return;
    }
    unsigned v = 0;
    try {
        v = std::stoul(std::string(hex + 1, 6), nullptr, 16);
    } catch (...) {
        v = 0x888888;
    }
    cairo_set_source_rgb(cr, ((v >> 16) & 0xFF) / 255.0, ((v >> 8) & 0xFF) / 255.0, (v & 0xFF) / 255.0);
}

void cairo_rounded_rect(cairo_t* cr, double x, double y, double w, double h, double radius) {
    radius = std::min(radius, std::min(w, h) / 2.0);
    constexpr double Quarter = std::numbers::pi / 2.0;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - radius, y + radius, radius, -Quarter, 0);
    cairo_arc(cr, x + w - radius, y + h - radius, radius, 0, Quarter);
    cairo_arc(cr, x + radius, y + h - radius, radius, Quarter, 2 * Quarter);
    cairo_arc(cr, x + radius, y + radius, radius, 2 * Quarter, 3 * Quarter);
    cairo_close_path(cr);
}

void cairo_hex_alpha(cairo_t* cr, const char* hex, double alpha) {
    if (!hex || hex[0] != '#' || std::strlen(hex) < 7) {
        cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, alpha);
        return;
    }
    unsigned v = 0;
    try {
        v = std::stoul(std::string(hex + 1, 6), nullptr, 16);
    } catch (...) {
        v = 0x888888;
    }
    cairo_set_source_rgba(cr, ((v >> 16) & 0xFF) / 255.0, ((v >> 8) & 0xFF) / 255.0, (v & 0xFF) / 255.0, alpha);
}

int z_layer_order(const DiagramObject& dobj, Repository& repo) {
    auto elem = repo.GetElementByID(dobj.GetElementID());
    const ObjectType t = elem ? elem->GetType() : ObjectType::Unknown;
    const int seq = dobj.GetSequence();
    // Smaller return value is drawn FIRST (further in the back).
    if (t == ObjectType::ActivityPartition) return -20000 - seq;
    if (t == ObjectType::Boundary) return -15000 - seq;
    if (t == ObjectType::Package && dobj.GetWidth() >= 180 && dobj.GetHeight() >= 130) return -8000 - seq;
    if (t == ObjectType::Activity && dobj.GetWidth() >= 110 && dobj.GetHeight() >= 65) return -4000 - seq;
    if (t == ObjectType::InteractionFragment || t == ObjectType::InteractionOccurrence) return -2000 - seq;
    return -seq;
}

ScreenPoint clip_to_rect_border(const ScreenRect& rect, const ScreenPoint& target) {
    const double cx = rect.x + rect.width / 2.0;
    const double cy = rect.y + rect.height / 2.0;
    const double dx = target.x - cx;
    const double dy = target.y - cy;
    if (std::abs(dx) < 1e-4 && std::abs(dy) < 1e-4) return {cx, cy};

    double t_min = 1.0;
    if (dx > 0) {
        double t = (rect.x + rect.width - cx) / dx;
        double y = cy + t * dy;
        if (t >= 0 && t < t_min && y >= rect.y - 1.0 && y <= rect.y + rect.height + 1.0) t_min = t;
    } else if (dx < 0) {
        double t = (rect.x - cx) / dx;
        double y = cy + t * dy;
        if (t >= 0 && t < t_min && y >= rect.y - 1.0 && y <= rect.y + rect.height + 1.0) t_min = t;
    }
    if (dy > 0) {
        double t = (rect.y + rect.height - cy) / dy;
        double x = cx + t * dx;
        if (t >= 0 && t < t_min && x >= rect.x - 1.0 && x <= rect.x + rect.width + 1.0) t_min = t;
    } else if (dy < 0) {
        double t = (rect.y - cy) / dy;
        double x = cx + t * dx;
        if (t >= 0 && t < t_min && x >= rect.x - 1.0 && x <= rect.x + rect.width + 1.0) t_min = t;
    }
    return {cx + t_min * dx, cy + t_min * dy};
}

std::vector<ScreenPoint> build_connector_screen_points(const Diagram& diagram, const DiagramLink& dlink,
                                                       const Connector& conn, const CoordinateSystem& coords) {
    const DiagramObject* start_dobj = nullptr;
    const DiagramObject* end_dobj = nullptr;
    for (size_t i = 0; i < diagram.GetDiagramObjects().size(); ++i) {
        auto obj = diagram.GetDiagramObjects().GetAt(static_cast<short>(i));
        if (!obj) continue;
        if (obj->GetElementID() == conn.GetClientID()) start_dobj = obj.get();
        if (obj->GetElementID() == conn.GetSupplierID()) end_dobj = obj.get();
    }
    std::vector<ScreenPoint> pts;
    const auto waypoints = dlink.ParseWaypoints();
    if (start_dobj && end_dobj) {
        const ScreenRect r1 = coords.ToScreenRect(*start_dobj);
        const ScreenRect r2 = coords.ToScreenRect(*end_dobj);
        if (!waypoints.empty()) {
            ScreenPoint firstWp = coords.ToScreenPoint(waypoints.front().x, waypoints.front().y);
            pts.push_back(clip_to_rect_border(r1, firstWp));
            for (const auto& wp : waypoints) pts.push_back(coords.ToScreenPoint(wp.x, wp.y));
            ScreenPoint lastWp = coords.ToScreenPoint(waypoints.back().x, waypoints.back().y);
            pts.push_back(clip_to_rect_border(r2, lastWp));
        } else {
            pts.push_back(clip_to_rect_border(r1, {r2.x + r2.width / 2.0, r2.y + r2.height / 2.0}));
            pts.push_back(clip_to_rect_border(r2, {r1.x + r1.width / 2.0, r1.y + r1.height / 2.0}));
        }
    } else if (waypoints.size() >= 2) {
        for (const auto& wp : waypoints) pts.push_back(coords.ToScreenPoint(wp.x, wp.y));
    }
    return pts;
}

void cairo_draw_connector(cairo_t* cr, const Diagram& diagram, const DiagramLink& dlink,
                          Repository& repo, const CoordinateSystem& coords) {
    auto conn = repo.GetConnectorByID(dlink.GetConnectorID());
    if (!conn) return;
    const auto pts = build_connector_screen_points(diagram, dlink, *conn, coords);
    if (pts.size() < 2) return;

    const ConnectorType type = conn->GetType();
    cairo_save(cr);
    cairo_hex(cr, nord::Secondary);
    cairo_set_line_width(cr, 1.5);
    if (type == ConnectorType::Realization || type == ConnectorType::Dependency ||
        type == ConnectorType::Usage || type == ConnectorType::Abstraction ||
        type == ConnectorType::UseCaseLink || type == ConnectorType::PackageImport ||
        type == ConnectorType::PackageMerge) {
        const double dashes[] = {6.0, 4.0};
        cairo_set_dash(cr, dashes, 2, 0.0);
    } else if (type == ConnectorType::NoteLink) {
        const double dashes[] = {3.0, 3.0};
        cairo_set_dash(cr, dashes, 2, 0.0);
    }

    cairo_move_to(cr, pts[0].x, pts[0].y);
    for (size_t i = 1; i < pts.size(); ++i) cairo_line_to(cr, pts[i].x, pts[i].y);
    cairo_stroke(cr);
    cairo_set_dash(cr, nullptr, 0, 0.0);

    const ScreenPoint& prev = pts[pts.size() - 2];
    const ScreenPoint& tip = pts.back();
    const double angle = std::atan2(tip.y - prev.y, tip.x - prev.x);
    constexpr double ArrowSize = 12.0;
    const double x1 = tip.x - std::cos(angle - std::numbers::pi / 6.0) * ArrowSize;
    const double y1 = tip.y - std::sin(angle - std::numbers::pi / 6.0) * ArrowSize;
    const double x2 = tip.x - std::cos(angle + std::numbers::pi / 6.0) * ArrowSize;
    const double y2 = tip.y - std::sin(angle + std::numbers::pi / 6.0) * ArrowSize;

    if (type == ConnectorType::Generalization || type == ConnectorType::Realization) {
        cairo_move_to(cr, tip.x, tip.y);
        cairo_line_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
        cairo_close_path(cr);
        cairo_hex(cr, nord::SurfaceLowest);
        cairo_fill_preserve(cr);
        cairo_hex(cr, nord::Secondary);
        cairo_stroke(cr);
    } else if (type == ConnectorType::Composition || type == ConnectorType::Aggregation) {
        const ScreenPoint& s = pts[0];
        const ScreenPoint& sNext = pts[1];
        const double dx = sNext.x - s.x, dy = sNext.y - s.y;
        const double len = std::hypot(dx, dy);
        if (len > 1e-4) {
            const double ux = dx / len, uy = dy / len;
            const double nx = -uy, ny = ux;
            cairo_move_to(cr, s.x, s.y);
            cairo_line_to(cr, s.x + ux * 7.0 + nx * 4.5, s.y + uy * 7.0 + ny * 4.5);
            cairo_line_to(cr, s.x + ux * 14.0, s.y + uy * 14.0);
            cairo_line_to(cr, s.x + ux * 7.0 - nx * 4.5, s.y + uy * 7.0 - ny * 4.5);
            cairo_close_path(cr);
            cairo_hex(cr, type == ConnectorType::Composition ? nord::Secondary : nord::SurfaceLowest);
            cairo_fill_preserve(cr);
            cairo_hex(cr, nord::Secondary);
            cairo_stroke(cr);
        }
    } else if (type == ConnectorType::Message) {
        cairo_move_to(cr, tip.x, tip.y);
        cairo_line_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
        cairo_close_path(cr);
        cairo_hex(cr, nord::Secondary);
        cairo_fill(cr);
    } else if (type == ConnectorType::Dependency || type == ConnectorType::Usage ||
               type == ConnectorType::Abstraction || type == ConnectorType::ControlFlow ||
               type == ConnectorType::ObjectFlow || type == ConnectorType::Transition ||
               type == ConnectorType::UseCaseLink || type == ConnectorType::CommunicationMessage ||
               conn->GetDirection() == "Source -> Destination" || conn->GetDirection() == "Bi-Directional") {
        cairo_move_to(cr, x1, y1);
        cairo_line_to(cr, tip.x, tip.y);
        cairo_line_to(cr, x2, y2);
        cairo_stroke(cr);
    }

    if (!conn->GetName().empty() || !conn->GetStereotype().empty()) {
        const size_t midIdx = pts.size() / 2;
        const double mx = (pts[midIdx - 1].x + pts[midIdx].x) / 2.0;
        const double my = (pts[midIdx - 1].y + pts[midIdx].y) / 2.0 - 6.0;
        std::string label;
        if (!conn->GetStereotype().empty()) label += "«" + conn->GetStereotype() + "» ";
        label += conn->GetName();
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11.0);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, label.c_str(), &ext);
        cairo_hex(cr, nord::OnSurfaceVariant);
        cairo_move_to(cr, mx - ext.width / 2.0, my);
        cairo_show_text(cr, label.c_str());
    }
    cairo_restore(cr);
}

} // namespace

DiagramRenderer::DiagramRenderer() = default;
DiagramRenderer::~DiagramRenderer() = default;

std::string DiagramRenderer::RenderToSVG(const Diagram& diagram, Repository& repo) {
    std::vector<std::shared_ptr<DiagramObject>> objects;
    for (size_t i = 0; i < diagram.GetDiagramObjects().size(); ++i) {
        objects.push_back(diagram.GetDiagramObjects().GetAt(static_cast<short>(i)));
    }

    std::vector<std::shared_ptr<DiagramLink>> links;
    for (size_t i = 0; i < diagram.GetDiagramLinks().size(); ++i) {
        links.push_back(diagram.GetDiagramLinks().GetAt(static_cast<short>(i)));
    }

    CoordinateSystem coords(50.0);
    coords.ComputeBounds(objects, links);

    double width = coords.GetCanvasWidth();
    double height = coords.GetCanvasHeight();

    std::ostringstream svg;
    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n";
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" "
        << "width=\"" << width << "\" height=\"" << height << "\" "
        << "viewBox=\"0 0 " << width << " " << height << "\">\n";

    // Defs: UML markers in Nord Frost, M3 elevation-1 shadow, Gemini header gradient
    svg << "<defs>\n"
        << "  <marker id=\"generalization\" markerWidth=\"12\" markerHeight=\"12\" refX=\"12\" refY=\"6\" orient=\"auto\">\n"
        << "    <polygon points=\"0 0, 12 6, 0 12\" fill=\"" << nord::SurfaceLowest << "\" stroke=\"" << nord::Secondary << "\" stroke-width=\"1.5\" />\n"
        << "  </marker>\n"
        << "  <marker id=\"arrow\" markerWidth=\"10\" markerHeight=\"10\" refX=\"10\" refY=\"5\" orient=\"auto\">\n"
        << "    <path d=\"M0,0 L10,5 L0,10\" fill=\"none\" stroke=\"" << nord::Secondary << "\" stroke-width=\"1.5\" />\n"
        << "  </marker>\n"
        << "  <marker id=\"composition\" markerWidth=\"16\" markerHeight=\"10\" refX=\"16\" refY=\"5\" orient=\"auto\">\n"
        << "    <polygon points=\"0 5, 8 0, 16 5, 8 10\" fill=\"" << nord::Secondary << "\" stroke=\"" << nord::Secondary << "\" stroke-width=\"1\" />\n"
        << "  </marker>\n"
        << "  <marker id=\"aggregation\" markerWidth=\"16\" markerHeight=\"10\" refX=\"16\" refY=\"5\" orient=\"auto\">\n"
        << "    <polygon points=\"0 5, 8 0, 16 5, 8 10\" fill=\"" << nord::SurfaceLowest << "\" stroke=\"" << nord::Secondary << "\" stroke-width=\"1.5\" />\n"
        << "  </marker>\n"
        << "  <filter id=\"drop-shadow\" x=\"-5%\" y=\"-5%\" width=\"115%\" height=\"120%\">\n"
        << "    <feDropShadow dx=\"0\" dy=\"3\" stdDeviation=\"3\" flood-color=\"#0F1218\" flood-opacity=\"0.43\" />\n"
        << "  </filter>\n"
        << "  <linearGradient id=\"m3-header\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"1\">\n"
        << "    <stop offset=\"0\" stop-color=\"" << nord::SurfaceHigh << "\" /><stop offset=\"1\" stop-color=\"" << nord::HeaderEnd << "\" />\n"
        << "  </linearGradient>\n"
        << "</defs>\n\n";

    // Background: M3 surface-container-lowest
    // (absolute size: SVG Tiny viewers such as QtSvg read "100%" as 100 user units)
    svg << "<rect x=\"0\" y=\"0\" width=\"" << width << "\" height=\"" << height << "\" fill=\"" << nord::SurfaceLowest << "\" />\n";

    // Diagram title: M3 headline medium
    svg << "<text x=\"20\" y=\"36\" font-family=\"" << SansFamily << "\" font-size=\"24\" font-weight=\"600\" letter-spacing=\"-0.2\" fill=\"" << nord::OnSurfaceVariant << "\">"
        << utils::escape_xml(diagram.GetName()) << " (" << to_string(diagram.GetType()) << ")</text>\n";

    // Sort elements by EA Z-order (containers & higher Sequence first, frontmost last)
    std::stable_sort(objects.begin(), objects.end(), [&](const auto& a, const auto& b) {
        if (!a || !b) return static_cast<bool>(a);
        return z_layer_order(*a, repo) < z_layer_order(*b, repo);
    });

    // Render Elements first (containers behind nodes)
    for (const auto& obj : objects) {
        if (obj) {
            RenderSVGObject(svg, *obj, repo, coords);
        }
    }

    // Render Connectors on top so arrowheads and labels are never obscured by element cards
    for (const auto& link : links) {
        if (link && !link->IsHidden()) {
            RenderSVGConnector(svg, *link, repo, coords);
        }
    }

    svg << "</svg>\n";
    return svg.str();
}

void DiagramRenderer::RenderSVGObject(std::ostringstream& svg, const DiagramObject& dobj, Repository& repo, const CoordinateSystem& coords) {
    auto elem = repo.GetElementByID(dobj.GetElementID());
    ScreenRect r = coords.ToScreenRect(dobj);

    std::string name = elem ? elem->GetName() : ("ID: " + std::to_string(dobj.GetElementID()));
    ObjectType type = elem ? elem->GetType() : ObjectType::Class;
    std::string stereotype = elem ? elem->GetStereotype() : "";
    if (type == ObjectType::Interface && stereotype.empty()) stereotype = "interface";
    const NodeStyle style = node_style(type);

    if (type == ObjectType::UseCase) {
        double cx = r.x + r.width / 2.0;
        double cy = r.y + r.height / 2.0;
        svg << "<ellipse cx=\"" << cx << "\" cy=\"" << cy << "\" rx=\"" << (r.width / 2.0) << "\" ry=\"" << (r.height / 2.0) << "\" "
            << "fill=\"" << style.body << "\" stroke=\"" << style.accent << "\" stroke-width=\"1\" filter=\"url(#drop-shadow)\" />\n";
        svg << "<text x=\"" << cx << "\" y=\"" << (cy + 5) << "\" font-family=\"" << SansFamily << "\" font-size=\"13\" font-weight=\"500\" "
            << "text-anchor=\"middle\" fill=\"" << nord::OnSurface << "\">" << utils::escape_xml(name) << "</text>\n";
        return;
    }

    if (type == ObjectType::Note) {
        constexpr double Fold = 12.0;
        svg << "<polygon points=\""
            << r.x << " " << r.y << ", "
            << (r.x + r.width - Fold) << " " << r.y << ", "
            << (r.x + r.width) << " " << (r.y + Fold) << ", "
            << (r.x + r.width) << " " << (r.y + r.height) << ", "
            << r.x << " " << (r.y + r.height) << "\" "
            << "fill=\"" << style.body << "\" stroke=\"" << style.accent << "\" stroke-width=\"1\" filter=\"url(#drop-shadow)\" />\n";
        svg << "<polygon points=\""
            << (r.x + r.width - Fold) << " " << r.y << ", "
            << (r.x + r.width - Fold) << " " << (r.y + Fold) << ", "
            << (r.x + r.width) << " " << (r.y + Fold) << "\" "
            << "fill=\"" << nord::SurfaceHigh << "\" stroke=\"" << style.accent << "\" stroke-width=\"1\" />\n";

        svg_note_text(svg, r, elem ? elem->GetNotes() : name, Fold);
        return;
    }

    if (type == ObjectType::Actor) {
        svg_actor(svg, r, name, nord::Success);
        return;
    }

    if (type == ObjectType::ActivityPartition || type == ObjectType::Boundary) {
        const double header_h = (type == ObjectType::ActivityPartition) ? std::min(26.0, r.height) : 0.0;
        svg << "<rect x=\"" << r.x << "\" y=\"" << r.y << "\" width=\"" << r.width << "\" height=\"" << r.height
            << "\" rx=\"8\" ry=\"8\" fill=\"" << nord::SurfaceContainer << "\" fill-opacity=\"0.18\" stroke=\""
            << nord::Secondary << "\" stroke-width=\"1.3\" />\n";
        if (header_h > 0.0) {
            svg << "<line x1=\"" << r.x << "\" y1=\"" << (r.y + header_h) << "\" x2=\"" << (r.x + r.width)
                << "\" y2=\"" << (r.y + header_h) << "\" stroke=\"" << nord::Secondary << "\" stroke-width=\"1.2\" />\n";
        }
        svg << "<text x=\"" << (r.x + r.width / 2.0) << "\" y=\"" << (r.y + 18.0) << "\" font-family=\"" << SansFamily
            << "\" font-size=\"13\" font-weight=\"600\" text-anchor=\"middle\" fill=\"" << nord::OnSurface << "\">"
            << utils::escape_xml(name) << "</text>\n";
        return;
    }

    if (type == ObjectType::InitialNode || type == ObjectType::FinalState) {
        const double d = std::min(r.width, r.height);
        const double cx = r.x + r.width / 2.0, cy = r.y + r.height / 2.0;
        if (type == ObjectType::InitialNode) {
            svg << "<circle cx=\"" << cx << "\" cy=\"" << cy << "\" r=\"" << (d / 2.0)
                << "\" fill=\"" << nord::OnSurface << "\" stroke=\"" << nord::Primary << "\" stroke-width=\"1.5\" />\n";
        } else {
            svg << "<circle cx=\"" << cx << "\" cy=\"" << cy << "\" r=\"" << (d / 2.0)
                << "\" fill=\"" << nord::SurfaceLowest << "\" stroke=\"" << nord::OnSurface << "\" stroke-width=\"1.8\" />\n"
                << "<circle cx=\"" << cx << "\" cy=\"" << cy << "\" r=\"" << (d * 0.3)
                << "\" fill=\"" << nord::OnSurface << "\" />\n";
        }
        if (!name.empty()) {
            svg << "<text x=\"" << cx << "\" y=\"" << (r.y + r.height + 14.0) << "\" font-family=\"" << SansFamily
                << "\" font-size=\"11\" text-anchor=\"middle\" fill=\"" << nord::OnSurface << "\">"
                << utils::escape_xml(name) << "</text>\n";
        }
        return;
    }

    if (type == ObjectType::Decision || type == ObjectType::MergeNode) {
        const double cx = r.x + r.width / 2.0, cy = r.y + r.height / 2.0;
        svg << "<polygon points=\"" << cx << "," << r.y << " " << (r.x + r.width) << "," << cy << " "
            << cx << "," << (r.y + r.height) << " " << r.x << "," << cy << "\" fill=\""
            << nord::SurfaceContainer << "\" stroke=\"" << nord::Warning << "\" stroke-width=\"1.5\" />\n";
        return;
    }

    if (type == ObjectType::Action) {
        const double rad = std::min(16.0, std::min(r.width, r.height) * 0.35);
        svg << "<rect x=\"" << r.x << "\" y=\"" << r.y << "\" width=\"" << r.width << "\" height=\"" << r.height
            << "\" rx=\"" << rad << "\" ry=\"" << rad << "\" fill=\"" << nord::SurfaceContainer
            << "\" stroke=\"" << nord::Primary << "\" stroke-width=\"1.3\" />\n"
            << "<text x=\"" << (r.x + r.width / 2.0) << "\" y=\"" << (r.y + r.height / 2.0 + 4.0)
            << "\" font-family=\"" << SansFamily << "\" font-size=\"12\" text-anchor=\"middle\" fill=\""
            << nord::OnSurface << "\">" << utils::escape_xml(name) << "</text>\n";
        return;
    }

    // UML card (class, interface, package, component, ...): 12 px corners, header gradient clipped to them
    // unique within this document even for symbols not saved yet (instance id 0)
    const std::string clip_id = "card-" + std::to_string(reinterpret_cast<std::uintptr_t>(&dobj));
    svg << "<clipPath id=\"" << clip_id << "\"><rect x=\"" << r.x << "\" y=\"" << r.y << "\" width=\"" << r.width << "\" height=\"" << r.height
        << "\" rx=\"" << m3::CornerRadius << "\" ry=\"" << m3::CornerRadius << "\" /></clipPath>\n";
    svg << "<rect x=\"" << r.x << "\" y=\"" << r.y << "\" width=\"" << r.width << "\" height=\"" << r.height << "\" "
        << "rx=\"" << m3::CornerRadius << "\" ry=\"" << m3::CornerRadius << "\" fill=\"" << style.body << "\" filter=\"url(#drop-shadow)\" />\n";

    const double header_h = stereotype.empty() ? m3::HeaderH : m3::HeaderStereoH;
    svg << "<rect x=\"" << r.x << "\" y=\"" << r.y << "\" width=\"" << r.width << "\" height=\"" << header_h
        << "\" fill=\"url(#m3-header)\" clip-path=\"url(#" << clip_id << ")\" />\n";

    const double cx = r.x + r.width / 2.0;
    double baseline = r.y + (stereotype.empty() ? 23.0 : 17.0);
    if (!stereotype.empty()) {
        svg << "<text x=\"" << cx << "\" y=\"" << baseline << "\" font-family=\"" << SansFamily << "\" font-size=\"11\" font-weight=\"500\" letter-spacing=\"0.5\" "
            << "text-anchor=\"middle\" fill=\"" << (type == ObjectType::Class ? nord::Primary : style.accent) << "\">&#171;"
            << utils::escape_xml(stereotype) << "&#187;</text>\n";
        baseline += 18.0;
    }
    svg << "<text x=\"" << cx << "\" y=\"" << baseline << "\" font-family=\"" << SansFamily << "\" font-size=\"14\" font-weight=\"600\" letter-spacing=\"0.15\" "
        << "text-anchor=\"middle\" fill=\"" << nord::OnSurface << "\">" << utils::escape_xml(name) << "</text>\n";
    svg_divider(svg, r, r.y + header_h);

    // Attribute and operation compartments on the 4 px grid
    double y = r.y + header_h;
    const double bottom = r.y + r.height;
    if (elem) {
        const auto& attrs = elem->GetAttributes();
        if (attrs.size() > 0) y += m3::PadY;
        for (size_t i = 0; i < attrs.size() && y + m3::LineH <= bottom; ++i) {
            auto attr = attrs.GetAt(static_cast<short>(i));
            if (!attr) continue;
            svg_feature(svg, r, y, attr->GetVisibility(), attr->GetName(), attr->GetType(), nord::OnSurfaceVariant);
            y += m3::LineH;
        }
        if (attrs.size() > 0) y += m3::PadY;

        const auto& methods = elem->GetMethods();
        if (methods.size() > 0 && y + m3::PadY + m3::LineH <= bottom) {
            svg_divider(svg, r, y);
            y += m3::PadY;
            for (size_t i = 0; i < methods.size() && y + m3::LineH <= bottom; ++i) {
                auto meth = methods.GetAt(static_cast<short>(i));
                if (!meth) continue;
                svg_feature(svg, r, y, meth->GetVisibility(), meth->GetName() + "()", meth->GetReturnType(), nord::OnSurface);
                y += m3::LineH;
            }
        }
    }

    if (type == ObjectType::Component) svg_component_glyph(svg, r, style.accent);

    // 1 px M3 outline on top
    svg << "<rect x=\"" << r.x << "\" y=\"" << r.y << "\" width=\"" << r.width << "\" height=\"" << r.height << "\" "
        << "rx=\"" << m3::CornerRadius << "\" ry=\"" << m3::CornerRadius << "\" fill=\"none\" stroke=\"" << style.accent << "\" stroke-width=\"1\" />\n";
}

void DiagramRenderer::RenderSVGConnector(std::ostringstream& svg, const DiagramLink& dlink, Repository& repo, const CoordinateSystem& coords) {
    auto conn = repo.GetConnectorByID(dlink.GetConnectorID());
    if (!conn) return;

    auto diag = repo.GetDiagramByID(dlink.GetDiagramID());
    std::vector<ScreenPoint> screen_pts = diag
        ? build_connector_screen_points(*diag, dlink, *conn, coords)
        : std::vector<ScreenPoint>{};
    if (screen_pts.size() < 2) {
        for (const auto& wp : dlink.ParseWaypoints()) {
            screen_pts.push_back(coords.ToScreenPoint(wp.x, wp.y));
        }
    }

    if (screen_pts.size() < 2) return;

    // Marker selection
    std::string marker_end = "";
    std::string stroke_dash = "";
    ConnectorType type = conn->GetType();

    if (type == ConnectorType::Generalization) {
        marker_end = "marker-end=\"url(#generalization)\"";
    } else if (type == ConnectorType::Realization) {
        marker_end = "marker-end=\"url(#generalization)\"";
        stroke_dash = "stroke-dasharray=\"6,4\"";
    } else if (type == ConnectorType::Dependency || type == ConnectorType::Usage ||
               type == ConnectorType::Abstraction || type == ConnectorType::UseCaseLink) {
        marker_end = "marker-end=\"url(#arrow)\"";
        stroke_dash = "stroke-dasharray=\"5,5\"";
    } else if (type == ConnectorType::ControlFlow || type == ConnectorType::ObjectFlow ||
               type == ConnectorType::Transition || type == ConnectorType::CommunicationMessage ||
               type == ConnectorType::Message) {
        marker_end = "marker-end=\"url(#arrow)\"";
    } else if (type == ConnectorType::Composition) {
        marker_end = "marker-start=\"url(#composition)\"";
    } else if (type == ConnectorType::Aggregation) {
        marker_end = "marker-start=\"url(#aggregation)\"";
    } else if (type == ConnectorType::NoteLink) {
        stroke_dash = "stroke-dasharray=\"3,3\"";
    } else {
        if (conn->GetDirection() == "Source -> Destination" || conn->GetDirection() == "Bi-Directional") {
            marker_end = "marker-end=\"url(#arrow)\"";
        }
    }

    svg << "<polyline points=\"";
    for (size_t i = 0; i < screen_pts.size(); ++i) {
        svg << screen_pts[i].x << "," << screen_pts[i].y << (i + 1 < screen_pts.size() ? " " : "");
    }
    svg << "\" fill=\"none\" stroke=\"" << nord::Secondary << "\" stroke-width=\"1.5\" "
        << marker_end << " " << stroke_dash << " />\n";

    // Connector Name Label
    if (!conn->GetName().empty() && screen_pts.size() >= 2) {
        double mid_x = (screen_pts[0].x + screen_pts[1].x) / 2.0;
        double mid_y = (screen_pts[0].y + screen_pts[1].y) / 2.0 - 6.0;
        svg << "<text x=\"" << mid_x << "\" y=\"" << mid_y << "\" font-family=\"" << SansFamily << "\" font-size=\"11\" font-weight=\"500\" "
            << "text-anchor=\"middle\" fill=\"" << nord::OnSurfaceVariant << "\">" << utils::escape_xml(conn->GetName()) << "</text>\n";
    }
}

bool DiagramRenderer::RenderToSVGFile(const Diagram& diagram, Repository& repo, const std::string& output_path) {
    std::string content = RenderToSVG(diagram, repo);
    std::ofstream out(output_path);
    if (!out.is_open()) return false;
    out << content;
    return true;
}

bool DiagramRenderer::RenderToPNGFile(const Diagram& diagram, Repository& repo, const std::string& output_path) {
    std::vector<std::shared_ptr<DiagramObject>> objects;
    for (size_t i = 0; i < diagram.GetDiagramObjects().size(); ++i) {
        objects.push_back(diagram.GetDiagramObjects().GetAt(static_cast<short>(i)));
    }
    std::vector<std::shared_ptr<DiagramLink>> links;
    for (size_t i = 0; i < diagram.GetDiagramLinks().size(); ++i) {
        links.push_back(diagram.GetDiagramLinks().GetAt(static_cast<short>(i)));
    }

    CoordinateSystem coords(50.0);
    coords.ComputeBounds(objects, links);

    int width = static_cast<int>(std::ceil(coords.GetCanvasWidth()));
    int height = static_cast<int>(std::ceil(coords.GetCanvasHeight()));

    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t* cr = cairo_create(surface);

    // Background: M3 surface-container-lowest
    cairo_hex(cr, nord::SurfaceLowest);
    cairo_paint(cr);

    // Title: headline medium
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 24.0);
    cairo_hex(cr, nord::OnSurfaceVariant);
    cairo_move_to(cr, 20.0, 36.0);
    cairo_show_text(cr, diagram.GetName().c_str());

    // Sort objects by EA Z-order (containers behind child nodes)
    std::stable_sort(objects.begin(), objects.end(), [&](const auto& a, const auto& b) {
        if (!a || !b) return static_cast<bool>(a);
        return z_layer_order(*a, repo) < z_layer_order(*b, repo);
    });

    // Render nodes
    for (const auto& obj : objects) {
        if (!obj) continue;
        ScreenRect r = coords.ToScreenRect(*obj);
        auto elem = repo.GetElementByID(obj->GetElementID());
        std::string name = elem ? elem->GetName() : ("ID:" + std::to_string(obj->GetElementID()));
        const ObjectType otype = elem ? elem->GetType() : ObjectType::Class;
        const NodeStyle style = node_style(otype);

        if (otype == ObjectType::ActivityPartition || otype == ObjectType::Boundary) {
            cairo_rounded_rect(cr, r.x, r.y, r.width, r.height, 8.0);
            cairo_hex_alpha(cr, nord::SurfaceContainer, 0.18);
            cairo_fill_preserve(cr);
            cairo_hex(cr, nord::Secondary);
            cairo_set_line_width(cr, 1.3);
            cairo_stroke(cr);
            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 13.0);
            cairo_hex(cr, nord::OnSurface);
            cairo_text_extents_t ext;
            cairo_text_extents(cr, name.c_str(), &ext);
            cairo_move_to(cr, r.x + (r.width - ext.width) / 2.0, r.y + 18.0);
            cairo_show_text(cr, name.c_str());
            continue;
        }

        if (otype == ObjectType::InitialNode || otype == ObjectType::FinalState) {
            const double d = std::min(r.width, r.height);
            const double cx = r.x + r.width / 2.0, cy = r.y + r.height / 2.0;
            cairo_arc(cr, cx, cy, d / 2.0, 0, 2 * std::numbers::pi);
            if (otype == ObjectType::InitialNode) {
                cairo_hex(cr, nord::OnSurface);
                cairo_fill(cr);
            } else {
                cairo_hex(cr, nord::SurfaceLowest);
                cairo_fill_preserve(cr);
                cairo_hex(cr, nord::OnSurface);
                cairo_set_line_width(cr, 1.8);
                cairo_stroke(cr);
                cairo_arc(cr, cx, cy, d * 0.3, 0, 2 * std::numbers::pi);
                cairo_fill(cr);
            }
            continue;
        }

        if (otype == ObjectType::Decision || otype == ObjectType::MergeNode) {
            const double cx = r.x + r.width / 2.0, cy = r.y + r.height / 2.0;
            cairo_move_to(cr, cx, r.y);
            cairo_line_to(cr, r.x + r.width, cy);
            cairo_line_to(cr, cx, r.y + r.height);
            cairo_line_to(cr, r.x, cy);
            cairo_close_path(cr);
            cairo_hex(cr, nord::SurfaceContainer);
            cairo_fill_preserve(cr);
            cairo_hex(cr, nord::Warning);
            cairo_set_line_width(cr, 1.5);
            cairo_stroke(cr);
            continue;
        }

        cairo_rounded_rect(cr, r.x, r.y, r.width, r.height, m3::CornerRadius);
        cairo_hex(cr, style.body);
        cairo_fill_preserve(cr);
        cairo_save(cr);
        cairo_clip(cr);
        cairo_hex(cr, nord::SurfaceHigh);
        cairo_rectangle(cr, r.x, r.y, r.width, m3::HeaderH);
        cairo_fill(cr);
        cairo_restore(cr);

        // Name: title medium
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 14.0);
        cairo_hex(cr, nord::OnSurface);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, name.c_str(), &ext);
        cairo_move_to(cr, r.x + (r.width - ext.width) / 2.0, r.y + 23.0);
        cairo_show_text(cr, name.c_str());

        cairo_hex(cr, nord::OutlineVariant);
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, r.x, r.y + m3::HeaderH);
        cairo_line_to(cr, r.x + r.width, r.y + m3::HeaderH);
        cairo_stroke(cr);

        // Attributes & operations
        if (elem) {
            double top = r.y + m3::HeaderH + m3::PadY;
            cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, 12.0);
            for (size_t i = 0; i < elem->GetAttributes().size(); ++i) {
                if (top + m3::LineH > r.y + r.height) break;
                auto attr = elem->GetAttributes().GetAt(static_cast<short>(i));
                if (!attr) continue;
                const char vis[2] = {visibility_char(attr->GetVisibility()), '\0'};
                cairo_hex(cr, visibility_color(attr->GetVisibility()));
                cairo_move_to(cr, r.x + m3::PadX, top + m3::TextBaseline);
                cairo_show_text(cr, vis);
                const std::string line = attr->GetName() + (attr->GetType().empty() ? "" : (" : " + attr->GetType()));
                cairo_hex(cr, nord::OnSurfaceVariant);
                cairo_move_to(cr, r.x + m3::PadX + m3::Gutter, top + m3::TextBaseline);
                cairo_show_text(cr, line.c_str());
                top += m3::LineH;
            }
            if (elem->GetMethods().size() > 0 && top + m3::PadY + m3::LineH <= r.y + r.height) {
                cairo_hex(cr, nord::OutlineVariant);
                cairo_move_to(cr, r.x, top);
                cairo_line_to(cr, r.x + r.width, top);
                cairo_stroke(cr);
                top += m3::PadY;
                for (size_t i = 0; i < elem->GetMethods().size(); ++i) {
                    if (top + m3::LineH > r.y + r.height) break;
                    auto meth = elem->GetMethods().GetAt(static_cast<short>(i));
                    if (!meth) continue;
                    const char vis[2] = {visibility_char(meth->GetVisibility()), '\0'};
                    cairo_hex(cr, visibility_color(meth->GetVisibility()));
                    cairo_move_to(cr, r.x + m3::PadX, top + m3::TextBaseline);
                    cairo_show_text(cr, vis);
                    const std::string line = meth->GetName() + "()" + (meth->GetReturnType().empty() ? "" : (" : " + meth->GetReturnType()));
                    cairo_hex(cr, nord::OnSurface);
                    cairo_move_to(cr, r.x + m3::PadX + m3::Gutter, top + m3::TextBaseline);
                    cairo_show_text(cr, line.c_str());
                    top += m3::LineH;
                }
            }
        }

        cairo_rounded_rect(cr, r.x, r.y, r.width, r.height, m3::CornerRadius);
        cairo_hex(cr, style.accent);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
    }

    // Render all visible connectors on top with arrowheads & labels
    for (const auto& link : links) {
        if (link && !link->IsHidden()) {
            cairo_draw_connector(cr, diagram, *link, repo, coords);
        }
    }

    cairo_status_t status = cairo_surface_write_to_png(surface, output_path.c_str());
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    return status == CAIRO_STATUS_SUCCESS;
}

} // namespace ea
