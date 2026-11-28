#include "ea/diagram/DiagramPalette.hpp"
#include "ea/diagram/SequenceModel.hpp"
#include <map>

namespace ea {

namespace {

using OT = ObjectType;
using CT = ConnectorType;

PaletteTool el(std::string label, OT type, std::string stereotype = "", int ntype = 0) {
    return {std::move(label), PaletteTool::Kind::Element, type, CT::Unknown, std::move(stereotype), ntype};
}

PaletteTool rel(std::string label, CT type, std::string stereotype = "") {
    return {std::move(label), PaletteTool::Kind::Relationship, OT::Unknown, type, std::move(stereotype), 0};
}

// Sequence message tool: the sort travels in `ntype`
PaletteTool msg(std::string label, sequence::MessageSort sort) {
    auto tool = rel(std::move(label), CT::Message);
    tool.ntype = static_cast<int>(sort);
    return tool;
}

// EA NType codes (t_object.NType)
namespace ntype {
constexpr int StateInitial = 3, StateFinal = 4, ActivityInitial = 100, ActivityFinal = 101, FlowFinal = 102;
constexpr int Alt = 0, Opt = 1, Break = 2, Par = 3, Loop = 4;
} // namespace ntype

PaletteGroup common() {
    return {"Comunes", {el("Note", OT::Note), el("Text", OT::Text), rel("Note Link", CT::NoteLink)}};
}

Palette make(std::string title, std::vector<PaletteGroup> groups) {
    groups.push_back(common());
    return {std::move(title), std::move(groups)};
}

const std::map<std::string, Palette>& palettes() {
    static const std::map<std::string, Palette> all = {
        {"class", make("Clases", {
            {"Clases", {el("Class", OT::Class), el("Interface", OT::Interface), el("Enumeration", OT::Enumeration),
                        el("Data Type", OT::DataType), el("Package", OT::Package)}},
            {"Relaciones", {rel("Association", CT::Association), rel("Aggregation", CT::Aggregation),
                            rel("Composition", CT::Composition), rel("Generalization", CT::Generalization),
                            rel("Realization", CT::Realization), rel("Dependency", CT::Dependency)}}})},
        {"object", make("Objetos", {
            {"Objetos", {el("Object", OT::Object), el("Class", OT::Class)}},
            {"Relaciones", {rel("Link", CT::Association), rel("Dependency", CT::Dependency)}}})},
        {"component", make("Componentes", {
            {"Componentes", {el("Component", OT::Component), el("Interface", OT::Interface), el("Port", OT::Port),
                             el("Class", OT::Class), el("Artifact", OT::Artifact)}},
            {"Relaciones", {rel("Assembly", CT::Assembly), rel("Delegate", CT::Delegate),
                            rel("Realization", CT::Realization), rel("Dependency", CT::Dependency),
                            rel("Association", CT::Association)}}})},
        {"deployment", make("Despliegue", {
            {"Despliegue", {el("Node", OT::Node), el("Device", OT::Device),
                            el("Execution Environment", OT::ExecutionEnvironment), el("Artifact", OT::Artifact),
                            el("Component", OT::Component)}},
            {"Relaciones", {rel("Communication Path", CT::Association), rel("Deployment", CT::Dependency, "deploy"),
                            rel("Manifestation", CT::Dependency, "manifest")}}})},
        {"package", make("Paquetes", {
            {"Paquetes", {el("Package", OT::Package), el("Class", OT::Class), el("Interface", OT::Interface)}},
            {"Relaciones", {rel("Import", CT::PackageImport), rel("Merge", CT::PackageMerge),
                            rel("Dependency", CT::Dependency), rel("Nesting", CT::Nesting)}}})},
        {"composite_structure", make("Estructura compuesta", {
            {"Estructura", {el("Class", OT::Class), el("Port", OT::Port), el("Interface", OT::Interface),
                            el("Component", OT::Component)}},
            {"Relaciones", {rel("Connector", CT::Association), rel("Assembly", CT::Assembly),
                            rel("Delegate", CT::Delegate)}}})},
        {"profile", make("Perfil", {
            {"Perfil", {el("Profile", OT::Package, "profile"), el("Stereotype", OT::Class, "stereotype"),
                        el("Metaclass", OT::Class, "metaclass")}},
            {"Relaciones", {rel("Extension", CT::Association, "extends"), rel("Generalization", CT::Generalization)}}})},
        {"use_case", make("Casos de uso", {
            {"Casos de uso", {el("Actor", OT::Actor), el("Use Case", OT::UseCase), el("System Boundary", OT::Boundary)}},
            {"Relaciones", {rel("Association", CT::Association), rel("Include", CT::UseCaseLink, "include"),
                            rel("Extend", CT::UseCaseLink, "extend"), rel("Generalization", CT::Generalization)}}})},
        {"sequence", make("Secuencia", {
            {"Interacción", {el("Lifeline", OT::Lifeline), el("Actor", OT::Actor),
                             el("Boundary", OT::Class, "boundary"), el("Control", OT::Class, "control"),
                             el("Entity", OT::Class, "entity")}},
            {"Fragmentos", {el("Alt", OT::InteractionFragment, "", ntype::Alt),
                            el("Opt", OT::InteractionFragment, "", ntype::Opt),
                            el("Loop", OT::InteractionFragment, "", ntype::Loop),
                            el("Par", OT::InteractionFragment, "", ntype::Par),
                            el("Break", OT::InteractionFragment, "", ntype::Break),
                            el("Interaction Use (ref)", OT::InteractionOccurrence)}},
            {"Mensajes", {msg("Message", sequence::MessageSort::Synchronous),
                          msg("Async Message", sequence::MessageSort::Asynchronous),
                          msg("Reply", sequence::MessageSort::Reply), msg("Create", sequence::MessageSort::Create),
                          msg("Delete", sequence::MessageSort::Delete)}}})},
        {"communication", make("Comunicación", {
            {"Comunicación", {el("Object", OT::Object), el("Actor", OT::Actor)}},
            {"Relaciones", {rel("Connector", CT::Association), rel("Message", CT::CommunicationMessage)}}})},
        {"activity", make("Actividad", {
            {"Actividad", {el("Activity", OT::Activity), el("Action", OT::Action), el("Object Node", OT::ObjectNode)}},
            {"Control", {el("Initial", OT::InitialNode, "", ntype::ActivityInitial),
                         el("Activity Final", OT::FinalState, "", ntype::ActivityFinal),
                         el("Flow Final", OT::FinalState, "", ntype::FlowFinal), el("Decision", OT::Decision),
                         el("Merge", OT::MergeNode), el("Fork / Join", OT::ForkNode)}},
            {"Flujos", {rel("Control Flow", CT::ControlFlow), rel("Object Flow", CT::ObjectFlow)}}})},
        {"state_machine", make("Máquina de estados", {
            {"Estados", {el("State", OT::State), el("Initial", OT::InitialNode, "", ntype::StateInitial),
                         el("Final", OT::FinalState, "", ntype::StateFinal), el("Choice", OT::Decision),
                         el("Fork / Join", OT::ForkNode), el("State Machine", OT::StateMachine)}},
            {"Transiciones", {rel("Transition", CT::Transition)}}})},
        {"timing", make("Timing", {
            {"Timing", {el("State Lifeline", OT::TimeLine), el("Lifeline", OT::Lifeline)}},
            {"Mensajes", {rel("Message", CT::Message)}}})},
        {"interaction_overview", make("Interaction Overview", {
            {"Interacciones", {el("Interaction Use (ref)", OT::InteractionOccurrence),
                               el("Initial", OT::InitialNode, "", ntype::ActivityInitial),
                               el("Final", OT::FinalState, "", ntype::ActivityFinal), el("Decision", OT::Decision),
                               el("Merge", OT::MergeNode), el("Fork / Join", OT::ForkNode)}},
            {"Flujos", {rel("Control Flow", CT::ControlFlow)}}})},
    };
    return all;
}

const Palette& general_palette() {
    static const Palette general = make("General", {
        {"Elementos", {el("Class", OT::Class), el("Interface", OT::Interface), el("Component", OT::Component),
                       el("Package", OT::Package), el("Actor", OT::Actor), el("Use Case", OT::UseCase),
                       el("Requirement", OT::Requirement)}},
        {"Relaciones", {rel("Association", CT::Association), rel("Generalization", CT::Generalization),
                        rel("Realization", CT::Realization), rel("Dependency", CT::Dependency),
                        rel("Aggregation", CT::Aggregation), rel("Composition", CT::Composition)}}});
    return general;
}

} // namespace

const Palette& palette_for(const UmlDiagramKind* kind) {
    if (!kind) return general_palette();
    auto it = palettes().find(kind->key);
    return it != palettes().end() ? it->second : general_palette();
}

const Palette& palette_for(DiagramType type, std::string_view stereotype) {
    return palette_for(find_uml_diagram_kind(type, stereotype));
}

} // namespace ea
