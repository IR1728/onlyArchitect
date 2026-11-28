#pragma once

// Text notation for features as typed in the Property Editor (UML 2.5 §9.5.4, §9.6.4):
//   visibility  "+" public, "-" private, "#" protected, "~" package (or the words)
//   multiplicity "1", "0..1", "*", "1..*"
//   parameters  "in a: int, out b: string = \"x\"" (direction optional, defaults to in)
// Pure functions, no UI: any front end parses and formats the same way.

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "ea/core/Types.hpp"
#include "ea/model/Method.hpp"

namespace ea::editor {

std::optional<Scope> parse_visibility(std::string_view text);
std::string visibility_symbol(Scope scope);

struct Multiplicity {
    std::string lower{"1"};
    std::string upper{"1"};
    bool operator==(const Multiplicity&) const = default;
};
std::optional<Multiplicity> parse_multiplicity(std::string_view text);
std::string format_multiplicity(const Multiplicity& m); // "1" for 1..1, "*" for 0..*

// Parsed parameter list, or the reason it is not valid
struct ParameterParse {
    std::vector<Parameter> parameters;
    std::string error; // empty when valid
    bool ok() const { return error.empty(); }
};
ParameterParse parse_parameters(std::string_view text);
std::string format_parameters(const std::vector<Parameter>& parameters);

// UML names in the Property Editor: not empty, no line breaks
bool is_valid_feature_name(std::string_view name);

} // namespace ea::editor
