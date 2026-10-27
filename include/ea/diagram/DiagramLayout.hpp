#pragma once

#include <memory>
#include <vector>
#include "ea/model/Diagram.hpp"
#include "ea/repository/Repository.hpp"

namespace ea {

enum class LayoutAlgorithm {
    ForceDirected,
    Hierarchical,
    Grid
};

class DiagramLayout {
public:
    static void Layout(Diagram& diagram, Repository& repo, LayoutAlgorithm algo = LayoutAlgorithm::ForceDirected);
    static void ForceDirectedLayout(Diagram& diagram, Repository& repo, int iterations = 100);
    static void HierarchicalLayout(Diagram& diagram, Repository& repo);
    static void GridLayout(Diagram& diagram, int columns = 4, int cell_w = 200, int cell_h = 120);
};

} // namespace ea
