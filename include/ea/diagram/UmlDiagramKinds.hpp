#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "ea/core/Types.hpp"

namespace ea {

enum class UmlDiagramCategory { Structural, Behavioral };

// One of the UML 2.x diagram kinds the editor can create, and how it is stored in t_diagram.
struct UmlDiagramKind {
    std::string key;          // stable identifier, e.g. "class", "sequence"
    std::string label;        // name shown to the user
    UmlDiagramCategory category;
    DiagramType type;         // t_diagram.Diagram_Type
    std::string stereotype;   // t_diagram.Stereotype; distinguishes kinds that share a Diagram_Type
};

// The 14 UML 2.x diagram kinds (7 structural, 7 behavioral), in presentation order
const std::vector<UmlDiagramKind>& uml_diagram_kinds();

const UmlDiagramKind* find_uml_diagram_kind(std::string_view key);
// Kind of an existing diagram; nullptr for non-UML types (Analysis, Custom, ...)
const UmlDiagramKind* find_uml_diagram_kind(DiagramType type, std::string_view stereotype);

std::string to_string(UmlDiagramCategory category);

} // namespace ea
