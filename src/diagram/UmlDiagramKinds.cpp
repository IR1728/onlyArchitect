#include "ea/diagram/UmlDiagramKinds.hpp"

namespace ea {

const std::vector<UmlDiagramKind>& uml_diagram_kinds() {
    using C = UmlDiagramCategory;
    static const std::vector<UmlDiagramKind> kinds = {
        {"class",               "Clases",               C::Structural, DiagramType::Logical,             ""},
        {"object",              "Objetos",              C::Structural, DiagramType::Object,              ""},
        {"component",           "Componentes",          C::Structural, DiagramType::Component,           ""},
        {"deployment",          "Despliegue",           C::Structural, DiagramType::Deployment,          ""},
        {"package",             "Paquetes",             C::Structural, DiagramType::Package,             ""},
        {"composite_structure", "Estructura compuesta", C::Structural, DiagramType::CompositeStructure,  ""},
        // EA has no Diagram_Type of its own for profiles: a package diagram marked «profile»
        {"profile",             "Perfil",               C::Structural, DiagramType::Package,             "profile"},

        {"use_case",            "Casos de uso",         C::Behavioral, DiagramType::UseCase,             ""},
        {"sequence",            "Secuencia",            C::Behavioral, DiagramType::Sequence,            ""},
        {"communication",       "Comunicación",         C::Behavioral, DiagramType::Communication,       ""},
        {"activity",            "Actividad",            C::Behavioral, DiagramType::Activity,            ""},
        {"state_machine",       "Máquina de estados",   C::Behavioral, DiagramType::Statechart,          ""},
        {"timing",              "Timing",               C::Behavioral, DiagramType::Timing,              ""},
        {"interaction_overview","Interaction Overview", C::Behavioral, DiagramType::InteractionOverview, ""},
    };
    return kinds;
}

const UmlDiagramKind* find_uml_diagram_kind(std::string_view key) {
    for (const auto& kind : uml_diagram_kinds()) {
        if (kind.key == key) return &kind;
    }
    return nullptr;
}

const UmlDiagramKind* find_uml_diagram_kind(DiagramType type, std::string_view stereotype) {
    const UmlDiagramKind* plain = nullptr;
    for (const auto& kind : uml_diagram_kinds()) {
        if (kind.type != type) continue;
        if (!kind.stereotype.empty() && kind.stereotype == stereotype) return &kind;
        if (kind.stereotype.empty() && !plain) plain = &kind;
    }
    return plain;
}

std::string to_string(UmlDiagramCategory category) {
    return category == UmlDiagramCategory::Structural ? "Estructurales" : "De comportamiento";
}

} // namespace ea
