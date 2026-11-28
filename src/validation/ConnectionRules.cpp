#include "ea/validation/ConnectionRules.hpp"
#include "ea/diagram/SequenceModel.hpp"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <initializer_list>
#include <unordered_set>

namespace ea::validation {

namespace {

bool is_one_of(ObjectType t, std::initializer_list<ObjectType> kinds) {
    return std::find(kinds.begin(), kinds.end(), t) != kinds.end();
}

// Comments, not model elements: they can only be attached with a Note Link
bool is_annotation(ObjectType t) {
    return is_one_of(t, {ObjectType::Note, ObjectType::Text});
}

bool is_activity_node(ObjectType t) {
    // EA stores activity initial/final nodes as "StateNode", which loads as State
    return is_one_of(t, {ObjectType::Activity, ObjectType::Action, ObjectType::State, ObjectType::InitialNode, ObjectType::FinalState,
                         ObjectType::Decision, ObjectType::MergeNode, ObjectType::ForkNode, ObjectType::JoinNode,
                         ObjectType::ObjectNode, ObjectType::Unknown});
}

bool is_state_vertex(ObjectType t) {
    return is_one_of(t, {ObjectType::State, ObjectType::StateMachine, ObjectType::InitialNode, ObjectType::FinalState,
                         ObjectType::Decision, ObjectType::MergeNode, ObjectType::ForkNode, ObjectType::JoinNode,
                         ObjectType::Unknown});
}

// Not Classifiers in UML: they cannot take part in generalizations or associations
bool is_non_classifier(ObjectType t) {
    return is_one_of(t, {ObjectType::Package, ObjectType::Action, ObjectType::InitialNode, ObjectType::FinalState,
                         ObjectType::Decision, ObjectType::MergeNode, ObjectType::ForkNode, ObjectType::JoinNode,
                         ObjectType::ObjectNode, ObjectType::State, ObjectType::Port});
}

// Stereotypes of the UML standard profile (and EA's robustness ones): the element keeps its UML meaning
bool is_standard_stereotype(const std::string& st) {
    static const std::unordered_set<std::string> standard = {
        "auxiliary", "boundary", "buildcomponent", "call", "control", "create", "derive", "destroy", "document",
        "entity", "executable", "file", "focus", "framework", "implement", "implementationclass", "instantiate",
        "library", "metaclass", "metamodel", "modellibrary", "process", "realization", "refine", "responsibility",
        "script", "send", "service", "source", "specification", "subsystem", "systemmodel", "trace", "type", "utility"};
    std::string lower(st);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return standard.count(lower) > 0;
}

// Kind used by the rules. An element extended by a profile/MDG technology (ArchiMate, BPMN, BMM, ...)
// has semantics UML alone cannot judge, so it is treated like an unrecognised kind.
ObjectType type_of(const Element& e) {
    const auto& st = e.GetStereotype();
    if (!st.empty() && !is_standard_stereotype(st)) return ObjectType::Unknown;
    return e.GetType();
}

std::string kind(const Element& e) {
    return to_string(e.GetType());
}

std::string ends(const Element& source, const Element& target) {
    return kind(source) + " → " + kind(target);
}

RuleVerdict check_generalization(const Element& source, const Element& target) {
    if (source.GetElementID() != 0 && source.GetElementID() == target.GetElementID()) {
        return RuleVerdict::violation("Un elemento no puede generalizarse a sí mismo.");
    }
    const auto s = type_of(source);
    const auto t = type_of(target);
    if (s == ObjectType::Unknown || t == ObjectType::Unknown) return RuleVerdict::ok();
    if (is_non_classifier(s) || is_non_classifier(t)) {
        return RuleVerdict::violation("La generalización solo une clasificadores; " + ends(source, target) + " no lo son.");
    }
    if (s != t) {
        std::string why = "Una generalización solo puede unir elementos del mismo tipo (" + ends(source, target) + ").";
        if (t == ObjectType::Interface || s == ObjectType::Interface) {
            why += " Para que una clase implemente una interfaz usa Realización.";
        }
        return RuleVerdict::violation(why);
    }
    return RuleVerdict::ok();
}

RuleVerdict check_realization(const Element& source, const Element& target) {
    if (source.GetElementID() != 0 && source.GetElementID() == target.GetElementID()) {
        return RuleVerdict::violation("Un elemento no puede realizarse a sí mismo.");
    }
    if (type_of(source) == ObjectType::Interface) {
        return RuleVerdict::violation(
            "Una interfaz no realiza otros elementos; para extender una interfaz usa Generalización.");
    }
    // Otherwise any pair of named elements: UML Realization is an Abstraction between NamedElements
    return RuleVerdict::ok();
}

// Association, Aggregation and Composition
RuleVerdict check_association(ConnectorType type, const Element& source, const Element& target) {
    const auto s = type_of(source);
    const auto t = type_of(target);
    const std::string name = to_string(type);
    // Ports are connectable: EA models links between ports (e.g. communication paths) as associations
    const auto cannot_associate = [](ObjectType k) { return k != ObjectType::Port && is_non_classifier(k); };
    if (cannot_associate(s) || cannot_associate(t)) {
        return RuleVerdict::violation("La " + name + " une clasificadores; " + ends(source, target) + " no es válida.");
    }
    if (s == ObjectType::Actor && t == ObjectType::Actor) {
        return RuleVerdict::violation("Dos actores no pueden asociarse entre sí; usa Generalización si uno especializa al otro.");
    }
    if (s == ObjectType::UseCase && t == ObjectType::UseCase) {
        return RuleVerdict::violation("Dos casos de uso no se asocian; relaciónalos con «include», «extend» o Generalización.");
    }
    // UML: an Actor can only have Associations to UseCases, Components and Classes
    for (const auto& [actor, other] : {std::pair{&source, &target}, std::pair{&target, &source}}) {
        if (type_of(*actor) == ObjectType::Actor &&
            !is_one_of(type_of(*other), {ObjectType::UseCase, ObjectType::Component, ObjectType::Class,
                                          ObjectType::Unknown})) {
            return RuleVerdict::violation("Un actor solo puede asociarse con casos de uso, componentes o clases, no con " +
                                          kind(*other) + ".");
        }
    }
    if (type != ConnectorType::Association &&
        (is_one_of(s, {ObjectType::Actor, ObjectType::UseCase}) || is_one_of(t, {ObjectType::Actor, ObjectType::UseCase}))) {
        return RuleVerdict::violation("Actores y casos de uso no forman relaciones todo-parte (" + name + ").");
    }
    return RuleVerdict::ok();
}

RuleVerdict check_flow(ConnectorType type, const Element& source, const Element& target) {
    if (!is_activity_node(type_of(source)) || !is_activity_node(type_of(target))) {
        return RuleVerdict::violation("Un " + to_string(type) + " solo une nodos de actividad (" + ends(source, target) + ").");
    }
    if (type_of(target) == ObjectType::InitialNode) {
        return RuleVerdict::violation("Ningún flujo puede llegar a un nodo inicial.");
    }
    return RuleVerdict::ok();
}

RuleVerdict check_transition(const Element& source, const Element& target) {
    if (!is_state_vertex(type_of(source)) || !is_state_vertex(type_of(target))) {
        return RuleVerdict::violation("Una transición solo une estados y pseudoestados (" + ends(source, target) + ").");
    }
    if (type_of(target) == ObjectType::InitialNode) {
        return RuleVerdict::violation("Ninguna transición puede llegar a un estado inicial.");
    }
    if (type_of(source) == ObjectType::FinalState) {
        return RuleVerdict::violation("Ninguna transición puede salir de un estado final.");
    }
    return RuleVerdict::ok();
}

RuleVerdict check_component_wiring(ConnectorType type, const Element& source, const Element& target) {
    for (const Element* e : {&source, &target}) {
        if (!is_one_of(type_of(*e), {ObjectType::Component, ObjectType::Port, ObjectType::Interface, ObjectType::Class,
                                      ObjectType::Unknown})) {
            return RuleVerdict::violation("Un conector " + to_string(type) + " une componentes, puertos o interfaces, no " +
                                          kind(*e) + ".");
        }
    }
    return RuleVerdict::ok();
}

// Interaction messages run between lifelines (or gates on fragments / interaction uses)
RuleVerdict check_message(const Element& source, const Element& target) {
    const auto takesMessages = [](ObjectType k) {
        return sequence::is_lifeline_kind(k) || sequence::is_interaction_frame(k) || k == ObjectType::Unknown;
    };
    if (!takesMessages(type_of(source)) || !takesMessages(type_of(target))) {
        return RuleVerdict::violation("Un mensaje solo une líneas de vida (" + ends(source, target) + ").");
    }
    return RuleVerdict::ok();
}

// «include» / «extend» (EA Connector_Type "UseCase")
RuleVerdict check_use_case_link(const Element& source, const Element& target) {
    const auto isUseCase = [](ObjectType k) { return k == ObjectType::UseCase || k == ObjectType::Unknown; };
    if (!isUseCase(type_of(source)) || !isUseCase(type_of(target))) {
        return RuleVerdict::violation("«include» y «extend» solo unen casos de uso (" + ends(source, target) + ").");
    }
    if (source.GetElementID() != 0 && source.GetElementID() == target.GetElementID()) {
        return RuleVerdict::violation("Un caso de uso no puede incluirse ni extenderse a sí mismo.");
    }
    return RuleVerdict::ok();
}

RuleVerdict check_package_relation(ConnectorType type, const Element& source, const Element& target) {
    if (type_of(source) != ObjectType::Package || type_of(target) != ObjectType::Package) {
        return RuleVerdict::violation(to_string(type) + " solo une paquetes (" + ends(source, target) + ").");
    }
    return RuleVerdict::ok();
}

bool parse_bound(std::string_view text, long& value) {
    auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    return ec == std::errc() && ptr == text.data() + text.size();
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
    while (!s.empty() && s.back() == ' ') s.remove_suffix(1);
    return s;
}

} // namespace

RuleVerdict check_connection(ConnectorType type, const Element& source, const Element& target) {
    const bool annotated = is_annotation(type_of(source)) || is_annotation(type_of(target));
    if (type == ConnectorType::NoteLink) {
        const auto attachable = [](ObjectType t) { return is_annotation(t) || t == ObjectType::Unknown; };
        if (!attachable(type_of(source)) && !attachable(type_of(target))) {
            return RuleVerdict::violation("Un Note Link debe tener una nota en uno de sus extremos.");
        }
        return RuleVerdict::ok();
    }
    if (annotated) {
        return RuleVerdict::violation("Las notas no participan en relaciones UML; únelas con un Note Link.");
    }

    switch (type) {
        case ConnectorType::Generalization: return check_generalization(source, target);
        case ConnectorType::Realization: return check_realization(source, target);
        case ConnectorType::Association:
        case ConnectorType::Aggregation:
        case ConnectorType::Composition: return check_association(type, source, target);
        case ConnectorType::ControlFlow:
        case ConnectorType::ObjectFlow: return check_flow(type, source, target);
        case ConnectorType::Transition: return check_transition(source, target);
        case ConnectorType::Assembly:
        case ConnectorType::Delegate: return check_component_wiring(type, source, target);
        case ConnectorType::PackageImport:
        case ConnectorType::PackageMerge: return check_package_relation(type, source, target);
        case ConnectorType::UseCaseLink: return check_use_case_link(source, target);
        case ConnectorType::Message: return check_message(source, target);
        default: return RuleVerdict::ok(); // Dependency, Usage, Abstraction, InformationFlow, Message, ...
    }
}

bool creates_generalization_cycle(const Element& source, const Element& target, const ElementLookup& lookup) {
    // The new edge source -> target closes a cycle iff source is already an ancestor-or-self of target
    const int sourceId = source.GetElementID();
    std::vector<int> pending{target.GetElementID()};
    std::unordered_set<int> visited;
    while (!pending.empty()) {
        const int current = pending.back();
        pending.pop_back();
        if (current == sourceId) return true;
        if (!visited.insert(current).second) continue;
        auto elem = lookup(current);
        if (!elem) continue;
        for (const auto& conn : elem->GetConnectors()) {
            if (conn && conn->GetType() == ConnectorType::Generalization && conn->GetClientID() == current) {
                pending.push_back(conn->GetSupplierID()); // parent
            }
        }
    }
    return false;
}

RuleVerdict check_new_connection(ConnectorType type, const Element& source, const Element& target,
                                 const ElementLookup& lookup) {
    auto verdict = check_connection(type, source, target);
    if (!verdict) return verdict;
    if (type == ConnectorType::Generalization && creates_generalization_cycle(source, target, lookup)) {
        return RuleVerdict::violation("Esta generalización crearía un ciclo de herencia (" + target.GetName() +
                                      " ya hereda de " + source.GetName() + ").");
    }
    return RuleVerdict::ok();
}

RuleVerdict check_multiplicity(const std::string& multiplicity) {
    const std::string_view text = trim(multiplicity);
    if (text.empty() || text == "*") return RuleVerdict::ok();

    const auto dots = text.find("..");
    const std::string_view lowerText = trim(dots == std::string_view::npos ? text : text.substr(0, dots));
    const std::string_view upperText = dots == std::string_view::npos ? lowerText : trim(text.substr(dots + 2));
    const auto malformed = [&](const std::string& why) {
        return RuleVerdict::violation("Multiplicidad \"" + multiplicity + "\" no válida: " + why);
    };

    if (lowerText.empty() || upperText.empty()) return malformed("falta un límite.");
    if (lowerText == "*") return malformed("el límite inferior no puede ser *.");

    long lower = 0;
    long upper = 0;
    const bool lowerIsNumber = parse_bound(lowerText, lower);
    const bool upperIsNumber = parse_bound(upperText, upper);
    if ((lowerIsNumber && lower < 0) || (upperIsNumber && upper < 0) ||
        lowerText.front() == '-' || upperText.front() == '-') {
        return malformed("los límites no pueden ser negativos.");
    }
    if (lowerIsNumber && upperIsNumber && upper < lower) return malformed("el límite superior es menor que el inferior.");
    return RuleVerdict::ok(); // numeric, "*" upper, or expressions such as "n" / "[Max]"
}

} // namespace ea::validation
