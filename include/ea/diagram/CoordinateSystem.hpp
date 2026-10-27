#pragma once

#include <algorithm>
#include <vector>
#include <cstdint>
#include <limits>
#include "ea/model/DiagramObject.hpp"
#include "ea/model/DiagramLink.hpp"

namespace ea {

struct ScreenRect {
    double x{0.0};
    double y{0.0};
    double width{0.0};
    double height{0.0};
};

struct ScreenPoint {
    double x{0.0};
    double y{0.0};
};

class CoordinateSystem {
public:
    explicit CoordinateSystem(double margin = 40.0) : m_margin(margin) {}

    // Computes diagram bounds across all diagram objects and link waypoints
    void ComputeBounds(const std::vector<std::shared_ptr<DiagramObject>>& objects,
                      const std::vector<std::shared_ptr<DiagramLink>>& links) {
        if (objects.empty()) {
            m_min_x = 0;
            m_max_x = 800;
            m_min_y = -600;
            m_max_y = 0;
            return;
        }

        m_min_x = std::numeric_limits<int>::max();
        m_max_x = std::numeric_limits<int>::lowest();
        m_min_y = std::numeric_limits<int>::max();
        m_max_y = std::numeric_limits<int>::lowest();

        for (const auto& obj : objects) {
            if (!obj) continue;
            m_min_x = std::min(m_min_x, std::min(obj->GetLeft(), obj->GetRight()));
            m_max_x = std::max(m_max_x, std::max(obj->GetLeft(), obj->GetRight()));
            m_min_y = std::min(m_min_y, std::min(obj->GetTop(), obj->GetBottom()));
            m_max_y = std::max(m_max_y, std::max(obj->GetTop(), obj->GetBottom()));
        }

        for (const auto& link : links) {
            if (!link) continue;
            for (const auto& pt : link->ParseWaypoints()) {
                m_min_x = std::min(m_min_x, pt.x);
                m_max_x = std::max(m_max_x, pt.x);
                m_min_y = std::min(m_min_y, pt.y);
                m_max_y = std::max(m_max_y, pt.y);
            }
        }
    }

    // Transforms an EA rectangle to Screen/SVG coordinates
    ScreenRect ToScreenRect(const DiagramObject& obj) const {
        ScreenRect rect;
        int left = std::min(obj.GetLeft(), obj.GetRight());
        int right = std::max(obj.GetLeft(), obj.GetRight());
        int top = std::max(obj.GetTop(), obj.GetBottom());
        int bottom = std::min(obj.GetTop(), obj.GetBottom());

        rect.x = (left - m_min_x) + m_margin;
        rect.y = (m_max_y - top) + m_margin;
        rect.width = std::max(20, right - left);
        rect.height = std::max(20, top - bottom);
        return rect;
    }

    // Transforms an EA point (x, y) to Screen coordinates
    ScreenPoint ToScreenPoint(int ea_x, int ea_y) const {
        ScreenPoint pt;
        pt.x = (ea_x - m_min_x) + m_margin;
        pt.y = (m_max_y - ea_y) + m_margin;
        return pt;
    }

    double GetCanvasWidth() const noexcept {
        return std::max(100.0, (m_max_x - m_min_x) + 2.0 * m_margin);
    }

    double GetCanvasHeight() const noexcept {
        return std::max(100.0, (m_max_y - m_min_y) + 2.0 * m_margin);
    }

private:
    int m_min_x{0};
    int m_max_x{800};
    int m_min_y{-600};
    int m_max_y{0};
    double m_margin{40.0};
};

} // namespace ea
