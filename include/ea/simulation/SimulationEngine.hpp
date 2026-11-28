#pragma once

#include <string>
#include <utility>
#include <vector>
#include "ea/core/Types.hpp"
#include "ea/repository/Repository.hpp"

namespace ea::simulation {

struct SimNode {
    int elementId{0};
    std::string id;
    std::string name;
    std::string kind; // "Initial", "Final", "Decision", "Action", "State", "Lifeline", "Actor", "Component", "Class", ...
    double x{0.0};
    double y{0.0};
    double width{160.0};
    double height{70.0};
};

struct SimEdge {
    int connectorId{0};
    int fromElementId{0};
    int toElementId{0};
    std::string fromNodeId;
    std::string toNodeId;
    std::string label;
    std::string kind;
    int sequenceNo{0};
};

struct SimTraceStep {
    int stepNumber{0};
    int elementId{0};
    std::string nodeId;
    std::string nodeName;
    std::string nodeKind;
    int connectorId{0};
    std::string logMessage;
    std::vector<std::pair<std::string, std::string>> variables;
};

struct SimulationPlan {
    int diagramId{0};
    std::string diagramName;
    DiagramType diagramType{DiagramType::Unknown};
    std::vector<SimNode> nodes;
    std::vector<SimEdge> edges;
    std::vector<SimTraceStep> steps;

    bool empty() const noexcept { return nodes.empty(); }
};

class SimulationEngine {
public:
    static SimulationPlan buildFromDiagram(Repository& repo, int diagramId);
};

} // namespace ea::simulation
