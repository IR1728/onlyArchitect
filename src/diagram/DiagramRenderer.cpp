#include "ea/diagram/DiagramRenderer.hpp"
#include "ea/diagram/CoordinateSystem.hpp"
#include "ea/core/Utils.hpp"
#include <sstream>
#include <fstream>
#include <cmath>
#include <iostream>
#include <cairo.h>

namespace ea {

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

    // Defs & Markers
    svg << "<defs>\n"
        << "  <marker id=\"generalization\" markerWidth=\"12\" markerHeight=\"12\" refX=\"12\" refY=\"6\" orient=\"auto\">\n"
        << "    <polygon points=\"0 0, 12 6, 0 12\" fill=\"#ffffff\" stroke=\"#333333\" stroke-width=\"1.5\" />\n"
        << "  </marker>\n"
        << "  <marker id=\"arrow\" markerWidth=\"10\" markerHeight=\"10\" refX=\"10\" refY=\"5\" orient=\"auto\">\n"
        << "    <path d=\"M0,0 L10,5 L0,10\" fill=\"none\" stroke=\"#333333\" stroke-width=\"1.5\" />\n"
        << "  </marker>\n"
        << "  <marker id=\"composition\" markerWidth=\"16\" markerHeight=\"10\" refX=\"16\" refY=\"5\" orient=\"auto\">\n"
        << "    <polygon points=\"0 5, 8 0, 16 5, 8 10\" fill=\"#333333\" stroke=\"#333333\" stroke-width=\"1\" />\n"
        << "  </marker>\n"
        << "  <marker id=\"aggregation\" markerWidth=\"16\" markerHeight=\"10\" refX=\"16\" refY=\"5\" orient=\"auto\">\n"
        << "    <polygon points=\"0 5, 8 0, 16 5, 8 10\" fill=\"#ffffff\" stroke=\"#333333\" stroke-width=\"1.5\" />\n"
        << "  </marker>\n"
        << "  <filter id=\"drop-shadow\" x=\"-5%\" y=\"-5%\" width=\"115%\" height=\"115%\">\n"
        << "    <feDropShadow dx=\"2\" dy=\"3\" stdDeviation=\"3\" flood-opacity=\"0.15\" />\n"
        << "  </filter>\n"
        << "</defs>\n\n";

    // Background
    svg << "<rect width=\"100%\" height=\"100%\" fill=\"#fcfcfc\" />\n";

    // Diagram Title Header
    svg << "<text x=\"20\" y=\"30\" font-family=\"Arial, sans-serif\" font-size=\"16\" font-weight=\"bold\" fill=\"#222222\">"
        << utils::escape_xml(diagram.GetName()) << " (" << to_string(diagram.GetType()) << ")</text>\n";

    // Render Connectors first so they sit below nodes
    for (const auto& link : links) {
        if (link && !link->IsHidden()) {
            RenderSVGConnector(svg, *link, repo, coords);
        }
    }

    // Render Elements on top
    for (const auto& obj : objects) {
        if (obj) {
            RenderSVGObject(svg, *obj, repo, coords);
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

    // Style colors
    std::string bg_color = "#ffffeb";
    std::string border_color = "#7d7d7d";

    if (type == ObjectType::Package) {
        bg_color = "#fffdf0";
        border_color = "#b09b60";
    } else if (type == ObjectType::Note) {
        bg_color = "#fffae0";
        border_color = "#c7b777";
    } else if (type == ObjectType::Interface) {
        bg_color = "#f0f8ff";
        border_color = "#5f9ea0";
    } else if (type == ObjectType::Enumeration) {
        bg_color = "#f5f5f5";
        border_color = "#778899";
    } else if (type == ObjectType::UseCase) {
        bg_color = "#e8f4fc";
        border_color = "#336699";
    }

    if (type == ObjectType::UseCase) {
        // Oval for UseCase
        double cx = r.x + r.width / 2.0;
        double cy = r.y + r.height / 2.0;
        svg << "<ellipse cx=\"" << cx << "\" cy=\"" << cy << "\" rx=\"" << (r.width / 2.0) << "\" ry=\"" << (r.height / 2.0) << "\" "
            << "fill=\"" << bg_color << "\" stroke=\"" << border_color << "\" stroke-width=\"1.5\" filter=\"url(#drop-shadow)\" />\n";
        svg << "<text x=\"" << cx << "\" y=\"" << (cy + 5) << "\" font-family=\"Arial, sans-serif\" font-size=\"12\" "
            << "text-anchor=\"middle\" fill=\"#111111\">" << utils::escape_xml(name) << "</text>\n";
        return;
    }

    if (type == ObjectType::Note) {
        // Note with folded ear
        double fold = 12.0;
        svg << "<polygon points=\""
            << r.x << " " << r.y << ", "
            << (r.x + r.width - fold) << " " << r.y << ", "
            << (r.x + r.width) << " " << (r.y + fold) << ", "
            << (r.x + r.width) << " " << (r.y + r.height) << ", "
            << r.x << " " << (r.y + r.height) << "\" "
            << "fill=\"" << bg_color << "\" stroke=\"" << border_color << "\" stroke-width=\"1.2\" filter=\"url(#drop-shadow)\" />\n";
        svg << "<polygon points=\""
            << (r.x + r.width - fold) << " " << r.y << ", "
            << (r.x + r.width - fold) << " " << (r.y + fold) << ", "
            << (r.x + r.width) << " " << (r.y + fold) << "\" "
            << "fill=\"#eee9cb\" stroke=\"" << border_color << "\" stroke-width=\"1\" />\n";

        std::string note_txt = elem ? elem->GetNotes() : name;
        svg << "<text x=\"" << (r.x + 8) << "\" y=\"" << (r.y + 20) << "\" font-family=\"Arial, sans-serif\" font-size=\"11\" fill=\"#333333\">"
            << utils::escape_xml(note_txt.substr(0, 80)) << "</text>\n";
        return;
    }

    // UML Box (Class, Interface, Package, Component, etc.)
    svg << "<rect x=\"" << r.x << "\" y=\"" << r.y << "\" width=\"" << r.width << "\" height=\"" << r.height << "\" "
        << "rx=\"2\" ry=\"2\" fill=\"" << bg_color << "\" stroke=\"" << border_color << "\" stroke-width=\"1.5\" filter=\"url(#drop-shadow)\" />\n";

    // Header Compartment
    double cur_y = r.y + 14.0;
    if (!stereotype.empty()) {
        svg << "<text x=\"" << (r.x + r.width / 2.0) << "\" y=\"" << cur_y << "\" font-family=\"Arial, sans-serif\" font-size=\"10\" font-style=\"italic\" "
            << "text-anchor=\"middle\" fill=\"#555555\">&laquo;" << utils::escape_xml(stereotype) << "&raquo;</text>\n";
        cur_y += 14.0;
    }

    svg << "<text x=\"" << (r.x + r.width / 2.0) << "\" y=\"" << cur_y << "\" font-family=\"Arial, sans-serif\" font-size=\"12\" font-weight=\"bold\" "
        << "text-anchor=\"middle\" fill=\"#111111\">" << utils::escape_xml(name) << "</text>\n";
    cur_y += 8.0;

    // Line below header
    svg << "<line x1=\"" << r.x << "\" y1=\"" << cur_y << "\" x2=\"" << (r.x + r.width) << "\" y2=\"" << cur_y << "\" stroke=\"" << border_color << "\" stroke-width=\"1\" />\n";
    cur_y += 14.0;

    // Attributes Compartment
    if (elem) {
        for (size_t i = 0; i < elem->GetAttributes().size(); ++i) {
            if (cur_y > r.y + r.height - 20) break;
            auto attr = elem->GetAttributes().GetAt(static_cast<short>(i));
            if (!attr) continue;

            char vis = '+';
            if (attr->GetVisibility() == Scope::Private) vis = '-';
            else if (attr->GetVisibility() == Scope::Protected) vis = '#';
            else if (attr->GetVisibility() == Scope::Package) vis = '~';

            svg << "<text x=\"" << (r.x + 8) << "\" y=\"" << cur_y << "\" font-family=\"monospace\" font-size=\"11\" fill=\"#222222\">"
                << vis << " " << utils::escape_xml(attr->GetName()) << " : " << utils::escape_xml(attr->GetType()) << "</text>\n";
            cur_y += 14.0;
        }

        // Line between Attributes and Operations if there are methods
        if (elem->GetMethods().size() > 0 && cur_y < r.y + r.height - 15) {
            svg << "<line x1=\"" << r.x << "\" y1=\"" << cur_y << "\" x2=\"" << (r.x + r.width) << "\" y2=\"" << cur_y << "\" stroke=\"" << border_color << "\" stroke-width=\"1\" />\n";
            cur_y += 14.0;

            for (size_t i = 0; i < elem->GetMethods().size(); ++i) {
                if (cur_y > r.y + r.height - 10) break;
                auto meth = elem->GetMethods().GetAt(static_cast<short>(i));
                if (!meth) continue;

                char vis = '+';
                if (meth->GetVisibility() == Scope::Private) vis = '-';
                else if (meth->GetVisibility() == Scope::Protected) vis = '#';
                else if (meth->GetVisibility() == Scope::Package) vis = '~';

                svg << "<text x=\"" << (r.x + 8) << "\" y=\"" << cur_y << "\" font-family=\"monospace\" font-size=\"11\" fill=\"#222222\">"
                    << vis << " " << utils::escape_xml(meth->GetName()) << "() : " << utils::escape_xml(meth->GetReturnType()) << "</text>\n";
                cur_y += 14.0;
            }
        }
    }
}

void DiagramRenderer::RenderSVGConnector(std::ostringstream& svg, const DiagramLink& dlink, Repository& repo, const CoordinateSystem& coords) {
    auto conn = repo.GetConnectorByID(dlink.GetConnectorID());
    if (!conn) return;

    auto start_elem = repo.GetElementByID(conn->GetClientID());
    auto end_elem = repo.GetElementByID(conn->GetSupplierID());

    std::vector<Point> ea_waypoints = dlink.ParseWaypoints();
    std::vector<ScreenPoint> screen_pts;

    // Check if waypoints exist
    if (!ea_waypoints.empty()) {
        for (const auto& wp : ea_waypoints) {
            screen_pts.push_back(coords.ToScreenPoint(wp.x, wp.y));
        }
    } else if (start_elem && end_elem) {
        // Fallback: Connect centers of start and end objects
        // Look up diagram objects for both
        DiagramObject start_dobj, end_dobj;
        bool found_start = false, found_end = false;
        // Search in repository for the diagram
        auto diag = repo.GetDiagramByID(dlink.GetDiagramID());
        if (diag) {
            for (size_t i = 0; i < diag->GetDiagramObjects().size(); ++i) {
                auto obj = diag->GetDiagramObjects().GetAt(static_cast<short>(i));
                if (obj && obj->GetElementID() == conn->GetClientID()) {
                    start_dobj = *obj;
                    found_start = true;
                }
                if (obj && obj->GetElementID() == conn->GetSupplierID()) {
                    end_dobj = *obj;
                    found_end = true;
                }
            }
        }
        if (found_start && found_end) {
            ScreenRect r1 = coords.ToScreenRect(start_dobj);
            ScreenRect r2 = coords.ToScreenRect(end_dobj);
            screen_pts.push_back({r1.x + r1.width / 2.0, r1.y + r1.height / 2.0});
            screen_pts.push_back({r2.x + r2.width / 2.0, r2.y + r2.height / 2.0});
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
    } else if (type == ConnectorType::Dependency) {
        marker_end = "marker-end=\"url(#arrow)\"";
        stroke_dash = "stroke-dasharray=\"5,5\"";
    } else if (type == ConnectorType::Composition) {
        marker_end = "marker-start=\"url(#composition)\"";
    } else if (type == ConnectorType::Aggregation) {
        marker_end = "marker-start=\"url(#aggregation)\"";
    } else if (type == ConnectorType::NoteLink) {
        stroke_dash = "stroke-dasharray=\"3,3\"";
    } else {
        if (conn->GetDirection() == "Source -> Destination") {
            marker_end = "marker-end=\"url(#arrow)\"";
        }
    }

    svg << "<polyline points=\"";
    for (size_t i = 0; i < screen_pts.size(); ++i) {
        svg << screen_pts[i].x << "," << screen_pts[i].y << (i + 1 < screen_pts.size() ? " " : "");
    }
    svg << "\" fill=\"none\" stroke=\"#444444\" stroke-width=\"1.5\" "
        << marker_end << " " << stroke_dash << " />\n";

    // Connector Name Label
    if (!conn->GetName().empty() && screen_pts.size() >= 2) {
        double mid_x = (screen_pts[0].x + screen_pts[1].x) / 2.0;
        double mid_y = (screen_pts[0].y + screen_pts[1].y) / 2.0 - 6.0;
        svg << "<text x=\"" << mid_x << "\" y=\"" << mid_y << "\" font-family=\"Arial, sans-serif\" font-size=\"11\" "
            << "text-anchor=\"middle\" fill=\"#333333\">" << utils::escape_xml(conn->GetName()) << "</text>\n";
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

    // Background white
    cairo_set_source_rgb(cr, 0.98, 0.98, 0.98);
    cairo_paint(cr);

    // Title
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 16.0);
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_move_to(cr, 20.0, 30.0);
    cairo_show_text(cr, diagram.GetName().c_str());

    // Render nodes
    for (const auto& obj : objects) {
        if (!obj) continue;
        ScreenRect r = coords.ToScreenRect(*obj);
        auto elem = repo.GetElementByID(obj->GetElementID());
        std::string name = elem ? elem->GetName() : ("ID:" + std::to_string(obj->GetElementID()));

        // Box
        cairo_set_source_rgb(cr, 1.0, 1.0, 0.92);
        cairo_rectangle(cr, r.x, r.y, r.width, r.height);
        cairo_fill_preserve(cr);

        cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
        cairo_set_line_width(cr, 1.5);
        cairo_stroke(cr);

        // Header text
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 12.0);
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, name.c_str(), &ext);
        cairo_move_to(cr, r.x + (r.width - ext.width) / 2.0, r.y + 20.0);
        cairo_show_text(cr, name.c_str());

        // Line
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, r.x, r.y + 28.0);
        cairo_line_to(cr, r.x + r.width, r.y + 28.0);
        cairo_stroke(cr);

        // Attributes
        if (elem) {
            double cur_y = r.y + 44.0;
            cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, 11.0);
            cairo_set_source_rgb(cr, 0.15, 0.15, 0.15);

            for (size_t i = 0; i < elem->GetAttributes().size(); ++i) {
                if (cur_y > r.y + r.height - 10) break;
                auto attr = elem->GetAttributes().GetAt(static_cast<short>(i));
                if (!attr) continue;
                std::string line = "+ " + attr->GetName() + " : " + attr->GetType();
                cairo_move_to(cr, r.x + 8.0, cur_y);
                cairo_show_text(cr, line.c_str());
                cur_y += 14.0;
            }
        }
    }

    cairo_status_t status = cairo_surface_write_to_png(surface, output_path.c_str());
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    return status == CAIRO_STATUS_SUCCESS;
}

} // namespace ea
