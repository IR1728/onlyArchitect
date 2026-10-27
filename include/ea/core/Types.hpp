#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <optional>
#include <cstdint>

namespace ea {

// Common GUID structure matching EA's standard {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}
using Guid = std::string;

// EA Object Types (t_object.Object_Type)
enum class ObjectType {
    Unknown,
    Package,
    Class,
    Interface,
    Enumeration,
    DataType,
    PrimitiveType,
    Component,
    Node,
    Artifact,
    Device,
    ExecutionEnvironment,
    UseCase,
    Actor,
    Activity,
    Action,
    State,
    StateMachine,
    InitialNode,
    FinalState,
    Decision,
    MergeNode,
    ForkNode,
    JoinNode,
    ObjectNode,
    Requirement,
    Issue,
    Change,
    Feature,
    Risk,
    Boundary,
    Note,
    Text,
    Screen,
    GUIElement,
    Port
};

// EA Connector Types (t_connector.Connector_Type)
enum class ConnectorType {
    Unknown,
    Association,
    Aggregation,
    Composition,
    Generalization,
    Realization,
    Dependency,
    Usage,
    Abstraction,
    InformationFlow,
    NoteLink,
    ControlFlow,
    ObjectFlow,
    Transition,
    Message,
    Nesting,
    PackageMerge,
    PackageImport,
    Assembly,
    Delegate
};

// EA Diagram Types (t_diagram.Diagram_Type)
enum class DiagramType {
    Unknown,
    Logical,         // Class diagram
    Package,
    Component,
    Deployment,
    UseCase,
    Activity,
    Statechart,
    Sequence,
    Communication,
    Timing,
    CompositeStructure,
    Custom,
    Analysis
};

// UML Visibility / Scope
enum class Scope {
    Public,
    Protected,
    Private,
    Package
};

// Conversion helper functions
std::string to_string(ObjectType type);
ObjectType object_type_from_string(std::string_view str);

std::string to_string(ConnectorType type);
ConnectorType connector_type_from_string(std::string_view str);

std::string to_string(DiagramType type);
DiagramType diagram_type_from_string(std::string_view str);

std::string to_string(Scope scope);
Scope scope_from_string(std::string_view str);

} // namespace ea
