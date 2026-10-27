#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "ea/core/Utils.hpp"

namespace ea {

struct Point {
    int x{0};
    int y{0};
};

class DiagramLink {
public:
    DiagramLink() = default;
    DiagramLink(int diagram_id, int connector_id)
        : m_diagram_id(diagram_id), m_connector_id(connector_id) {}

    int GetDiagramID() const noexcept { return m_diagram_id; }
    void SetDiagramID(int id) noexcept { m_diagram_id = id; }

    int GetConnectorID() const noexcept { return m_connector_id; }
    void SetConnectorID(int id) noexcept { m_connector_id = id; }

    int GetInstanceID() const noexcept { return m_instance_id; }
    void SetInstanceID(int id) noexcept { m_instance_id = id; }

    const std::string& GetGeometry() const noexcept { return m_geometry; }
    void SetGeometry(std::string geom) { m_geometry = std::move(geom); }

    const std::string& GetStyle() const noexcept { return m_style; }
    void SetStyle(std::string style) { m_style = std::move(style); }

    const std::string& GetPath() const noexcept { return m_path; }
    void SetPath(std::string path) { m_path = std::move(path); }

    bool IsHidden() const noexcept { return m_hidden; }
    void SetHidden(bool h) noexcept { m_hidden = h; }

    // Parses path points: format "691:-255;554:-255;"
    std::vector<Point> ParseWaypoints() const {
        std::vector<Point> pts;
        if (m_path.empty()) return pts;
        auto parts = utils::split(m_path, ';');
        for (const auto& part : parts) {
            auto coords = utils::split(part, ':');
            if (coords.size() == 2) {
                try {
                    Point p;
                    p.x = std::stoi(coords[0]);
                    p.y = std::stoi(coords[1]);
                    pts.push_back(p);
                } catch (...) {}
            }
        }
        return pts;
    }

    std::string GetName() const {
        return "Connector_" + std::to_string(m_connector_id);
    }

    bool Update() { return true; }

private:
    int m_diagram_id{0};
    int m_connector_id{0};
    int m_instance_id{0};
    std::string m_geometry;
    std::string m_style;
    std::string m_path;
    bool m_hidden{false};
};

} // namespace ea
