#pragma once

#include <string>
#include <vector>
#include "ea/model/Attribute.hpp"
#include "ea/model/Element.hpp"
#include "ea/model/Method.hpp"

namespace ea::editor {

// Deep copy of the user-editable state of an element (scalar properties, attributes, operations).
struct ElementSnapshot {
    std::string name, alias, keywords, subtype, stereotype, notes, status, author, version;
    ObjectType type{ObjectType::Class};
    bool isAbstract{false};
    std::vector<Attribute> attributes;
    std::vector<Method> methods;

    static ElementSnapshot capture(const Element& elem);
    void restoreInto(Element& elem) const;
    bool operator==(const ElementSnapshot&) const = default;
};

} // namespace ea::editor
