#include "ea/core/Types.hpp"
#include <unordered_map>

namespace ea {

std::string to_string(ObjectType type) {
    switch (type) {
        case ObjectType::Package: return "Package";
        case ObjectType::Class: return "Class";
        case ObjectType::Interface: return "Interface";
        case ObjectType::Enumeration: return "Enumeration";
        case ObjectType::DataType: return "DataType";
        case ObjectType::PrimitiveType: return "PrimitiveType";
        case ObjectType::Component: return "Component";
        case ObjectType::Node: return "Node";
        case ObjectType::Artifact: return "Artifact";
        case ObjectType::Device: return "Device";
        case ObjectType::ExecutionEnvironment: return "ExecutionEnvironment";
        case ObjectType::UseCase: return "UseCase";
        case ObjectType::Actor: return "Actor";
        case ObjectType::Activity: return "Activity";
        case ObjectType::Action: return "Action";
        case ObjectType::State: return "State";
        case ObjectType::StateMachine: return "StateMachine";
        case ObjectType::InitialNode: return "InitialNode";
        case ObjectType::FinalState: return "FinalState";
        case ObjectType::Decision: return "Decision";
        case ObjectType::MergeNode: return "MergeNode";
        case ObjectType::ForkNode: return "ForkNode";
        case ObjectType::JoinNode: return "JoinNode";
        case ObjectType::ObjectNode: return "ObjectNode";
        case ObjectType::Requirement: return "Requirement";
        case ObjectType::Issue: return "Issue";
        case ObjectType::Change: return "Change";
        case ObjectType::Feature: return "Feature";
        case ObjectType::Risk: return "Risk";
        case ObjectType::Boundary: return "Boundary";
        case ObjectType::Note: return "Note";
        case ObjectType::Text: return "Text";
        case ObjectType::Screen: return "Screen";
        case ObjectType::GUIElement: return "GUIElement";
        case ObjectType::Port: return "Port";
        default: return "Class";
    }
}

ObjectType object_type_from_string(std::string_view str) {
    static const std::unordered_map<std::string_view, ObjectType> map = {
        {"Package", ObjectType::Package},
        {"Class", ObjectType::Class},
        {"Interface", ObjectType::Interface},
        {"Enumeration", ObjectType::Enumeration},
        {"DataType", ObjectType::DataType},
        {"PrimitiveType", ObjectType::PrimitiveType},
        {"Component", ObjectType::Component},
        {"Node", ObjectType::Node},
        {"Artifact", ObjectType::Artifact},
        {"Device", ObjectType::Device},
        {"ExecutionEnvironment", ObjectType::ExecutionEnvironment},
        {"UseCase", ObjectType::UseCase},
        {"Use Case", ObjectType::UseCase},
        {"Actor", ObjectType::Actor},
        {"Activity", ObjectType::Activity},
        {"Action", ObjectType::Action},
        {"State", ObjectType::State},
        {"StateNode", ObjectType::State},
        {"StateMachine", ObjectType::StateMachine},
        {"InitialNode", ObjectType::InitialNode},
        {"FinalState", ObjectType::FinalState},
        {"Decision", ObjectType::Decision},
        {"MergeNode", ObjectType::MergeNode},
        {"ForkNode", ObjectType::ForkNode},
        {"JoinNode", ObjectType::JoinNode},
        {"ObjectNode", ObjectType::ObjectNode},
        {"Requirement", ObjectType::Requirement},
        {"Issue", ObjectType::Issue},
        {"Change", ObjectType::Change},
        {"Feature", ObjectType::Feature},
        {"Risk", ObjectType::Risk},
        {"Boundary", ObjectType::Boundary},
        {"Note", ObjectType::Note},
        {"Text", ObjectType::Text},
        {"Screen", ObjectType::Screen},
        {"GUIElement", ObjectType::GUIElement},
        {"Port", ObjectType::Port}
    };
    auto it = map.find(str);
    return (it != map.end()) ? it->second : ObjectType::Unknown;
}

std::string to_string(ConnectorType type) {
    switch (type) {
        case ConnectorType::Association: return "Association";
        case ConnectorType::Aggregation: return "Aggregation";
        case ConnectorType::Composition: return "Composition";
        case ConnectorType::Generalization: return "Generalization";
        case ConnectorType::Realization: return "Realization";
        case ConnectorType::Dependency: return "Dependency";
        case ConnectorType::Usage: return "Usage";
        case ConnectorType::Abstraction: return "Abstraction";
        case ConnectorType::InformationFlow: return "InformationFlow";
        case ConnectorType::NoteLink: return "NoteLink";
        case ConnectorType::ControlFlow: return "ControlFlow";
        case ConnectorType::ObjectFlow: return "ObjectFlow";
        case ConnectorType::Transition: return "Transition";
        case ConnectorType::Message: return "Sequence";
        case ConnectorType::Nesting: return "Nesting";
        case ConnectorType::PackageMerge: return "PackageMerge";
        case ConnectorType::PackageImport: return "PackageImport";
        case ConnectorType::Assembly: return "Assembly";
        case ConnectorType::Delegate: return "Delegate";
        default: return "Association";
    }
}

ConnectorType connector_type_from_string(std::string_view str) {
    static const std::unordered_map<std::string_view, ConnectorType> map = {
        {"Association", ConnectorType::Association},
        {"Aggregation", ConnectorType::Aggregation},
        {"Composition", ConnectorType::Composition},
        {"Generalization", ConnectorType::Generalization},
        {"Realization", ConnectorType::Realization},
        {"Realisation", ConnectorType::Realization},
        {"Dependency", ConnectorType::Dependency},
        {"Usage", ConnectorType::Usage},
        {"Abstraction", ConnectorType::Abstraction},
        {"InformationFlow", ConnectorType::InformationFlow},
        {"NoteLink", ConnectorType::NoteLink},
        {"ControlFlow", ConnectorType::ControlFlow},
        {"ObjectFlow", ConnectorType::ObjectFlow},
        {"StateFlow", ConnectorType::Transition},
        {"Transition", ConnectorType::Transition},
        {"Sequence", ConnectorType::Message},
        {"Message", ConnectorType::Message},
        {"Nesting", ConnectorType::Nesting},
        {"PackageMerge", ConnectorType::PackageMerge},
        {"PackageImport", ConnectorType::PackageImport},
        {"Assembly", ConnectorType::Assembly},
        {"Delegate", ConnectorType::Delegate}
    };
    auto it = map.find(str);
    return (it != map.end()) ? it->second : ConnectorType::Unknown;
}

std::string to_string(DiagramType type) {
    switch (type) {
        case DiagramType::Logical: return "Logical";
        case DiagramType::Package: return "Package";
        case DiagramType::Component: return "Component";
        case DiagramType::Deployment: return "Deployment";
        case DiagramType::UseCase: return "Use Case";
        case DiagramType::Activity: return "Activity";
        case DiagramType::Statechart: return "Statechart";
        case DiagramType::Sequence: return "Sequence";
        case DiagramType::Communication: return "Communication";
        case DiagramType::Timing: return "Timing";
        case DiagramType::CompositeStructure: return "CompositeStructure";
        case DiagramType::Analysis: return "Analysis";
        case DiagramType::Custom: return "Custom";
        default: return "Logical";
    }
}

DiagramType diagram_type_from_string(std::string_view str) {
    static const std::unordered_map<std::string_view, DiagramType> map = {
        {"Logical", DiagramType::Logical},
        {"Class", DiagramType::Logical},
        {"Package", DiagramType::Package},
        {"Component", DiagramType::Component},
        {"Deployment", DiagramType::Deployment},
        {"Use Case", DiagramType::UseCase},
        {"UseCase", DiagramType::UseCase},
        {"Activity", DiagramType::Activity},
        {"Statechart", DiagramType::Statechart},
        {"State", DiagramType::Statechart},
        {"Sequence", DiagramType::Sequence},
        {"Communication", DiagramType::Communication},
        {"Timing", DiagramType::Timing},
        {"CompositeStructure", DiagramType::CompositeStructure},
        {"Analysis", DiagramType::Analysis},
        {"Custom", DiagramType::Custom}
    };
    auto it = map.find(str);
    return (it != map.end()) ? it->second : DiagramType::Unknown;
}

std::string to_string(Scope scope) {
    switch (scope) {
        case Scope::Public: return "Public";
        case Scope::Protected: return "Protected";
        case Scope::Private: return "Private";
        case Scope::Package: return "Package";
    }
    return "Public";
}

Scope scope_from_string(std::string_view str) {
    if (str == "Private" || str == "private" || str == "-") return Scope::Private;
    if (str == "Protected" || str == "protected" || str == "#") return Scope::Protected;
    if (str == "Package" || str == "package" || str == "~") return Scope::Package;
    return Scope::Public;
}

} // namespace ea
