#pragma once

#include <cstdlib>
#include "ea/model/DiagramObject.hpp"

namespace ea::editor {

// Bounds of a symbol on a diagram, in EA's t_diagramobjects convention (Top/Bottom are negative Y).
struct ElementGeometry {
    int left{0};
    int top{0};
    int right{0};
    int bottom{0};

    static ElementGeometry of(const DiagramObject& obj) {
        return {obj.GetLeft(), obj.GetTop(), obj.GetRight(), obj.GetBottom()};
    }

    void applyTo(DiagramObject& obj) const {
        obj.SetLeft(left);
        obj.SetTop(top);
        obj.SetRight(right);
        obj.SetBottom(bottom);
    }

    int width() const { return std::abs(right - left); }
    int height() const { return std::abs(top - bottom); }

    bool operator==(const ElementGeometry&) const = default;
};

} // namespace ea::editor
