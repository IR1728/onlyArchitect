#include "ea/diagram/DiagramLayout.hpp"
#include <cmath>
#include <unordered_map>
#include <vector>

namespace ea {

void DiagramLayout::Layout(Diagram& diagram, Repository& repo, LayoutAlgorithm algo) {
    switch (algo) {
        case LayoutAlgorithm::ForceDirected:
            ForceDirectedLayout(diagram, repo);
            break;
        case LayoutAlgorithm::Hierarchical:
            HierarchicalLayout(diagram, repo);
            break;
        case LayoutAlgorithm::Grid:
            GridLayout(diagram);
            break;
    }
}

void DiagramLayout::GridLayout(Diagram& diagram, int columns, int cell_w, int cell_h) {
    if (columns <= 0) columns = 4;
    int row = 0;
    int col = 0;
    int start_x = 50;
    int start_y = -50;

    for (size_t i = 0; i < diagram.GetDiagramObjects().size(); ++i) {
        auto obj = diagram.GetDiagramObjects().GetAt(static_cast<short>(i));
        if (!obj) continue;

        int w = obj->GetWidth() > 0 ? obj->GetWidth() : 120;
        int h = obj->GetHeight() > 0 ? obj->GetHeight() : 80;

        int left = start_x + col * (cell_w + 40);
        int top = start_y - row * (cell_h + 40);
        int right = left + w;
        int bottom = top - h;

        obj->SetLeft(left);
        obj->SetTop(top);
        obj->SetRight(right);
        obj->SetBottom(bottom);

        col++;
        if (col >= columns) {
            col = 0;
            row++;
        }
    }
}

void DiagramLayout::ForceDirectedLayout(Diagram& diagram, Repository& repo, int iterations) {
    struct Node {
        std::shared_ptr<DiagramObject> dobj;
        double x{0.0};
        double y{0.0};
        double vx{0.0};
        double vy{0.0};
        double width{120.0};
        double height{80.0};
    };

    std::vector<Node> nodes;
    std::unordered_map<int, size_t> elem_to_node;

    for (size_t i = 0; i < diagram.GetDiagramObjects().size(); ++i) {
        auto dobj = diagram.GetDiagramObjects().GetAt(static_cast<short>(i));
        if (!dobj) continue;

        Node n;
        n.dobj = dobj;
        n.width = dobj->GetWidth() > 0 ? dobj->GetWidth() : 120.0;
        n.height = dobj->GetHeight() > 0 ? dobj->GetHeight() : 80.0;
        n.x = dobj->GetLeft() + n.width / 2.0;
        n.y = dobj->GetTop() - n.height / 2.0;

        elem_to_node[dobj->GetElementID()] = nodes.size();
        nodes.push_back(n);
    }

    if (nodes.empty()) return;

    // Simulation parameters
    const double k_repulse = 40000.0;
    const double k_spring = 0.05;
    const double desired_dist = 220.0;
    const double damping = 0.85;

    for (int iter = 0; iter < iterations; ++iter) {
        // 1. Repulsion between all pairs
        for (size_t i = 0; i < nodes.size(); ++i) {
            for (size_t j = i + 1; j < nodes.size(); ++j) {
                double dx = nodes[i].x - nodes[j].x;
                double dy = nodes[i].y - nodes[j].y;
                double dist_sq = dx * dx + dy * dy;
                if (dist_sq < 100.0) dist_sq = 100.0; // Avoid div by zero
                double dist = std::sqrt(dist_sq);

                double force = k_repulse / dist_sq;
                double fx = (dx / dist) * force;
                double fy = (dy / dist) * force;

                nodes[i].vx += fx;
                nodes[i].vy += fy;
                nodes[j].vx -= fx;
                nodes[j].vy -= fy;
            }
        }

        // 2. Attraction along connectors
        for (size_t i = 0; i < diagram.GetDiagramLinks().size(); ++i) {
            auto link = diagram.GetDiagramLinks().GetAt(static_cast<short>(i));
            if (!link) continue;
            auto conn = repo.GetConnectorByID(link->GetConnectorID());
            if (!conn) continue;

            auto it1 = elem_to_node.find(conn->GetClientID());
            auto it2 = elem_to_node.find(conn->GetSupplierID());
            if (it1 == elem_to_node.end() || it2 == elem_to_node.end()) continue;

            Node& n1 = nodes[it1->second];
            Node& n2 = nodes[it2->second];

            double dx = n2.x - n1.x;
            double dy = n2.y - n1.y;
            double dist = std::sqrt(dx * dx + dy * dy);
            if (dist < 1.0) dist = 1.0;

            double displacement = dist - desired_dist;
            double force = k_spring * displacement;
            double fx = (dx / dist) * force;
            double fy = (dy / dist) * force;

            n1.vx += fx;
            n1.vy += fy;
            n2.vx -= fx;
            n2.vy -= fy;
        }

        // 3. Update positions with damping
        for (auto& n : nodes) {
            n.x += n.vx;
            n.y += n.vy;
            n.vx *= damping;
            n.vy *= damping;
        }
    }

    // Write back to DiagramObjects
    for (const auto& n : nodes) {
        int left = static_cast<int>(n.x - n.width / 2.0);
        int right = static_cast<int>(n.x + n.width / 2.0);
        int top = static_cast<int>(n.y + n.height / 2.0);
        int bottom = static_cast<int>(n.y - n.height / 2.0);

        n.dobj->SetLeft(left);
        n.dobj->SetRight(right);
        n.dobj->SetTop(top);
        n.dobj->SetBottom(bottom);
    }
}

void DiagramLayout::HierarchicalLayout(Diagram& diagram, [[maybe_unused]] Repository& repo) {
    // Top-down layer assignment
    GridLayout(diagram, 3, 220, 140);
}

} // namespace ea
