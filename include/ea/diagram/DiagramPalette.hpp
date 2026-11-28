#pragma once

// Contextual toolbox content (guiaSecuencia.md §1.1 "Toolbox por tipo de diagrama": only the elements
// and relationships that make sense on the active diagram). Pure data, shared by any front end.

#include <string>
#include <string_view>
#include <vector>
#include "ea/core/Types.hpp"
#include "ea/diagram/UmlDiagramKinds.hpp"

namespace ea {

struct PaletteTool {
    enum class Kind { Element, Relationship };

    std::string label;
    Kind kind{Kind::Element};
    ObjectType element{ObjectType::Unknown};          // when kind == Element
    ConnectorType relationship{ConnectorType::Unknown}; // when kind == Relationship
    std::string stereotype;                           // e.g. "include", "entity"
    int ntype{0}; // EA variant: fragment operator / pseudostate kind (elements), message sort (messages)

    bool isElement() const { return kind == Kind::Element; }
};

struct PaletteGroup {
    std::string title;
    std::vector<PaletteTool> tools;
};

struct Palette {
    std::string title;                // e.g. "Secuencia"
    std::vector<PaletteGroup> groups; // the last group is always "Comunes" (notes, text, note link)
};

// Palette for a UML diagram kind (see uml_diagram_kinds()); the general one for nullptr
const Palette& palette_for(const UmlDiagramKind* kind);
// Palette for an existing diagram; diagrams that are not one of the UML kinds get the general palette
const Palette& palette_for(DiagramType type, std::string_view stereotype);

} // namespace ea
