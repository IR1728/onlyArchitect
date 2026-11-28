#include "ea/editor/CanvasGeometry.hpp"
#include <algorithm>
#include <cmath>

namespace ea::editor {

double clamp_zoom(double zoom) {
    if (!(zoom > 0)) return 1.0; // NaN or non-positive: back to 100 %
    return std::clamp(zoom, MinZoom, MaxZoom);
}

double zoom_by_steps(double zoom, int steps) {
    return clamp_zoom(clamp_zoom(zoom) * std::pow(ZoomStep, steps));
}

double snap(double value, int grid) {
    if (grid <= 0) return value;
    return std::round(value / grid) * grid;
}

std::pair<double, double> snap_position(double left, double top, int grid) {
    return {snap(left, grid), snap(top, grid)};
}

std::vector<std::pair<Handle, Box>> handle_boxes(const Box& b, double size) {
    const double h = size / 2, cx = (b.left + b.right) / 2, cy = (b.top + b.bottom) / 2;
    auto at = [h](double x, double y) { return Box{x - h, y - h, x + h, y + h}; };
    return {{Handle::TopLeft, at(b.left, b.top)},       {Handle::Top, at(cx, b.top)},
            {Handle::TopRight, at(b.right, b.top)},     {Handle::Right, at(b.right, cy)},
            {Handle::BottomRight, at(b.right, b.bottom)}, {Handle::Bottom, at(cx, b.bottom)},
            {Handle::BottomLeft, at(b.left, b.bottom)}, {Handle::Left, at(b.left, cy)}};
}

Handle handle_at(const Box& box, double x, double y, double size) {
    for (const auto& [handle, hb] : handle_boxes(box, size)) {
        if (x >= hb.left && x <= hb.right && y >= hb.top && y <= hb.bottom) return handle;
    }
    return Handle::None;
}

Box resize_box(const Box& start, Handle handle, double dx, double dy, double minSize, int grid) {
    Box b = start;
    const bool left = handle == Handle::TopLeft || handle == Handle::Left || handle == Handle::BottomLeft;
    const bool right = handle == Handle::TopRight || handle == Handle::Right || handle == Handle::BottomRight;
    const bool top = handle == Handle::TopLeft || handle == Handle::Top || handle == Handle::TopRight;
    const bool bottom = handle == Handle::BottomLeft || handle == Handle::Bottom || handle == Handle::BottomRight;
    if (left) b.left = std::min(snap(start.left + dx, grid), start.right - minSize);
    if (right) b.right = std::max(snap(start.right + dx, grid), start.left + minSize);
    if (top) b.top = std::min(snap(start.top + dy, grid), start.bottom - minSize);
    if (bottom) b.bottom = std::max(snap(start.bottom + dy, grid), start.top + minSize);
    return b;
}

std::vector<size_t> marquee_select(double startX, double startY, double endX, double endY, const std::vector<Box>& symbols) {
    const Box marquee{std::min(startX, endX), std::min(startY, endY), std::max(startX, endX), std::max(startY, endY)};
    const bool crossing = endX < startX;
    std::vector<size_t> selected;
    for (size_t i = 0; i < symbols.size(); ++i) {
        if (crossing ? marquee.intersects(symbols[i]) : marquee.contains(symbols[i])) selected.push_back(i);
    }
    return selected;
}

} // namespace ea::editor
