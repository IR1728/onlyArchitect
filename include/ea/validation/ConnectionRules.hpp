#pragma once

// UML 2.x well-formedness rules for relationships (guiaSecuencia.md §1.1 "Validación UML",
// §3.2 "Al intentar crear una relación inválida ... no se crea la conexión").
//
// Pure functions over model entities: no persistence, no GUI. Element kinds the model does not
// recognise (ObjectType::Unknown, e.g. EA's Object, Part, Constraint) are given the benefit of
// the doubt, so models created by other tools are not flagged for what we cannot judge.

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "ea/core/Types.hpp"
#include "ea/model/Element.hpp"

namespace ea::validation {

struct RuleVerdict {
    bool allowed{true};
    std::string reason; // why it is not allowed, phrased for the user

    static RuleVerdict ok() { return {}; }
    static RuleVerdict violation(std::string why) { return {false, std::move(why)}; }
    explicit operator bool() const { return allowed; }
};

// Can a relationship of `type` go from `source` (client) to `target` (supplier)?
RuleVerdict check_connection(ConnectorType type, const Element& source, const Element& target);

using ElementLookup = std::function<std::shared_ptr<Element>(int elementId)>;

// Would a new generalization source -> target close a cycle in the inheritance hierarchy?
// Follows the Generalization connectors attached to each element.
bool creates_generalization_cycle(const Element& source, const Element& target, const ElementLookup& lookup);

// Everything a new relationship must satisfy: check_connection + no generalization cycles
RuleVerdict check_new_connection(ConnectorType type, const Element& source, const Element& target,
                                 const ElementLookup& lookup);

// UML multiplicity ("1", "0..1", "*", "1..*", "2..5"). Bounds that are expressions ("n", "[Max]")
// are accepted as such; only what is certainly wrong is rejected (e.g. "1..", "*..1", "5..2").
RuleVerdict check_multiplicity(const std::string& multiplicity);

} // namespace ea::validation
