#pragma once

// Canvas interaction rules (guiaSecuencia.md §1.1 canvas): zoom limits, grid snapping, the eight
// resize handles and marquee selection. Pure geometry in diagram coordinates (y grows downwards);
// the UI only translates mouse events into these calls.

#include <utility>
#include <vector>

namespace ea::editor {

inline constexpr double MinZoom = 0.10; // 10 %
inline constexpr double MaxZoom = 4.00; // 400 %
inline constexpr double ZoomStep = 1.2;  // one wheel notch / button press
inline constexpr int GridSize = 10;      // snapping grid (the canvas draws a dot every 2 cells)
inline constexpr double MinSymbolSize = 20.0;

double clamp_zoom(double zoom);
double zoom_by_steps(double zoom, int steps); // ×1.2 per step, clamped

double snap(double value, int grid = GridSize);

struct Box {
    double left{0}, top{0}, right{0}, bottom{0};
    double width() const { return right - left; }
    double height() const { return bottom - top; }
    bool contains(const Box& other) const {
        return other.left >= left && other.right <= right && other.top >= top && other.bottom <= bottom;
    }
    bool intersects(const Box& other) const {
        return other.left < right && other.right > left && other.top < bottom && other.bottom > top;
    }
    bool operator==(const Box&) const = default;
};

// A symbol dragged to `topLeft` lands on the grid (its size does not change)
std::pair<double, double> snap_position(double left, double top, int grid = GridSize);

enum class Handle { None, TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left };

// The handle under `x, y` for a symbol `box`; handles are squares of `handleSize` centred on the
// corners and edge midpoints
Handle handle_at(const Box& box, double x, double y, double handleSize);
std::vector<std::pair<Handle, Box>> handle_boxes(const Box& box, double handleSize);

// The box after dragging `handle` by (dx, dy): only the edges the handle owns move, the opposite
// ones stay put; moved edges snap to the grid (unless grid <= 0); never smaller than `minSize`.
Box resize_box(const Box& start, Handle handle, double dx, double dy, double minSize = MinSymbolSize, int grid = GridSize);

// Marquee selection: dragging left-to-right selects the symbols fully inside the marquee, right-to-
// left (a "crossing" selection) also those it touches. Returns indexes into `symbols`.
std::vector<size_t> marquee_select(double startX, double startY, double endX, double endY, const std::vector<Box>& symbols);

} // namespace ea::editor
