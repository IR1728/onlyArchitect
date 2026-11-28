#pragma once

#include <string>
#include <cstdint>
#include <algorithm>

namespace ea {

class DiagramObject {
public:
    DiagramObject() = default;
    DiagramObject(int diagram_id, int object_id, int left, int top, int right, int bottom, int seq = 0)
        : m_diagram_id(diagram_id), m_object_id(object_id),
          m_left(left), m_top(top), m_right(right), m_bottom(bottom), m_sequence(seq) {}

    int GetDiagramID() const noexcept { return m_diagram_id; }
    void SetDiagramID(int id) noexcept { m_diagram_id = id; }

    int GetElementID() const noexcept { return m_object_id; }
    void SetElementID(int id) noexcept { m_object_id = id; }

    int GetInstanceID() const noexcept { return m_instance_id; }
    void SetInstanceID(int id) noexcept { m_instance_id = id; }

    int GetLeft() const noexcept { return m_left; }
    void SetLeft(int l) noexcept { m_left = l; }

    int GetRight() const noexcept { return m_right; }
    void SetRight(int r) noexcept { m_right = r; }

    int GetTop() const noexcept { return m_top; }
    void SetTop(int t) noexcept { m_top = t; }

    int GetBottom() const noexcept { return m_bottom; }
    void SetBottom(int b) noexcept { m_bottom = b; }

    int GetSequence() const noexcept { return m_sequence; }
    void SetSequence(int s) noexcept { m_sequence = s; }

    const std::string& GetStyle() const noexcept { return m_style; }
    void SetStyle(std::string style) { m_style = std::move(style); }

    // Dimensions in standard positive coordinates
    int GetWidth() const noexcept {
        return std::abs(m_right - m_left);
    }

    int GetHeight() const noexcept {
        return std::abs(m_top - m_bottom);
    }

    // Modern C++ name getter for polymorphic collection compatibility
    std::string GetName() const {
        return "Element_" + std::to_string(m_object_id);
    }

    bool Update() { return true; }

private:
    int m_diagram_id{0};
    int m_object_id{0};
    int m_instance_id{0};
    int m_left{0};
    int m_top{0};
    int m_right{0};
    int m_bottom{0};
    int m_sequence{0};
    std::string m_style;
};

} // namespace ea
