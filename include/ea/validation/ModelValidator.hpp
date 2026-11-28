#pragma once

#include <string>
#include <vector>
#include "ea/core/Collection.hpp"
#include "ea/model/Package.hpp"
#include "ea/validation/ConnectionRules.hpp"

namespace ea::validation {

enum class Severity { Error, Warning };

struct Issue {
    Severity severity{Severity::Error};
    std::string rule;      // stable identifier, e.g. "unique-name"
    std::string message;   // for the user
    int elementId{0};      // element the issue is about (0 if none)
    int connectorId{0};    // relationship the issue is about (0 if none)
};

struct ValidationReport {
    std::vector<Issue> issues;
    size_t elementsChecked{0};
    size_t connectorsChecked{0};

    size_t count(Severity severity) const;
    bool clean() const { return issues.empty(); }
};

// Integrity check of a whole model (guiaSecuencia.md §1.1 "Validación UML"): relationship rules,
// generalization cycles, multiplicities, unique names per namespace, unresolved relationship ends.
// One pass to index elements and connectors, then each rule runs over the index: O(elements + connectors).
// `resolve` finds elements that are not in the package tree (e.g. elements nested in other elements,
// which the tree does not load); without it such relationship ends are reported as unresolved.
ValidationReport validate_model(const Collection<Package>& models, const ElementLookup& resolve = {});

std::string to_string(Severity severity);

} // namespace ea::validation
