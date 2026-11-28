#pragma once

#include <cstddef>
#include <string>
#include "ea/repository/Repository.hpp"

namespace ea::editor {

struct PatternResult {
    bool ok{false};
    int packageId{0};
    int diagramId{0};
    size_t elementsCreated{0};
    size_t connectorsCreated{0};
    std::string error;
};

class ModelPatternService {
public:
    // Instantiates a real UML pattern (package + diagram + starter elements + connectors)
    // inside `parentPackageId` (or creates a root model if none exists).
    static PatternResult instantiatePattern(Repository& repo, int parentPackageId,
                                            const std::string& category, const std::string& patternName);
};

} // namespace ea::editor
