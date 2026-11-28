#include "ea/validation/ModelValidator.hpp"
#include "ea/validation/ConnectionRules.hpp"
#include <algorithm>
#include <map>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace ea::validation {

namespace {

struct ModelIndex {
    std::unordered_map<int, std::shared_ptr<Element>> elements;
    std::vector<std::shared_ptr<Connector>> connectors; // each one once
};

void index_element(const std::shared_ptr<Element>& elem, ModelIndex& index, std::unordered_set<int>& seenConnectors) {
    if (!elem || !index.elements.emplace(elem->GetElementID(), elem).second) return;
    for (const auto& conn : elem->GetConnectors()) {
        if (conn && seenConnectors.insert(conn->GetConnectorID()).second) index.connectors.push_back(conn);
    }
    for (const auto& child : elem->GetElements()) index_element(child, index, seenConnectors);
}

void index_package(const Package& pkg, ModelIndex& index, std::unordered_set<int>& seenConnectors) {
    for (const auto& elem : pkg.GetElements()) index_element(elem, index, seenConnectors);
    for (const auto& sub : pkg.GetPackages()) {
        if (sub) index_package(*sub, index, seenConnectors);
    }
}

std::string describe(const Element& e) {
    return to_string(e.GetType()) + " \"" + e.GetName() + "\"";
}

// Kinds whose names carry no identity (comments, control nodes), or that we do not recognise
bool name_is_optional(ObjectType t) {
    switch (t) {
        case ObjectType::Note: case ObjectType::Text: case ObjectType::Boundary:
        case ObjectType::InitialNode: case ObjectType::FinalState: case ObjectType::Decision:
        case ObjectType::MergeNode: case ObjectType::ForkNode: case ObjectType::JoinNode:
        case ObjectType::Unknown:
            return true;
        default:
            return false;
    }
}

// UML: members of a namespace must be distinguishable, i.e. differ in name or in kind.
// A warning, not an error: EA allows it and models use it on purpose (e.g. several "Desktop" nodes).
void check_unique_names(const ModelIndex& index, std::vector<Issue>& out) {
    using Key = std::tuple<int, int, ObjectType, std::string>; // package, owner element, kind, name
    std::map<Key, int> firstWithName;
    for (const auto& [id, elem] : index.elements) {
        if (name_is_optional(elem->GetType())) continue;
        if (elem->GetName().empty()) {
            out.push_back({Severity::Warning, "missing-name", describe(*elem) + " no tiene nombre.", id, 0});
            continue;
        }
        Key key{elem->GetPackageID(), elem->GetParentID(), elem->GetType(), elem->GetName()};
        auto [it, inserted] = firstWithName.emplace(key, id);
        if (!inserted) {
            out.push_back({Severity::Warning, "unique-name",
                           "Nombre repetido en el mismo espacio de nombres: " + describe(*elem) +
                               " (también el elemento " + std::to_string(it->second) + ").",
                           id, 0});
        }
    }
}

// Brings into the index the relationship ends that live outside the package tree
void resolve_connector_ends(ModelIndex& index, const ElementLookup& resolve) {
    if (!resolve) return;
    for (const auto& conn : index.connectors) {
        for (int endId : {conn->GetClientID(), conn->GetSupplierID()}) {
            if (index.elements.count(endId)) continue;
            if (auto elem = resolve(endId)) index.elements.emplace(endId, std::move(elem));
        }
    }
}

void check_connectors(const ModelIndex& index, std::vector<Issue>& out) {
    for (const auto& conn : index.connectors) {
        auto source = index.elements.find(conn->GetClientID());
        auto target = index.elements.find(conn->GetSupplierID());
        if (source == index.elements.end() || target == index.elements.end()) {
            out.push_back({Severity::Warning, "unresolved-end",
                           to_string(conn->GetType()) + " " + std::to_string(conn->GetConnectorID()) +
                               " apunta a un elemento que no está en el modelo.",
                           0, conn->GetConnectorID()});
            continue;
        }
        if (auto verdict = check_connection(conn->GetType(), *source->second, *target->second); !verdict) {
            out.push_back({Severity::Error, "relationship-rule",
                           describe(*source->second) + " → " + describe(*target->second) + ": " + verdict.reason,
                           source->second->GetElementID(), conn->GetConnectorID()});
        }
        for (const auto* mult : {&conn->GetClientMultiplicity(), &conn->GetSupplierMultiplicity()}) {
            if (auto verdict = check_multiplicity(*mult); !verdict) {
                out.push_back({Severity::Error, "multiplicity", verdict.reason + " (" + to_string(conn->GetType()) + " " +
                                                                    std::to_string(conn->GetConnectorID()) + ")",
                               source->second->GetElementID(), conn->GetConnectorID()});
            }
        }
    }
}

// Iterative DFS with three colours over the Generalization edges: a back edge is a cycle
void check_generalization_cycles(const ModelIndex& index, std::vector<Issue>& out) {
    std::unordered_map<int, std::vector<int>> parents;
    for (const auto& conn : index.connectors) {
        if (conn->GetType() == ConnectorType::Generalization) parents[conn->GetClientID()].push_back(conn->GetSupplierID());
    }
    enum class Colour { White, Grey, Black };
    std::unordered_map<int, Colour> colour;
    colour.reserve(parents.size());

    for (const auto& [start, _] : parents) {
        if (colour[start] != Colour::White) continue;
        std::vector<std::pair<int, size_t>> stack{{start, 0}}; // node, next parent to visit
        colour[start] = Colour::Grey;
        while (!stack.empty()) {
            auto& [node, next] = stack.back();
            const auto it = parents.find(node);
            if (it == parents.end() || next >= it->second.size()) {
                colour[node] = Colour::Black;
                stack.pop_back();
                continue;
            }
            const int parent = it->second[next++];
            const Colour c = colour[parent];
            if (c == Colour::Grey) {
                auto elem = index.elements.find(parent);
                const std::string name = elem != index.elements.end() ? describe(*elem->second) : std::to_string(parent);
                out.push_back({Severity::Error, "generalization-cycle",
                               "Ciclo de herencia: " + name + " termina heredando de sí mismo.", parent, 0});
            } else if (c == Colour::White) {
                colour[parent] = Colour::Grey;
                stack.emplace_back(parent, 0);
            }
        }
    }
}

} // namespace

size_t ValidationReport::count(Severity severity) const {
    return static_cast<size_t>(std::count_if(issues.begin(), issues.end(),
                                             [severity](const Issue& i) { return i.severity == severity; }));
}

ValidationReport validate_model(const Collection<Package>& models, const ElementLookup& resolve) {
    ModelIndex index;
    std::unordered_set<int> seenConnectors;
    for (const auto& model : models) {
        if (model) index_package(*model, index, seenConnectors);
    }
    const size_t treeElements = index.elements.size();
    resolve_connector_ends(index, resolve);

    ValidationReport report;
    report.elementsChecked = treeElements;
    report.connectorsChecked = index.connectors.size();
    check_connectors(index, report.issues);
    check_generalization_cycles(index, report.issues);
    check_unique_names(index, report.issues);
    // Errors first, then by element for stable, readable output
    std::stable_sort(report.issues.begin(), report.issues.end(), [](const Issue& a, const Issue& b) {
        return std::tie(a.severity, a.elementId, a.connectorId) < std::tie(b.severity, b.elementId, b.connectorId);
    });
    return report;
}

std::string to_string(Severity severity) {
    return severity == Severity::Error ? "Error" : "Advertencia";
}

} // namespace ea::validation
