#include "ea/editor/FeatureText.hpp"
#include <algorithm>
#include <cctype>

namespace ea::editor {

namespace {

std::string_view trim(std::string_view s) {
    const auto isSpace = [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };
    while (!s.empty() && isSpace(s.front())) s.remove_prefix(1);
    while (!s.empty() && isSpace(s.back())) s.remove_suffix(1);
    return s;
}

std::string lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
    return out;
}

bool is_bound(std::string_view b) {
    if (b == "*") return true;
    return !b.empty() && std::all_of(b.begin(), b.end(), [](unsigned char c) { return std::isdigit(c); });
}

// Splits on commas that are not inside quotes (default values may contain commas)
std::vector<std::string_view> split_parameters(std::string_view text) {
    std::vector<std::string_view> parts;
    bool quoted = false;
    size_t start = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '"') quoted = !quoted;
        else if (text[i] == ',' && !quoted) {
            parts.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    parts.push_back(text.substr(start));
    return parts;
}

constexpr std::string_view kDirections[] = {"inout", "in", "out", "return"};

} // namespace

std::optional<Scope> parse_visibility(std::string_view text) {
    const std::string t = lower(trim(text));
    if (t == "+" || t == "public") return Scope::Public;
    if (t == "-" || t == "private") return Scope::Private;
    if (t == "#" || t == "protected") return Scope::Protected;
    if (t == "~" || t == "package") return Scope::Package;
    return std::nullopt;
}

std::string visibility_symbol(Scope scope) {
    switch (scope) {
        case Scope::Private: return "-";
        case Scope::Protected: return "#";
        case Scope::Package: return "~";
        default: return "+";
    }
}

std::optional<Multiplicity> parse_multiplicity(std::string_view text) {
    const auto t = trim(text);
    if (t.empty()) return Multiplicity{};
    const auto dots = t.find("..");
    if (dots == std::string_view::npos) {
        if (!is_bound(t)) return std::nullopt;
        if (t == "*") return Multiplicity{"0", "*"};
        return Multiplicity{std::string(t), std::string(t)};
    }
    const auto lo = trim(t.substr(0, dots));
    const auto up = trim(t.substr(dots + 2));
    if (!is_bound(lo) || lo == "*" || !is_bound(up)) return std::nullopt;
    if (up != "*" && std::stoul(std::string(lo)) > std::stoul(std::string(up))) return std::nullopt;
    return Multiplicity{std::string(lo), std::string(up)};
}

std::string format_multiplicity(const Multiplicity& m) {
    const std::string lo = m.lower.empty() ? "1" : m.lower;
    const std::string up = m.upper.empty() ? "1" : m.upper;
    if (lo == up) return lo;
    if (lo == "0" && up == "*") return "*";
    return lo + ".." + up;
}

ParameterParse parse_parameters(std::string_view text) {
    ParameterParse result;
    if (trim(text).empty()) return result;
    int pos = 0;
    for (auto part : split_parameters(text)) {
        part = trim(part);
        if (part.empty()) return {{}, "Hay un parámetro vacío (¿una coma de más?)."};
        Parameter p;
        p.pos = pos++;
        // direction
        for (auto dir : kDirections) {
            if (part.size() > dir.size() && lower(part.substr(0, dir.size())) == dir &&
                std::isspace(static_cast<unsigned char>(part[dir.size()]))) {
                p.kind = std::string(dir);
                part = trim(part.substr(dir.size()));
                break;
            }
        }
        // default value
        if (const auto eq = part.find('='); eq != std::string_view::npos) {
            p.default_value = std::string(trim(part.substr(eq + 1)));
            part = trim(part.substr(0, eq));
        }
        // name : type
        if (const auto colon = part.find(':'); colon != std::string_view::npos) {
            p.name = std::string(trim(part.substr(0, colon)));
            p.type = std::string(trim(part.substr(colon + 1)));
        } else {
            p.name = std::string(part);
        }
        if (!is_valid_feature_name(p.name) || p.name.find(' ') != std::string::npos) {
            return {{}, "Nombre de parámetro no válido: \"" + std::string(part) + "\" (formato: nombre: Tipo)."};
        }
        result.parameters.push_back(std::move(p));
    }
    return result;
}

std::string format_parameters(const std::vector<Parameter>& parameters) {
    std::string out;
    for (const auto& p : parameters) {
        if (!out.empty()) out += ", ";
        if (!p.kind.empty() && p.kind != "in") out += p.kind + " ";
        out += p.name;
        if (!p.type.empty()) out += ": " + p.type;
        if (!p.default_value.empty()) out += " = " + p.default_value;
    }
    return out;
}

bool is_valid_feature_name(std::string_view name) {
    const auto t = trim(name);
    return !t.empty() && t.find_first_of("\r\n") == std::string_view::npos && t.size() == name.size();
}

} // namespace ea::editor
