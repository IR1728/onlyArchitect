#include "ea/simulation/SimulationEngine.hpp"
#include <algorithm>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace ea::simulation {

namespace {

bool isDecorative(ObjectType t) {
    return t == ObjectType::Note || t == ObjectType::Text || t == ObjectType::Boundary;
}

std::string simKindFor(ObjectType t) {
    switch (t) {
        case ObjectType::InitialNode: return "Initial";
        case ObjectType::FinalState: return "Final";
        case ObjectType::Decision:
        case ObjectType::MergeNode: return "Decision";
        case ObjectType::State:
        case ObjectType::StateMachine: return "State";
        case ObjectType::Action:
        case ObjectType::Activity: return "Action";
        default: return to_string(t);
    }
}

std::string nodeIdFor(int elementId) {
    return "elem_" + std::to_string(elementId);
}

std::string edgeDisplayLabel(const Connector& c) {
    if (!c.GetName().empty()) return c.GetName();
    if (!c.GetStereotype().empty()) return "<<" + c.GetStereotype() + ">>";
    return to_string(c.GetType());
}

} // namespace

SimulationPlan SimulationEngine::buildFromDiagram(Repository& repo, int diagramId) {
    SimulationPlan plan;
    if (!repo.IsOpen() || diagramId <= 0) return plan;

    auto diagram = repo.GetDiagramByID(diagramId);
    if (!diagram) return plan;

    plan.diagramId = diagram->GetDiagramID();
    plan.diagramName = diagram->GetName();
    plan.diagramType = diagram->GetType();

    std::unordered_map<int, size_t> nodeIndexByElem;
    std::unordered_map<int, std::shared_ptr<Element>> elemById;

    for (const auto& dobj : diagram->GetDiagramObjects()) {
        if (!dobj) continue;
        auto elem = repo.GetElementByID(dobj->GetElementID());
        if (!elem || isDecorative(elem->GetType())) continue;

        if (nodeIndexByElem.count(elem->GetElementID())) continue;

        SimNode node;
        node.elementId = elem->GetElementID();
        node.id = nodeIdFor(node.elementId);
        node.name = elem->GetName().empty() ? to_string(elem->GetType()) : elem->GetName();
        node.kind = simKindFor(elem->GetType());
        node.x = static_cast<double>(dobj->GetLeft());
        node.y = static_cast<double>(-dobj->GetTop());
        node.width = std::max(44.0, static_cast<double>(dobj->GetWidth()));
        node.height = std::max(36.0, static_cast<double>(dobj->GetHeight()));

        if (node.kind == "Initial" || node.kind == "Final") {
            node.width = std::min(node.width, 40.0);
            node.height = std::min(node.height, 40.0);
        }

        nodeIndexByElem[node.elementId] = plan.nodes.size();
        elemById[node.elementId] = elem;
        plan.nodes.push_back(std::move(node));
    }

    if (plan.nodes.empty()) return plan;

    // Normalize coordinates so the simulation scene starts cleanly near (40, 40)
    double minX = plan.nodes.front().x;
    double minY = plan.nodes.front().y;
    for (const auto& n : plan.nodes) {
        minX = std::min(minX, n.x);
        minY = std::min(minY, n.y);
    }
    for (auto& n : plan.nodes) {
        n.x = (n.x - minX) + 40.0;
        n.y = (n.y - minY) + 40.0;
    }

    std::unordered_set<int> seenConn;
    for (const auto& link : diagram->GetDiagramLinks()) {
        if (!link || link->IsHidden()) continue;
        auto conn = repo.GetConnectorByID(link->GetConnectorID());
        if (!conn || !seenConn.insert(conn->GetConnectorID()).second) continue;
        if (!nodeIndexByElem.count(conn->GetClientID()) || !nodeIndexByElem.count(conn->GetSupplierID())) continue;

        SimEdge edge;
        edge.connectorId = conn->GetConnectorID();
        edge.fromElementId = conn->GetClientID();
        edge.toElementId = conn->GetSupplierID();
        edge.fromNodeId = nodeIdFor(edge.fromElementId);
        edge.toNodeId = nodeIdFor(edge.toElementId);
        edge.label = edgeDisplayLabel(*conn);
        edge.kind = to_string(conn->GetType());
        edge.sequenceNo = conn->GetSequenceNo();
        plan.edges.push_back(std::move(edge));
    }

    // Build deterministic execution trace
    if (plan.diagramType == DiagramType::Sequence && !plan.edges.empty()) {
        std::vector<SimEdge> ordered = plan.edges;
        std::stable_sort(ordered.begin(), ordered.end(), [](const SimEdge& a, const SimEdge& b) {
            if (a.sequenceNo != b.sequenceNo) return a.sequenceNo < b.sequenceNo;
            return a.connectorId < b.connectorId;
        });

        int stepNo = 0;
        for (const auto& e : ordered) {
            const auto& src = plan.nodes[nodeIndexByElem[e.fromElementId]];
            const auto& dst = plan.nodes[nodeIndexByElem[e.toElementId]];
            SimTraceStep step;
            step.stepNumber = ++stepNo;
            step.elementId = dst.elementId;
            step.nodeId = dst.id;
            step.nodeName = dst.name;
            step.nodeKind = dst.kind;
            step.connectorId = e.connectorId;
            step.logMessage = src.name + " -> " + dst.name + ": " + e.label;
            step.variables = {
                {"step", std::to_string(stepNo) + " / " + std::to_string(ordered.size())},
                {"sender", src.name},
                {"receiver", dst.name},
                {"message", e.label},
                {"connector_type", e.kind},
            };
            plan.steps.push_back(std::move(step));
        }
        return plan;
    }

    // Graph traversal for Activity, StateMachine, Component, Class, UseCase, Deployment
    std::unordered_map<int, std::vector<const SimEdge*>> outgoing;
    std::unordered_map<int, int> inDegree;
    for (const auto& n : plan.nodes) inDegree[n.elementId] = 0;
    for (const auto& e : plan.edges) {
        outgoing[e.fromElementId].push_back(&e);
        if (e.fromElementId != e.toElementId) inDegree[e.toElementId]++;
    }

    std::vector<int> startOrder;
    // 1. Explicit InitialNode first
    for (const auto& n : plan.nodes) {
        if (n.kind == "Initial") startOrder.push_back(n.elementId);
    }
    // 2. Nodes with in-degree 0 (left-to-right, top-to-bottom)
    std::vector<const SimNode*> zeroIn;
    for (const auto& n : plan.nodes) {
        if (n.kind != "Initial" && inDegree[n.elementId] == 0) zeroIn.push_back(&n);
    }
    std::sort(zeroIn.begin(), zeroIn.end(), [](const SimNode* a, const SimNode* b) {
        if (std::abs(a->x - b->x) > 40.0) return a->x < b->x;
        return a->y < b->y;
    });
    for (const auto* n : zeroIn) startOrder.push_back(n->elementId);
    // 3. Remaining nodes (handles cycles or disconnected components)
    for (const auto& n : plan.nodes) startOrder.push_back(n.elementId);

    std::unordered_set<int> visited;
    std::queue<std::pair<int, const SimEdge*>> q;
    int stepNo = 0;

    for (int seed : startOrder) {
        if (visited.count(seed)) continue;
        visited.insert(seed);
        q.push({seed, nullptr});

        while (!q.empty()) {
            auto [currId, viaEdge] = q.front();
            q.pop();

            const auto& node = plan.nodes[nodeIndexByElem[currId]];
            const auto& elem = elemById[currId];
            SimTraceStep step;
            step.stepNumber = ++stepNo;
            step.elementId = currId;
            step.nodeId = node.id;
            step.nodeName = node.name;
            step.nodeKind = node.kind;
            step.connectorId = viaEdge ? viaEdge->connectorId : 0;

            if (viaEdge) {
                const auto& prevNode = plan.nodes[nodeIndexByElem[viaEdge->fromElementId]];
                step.logMessage = prevNode.name + " --[" + viaEdge->label + "]--> " + node.name;
            } else {
                step.logMessage = "Activando [" + node.kind + "] " + node.name;
            }

            step.variables = {
                {"active_node", node.name},
                {"element_type", node.kind},
                {"status", elem ? elem->GetStatus() : ""},
            };
            if (elem && !elem->GetStereotype().empty()) {
                step.variables.emplace_back("stereotype", "<<" + elem->GetStereotype() + ">>");
            }
            if (viaEdge) {
                step.variables.emplace_back("last_transition", viaEdge->label);
            }
            plan.steps.push_back(std::move(step));

            for (const SimEdge* outEdge : outgoing[currId]) {
                if (!visited.count(outEdge->toElementId)) {
                    visited.insert(outEdge->toElementId);
                    q.push({outEdge->toElementId, outEdge});
                }
            }
        }
    }

    return plan;
}

} // namespace ea::simulation
