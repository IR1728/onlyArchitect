#ifdef NDEBUG
#undef NDEBUG
#endif
// Canvas rules without Qt: zoom limits, snapping, the eight resize handles, marquee selection.
#include <cassert>
#include <cmath>
#include <iostream>
#include "ea/editor/CanvasGeometry.hpp"

using namespace ea::editor;
using H = Handle;

int main() {
    std::cout << "==========================================\n";
    std::cout << " Running onlyArchitect Canvas Geometry Tests (no Qt)\n";
    std::cout << "==========================================\n";

    assert(clamp_zoom(10) == MaxZoom && clamp_zoom(0.001) == MinZoom && clamp_zoom(1.5) == 1.5);
    assert(clamp_zoom(-1) == 1.0 && clamp_zoom(std::nan("")) == 1.0);
    double z = 1.0;
    for (int i = 0; i < 50; ++i) z = zoom_by_steps(z, +1);
    assert(z == MaxZoom);
    for (int i = 0; i < 50; ++i) z = zoom_by_steps(z, -1);
    assert(z == MinZoom);
    assert(std::abs(zoom_by_steps(1.0, 1) - 1.2) < 1e-9);
    std::cout << "  ✓ Zoom stays within 10 %..400 %, ×1.2 per step\n";

    assert(snap(14) == 10 && snap(15) == 20 && snap(-14) == -10 && snap(7, 0) == 7);
    assert((snap_position(103, 207) == std::pair<double, double>{100, 210}));
    std::cout << "  ✓ Grid snapping (10 px), negative coordinates included, grid 0 disables it\n";

    const Box b{100, 100, 270, 200};
    assert(handle_boxes(b, 8).size() == 8);
    assert(handle_at(b, 100, 100, 8) == H::TopLeft && handle_at(b, 185, 100, 8) == H::Top && handle_at(b, 270, 150, 8) == H::Right);
    assert(handle_at(b, 270, 200, 8) == H::BottomRight && handle_at(b, 100, 150, 8) == H::Left && handle_at(b, 180, 150, 8) == H::None);
    assert((resize_box(b, H::BottomRight, 33, 18) == Box{100, 100, 300, 220}));
    assert((resize_box(b, H::TopLeft, -33, -47) == Box{70, 50, 270, 200}) && "opposite corner fixed");
    assert((resize_box(b, H::Left, -12, 999) == Box{90, 100, 270, 200}) && "an edge handle moves one edge only");
    assert((resize_box(b, H::Top, 0, 500) == Box{100, 180, 270, 200}) && "never below the minimum size");
    assert((resize_box(b, H::Right, -1000, 0, 20, 0) == Box{100, 100, 120, 200}));
    assert((resize_box(b, H::BottomRight, 3, 3, 20, 0) == Box{100, 100, 273, 203}) && "free resize without grid");
    std::cout << "  ✓ Eight handles: hit test, only the owned edges move, snapping, minimum size\n";

    const std::vector<Box> symbols = {{0, 0, 50, 50}, {100, 0, 150, 50}, {40, 40, 120, 90}};
    auto enclosed = marquee_select(-10, -10, 160, 60, symbols);
    assert((enclosed == std::vector<size_t>{0, 1}) && "left-to-right: only fully enclosed symbols");
    auto crossing = marquee_select(160, 60, -10, -10, symbols);
    assert((crossing == std::vector<size_t>{0, 1, 2}) && "right-to-left: touching is enough");
    assert(marquee_select(200, 200, 300, 300, symbols).empty());
    std::cout << "  ✓ Marquee: enclosing (left→right) and crossing (right→left) selection\n";

    std::cout << "==========================================\n";
    std::cout << " ALL CANVAS GEOMETRY TESTS PASSED!\n";
    std::cout << "==========================================\n";
    return 0;
}
