#include "UmlIcons.hpp"
#include "DarkTheme.hpp"
#include "M3Icons.hpp"

namespace ea::gui {

namespace {

QIcon iconOf(const UmlGlyph& glyph) { return M3Icons::icon(glyph.symbol, glyph.color); }

} // namespace

UmlGlyph umlElementGlyph(ObjectType type) {
    const auto& c = m3();
    switch (type) {
        // Structure
        case ObjectType::Package: return {"folder", c.secondary};
        case ObjectType::Class: return {"view_agenda", c.primary};
        case ObjectType::Interface: return {"radio_button_unchecked", c.tertiary};
        case ObjectType::Enumeration: return {"format_list_numbered", c.secondary};
        case ObjectType::DataType: return {"data_object", c.secondary};
        case ObjectType::PrimitiveType: return {"tag", c.secondary};
        case ObjectType::Object: return {"label", c.primary};
        case ObjectType::Port: return {"crop_free", c.secondary};
        // Components and deployment
        case ObjectType::Component: return {"extension", c.primary};
        case ObjectType::Node: return {"dns", c.primary};
        case ObjectType::Artifact: return {"description", c.onSurfaceVariant};
        case ObjectType::Device: return {"devices", c.primary};
        case ObjectType::ExecutionEnvironment: return {"memory", c.primary};
        // Use cases
        case ObjectType::UseCase: return {"trip_origin", c.primary};
        case ObjectType::Actor: return {"person", c.success};
        case ObjectType::Boundary: return {"frame_inspect", c.onSurfaceMuted};
        // Behaviour
        case ObjectType::Activity: return {"workflow", c.primary};
        case ObjectType::Action: return {"rounded_corner", c.primary};
        case ObjectType::State: return {"toggle_on", c.primary};
        case ObjectType::StateMachine: return {"schema", c.primary};
        case ObjectType::InitialNode: return {"circle", c.onSurface};
        case ObjectType::FinalState: return {"stop_circle", c.onSurface};
        case ObjectType::Decision:
        case ObjectType::MergeNode: return {"diamond", c.warning};
        case ObjectType::ForkNode: return {"call_split", c.onSurface};
        case ObjectType::JoinNode: return {"call_merge", c.onSurface};
        case ObjectType::ObjectNode: return {"view_in_ar", c.secondary};
        case ObjectType::ActivityPartition: return {"view_stream", c.secondary};
        case ObjectType::Event: return {"bolt", c.warning};
        // Interactions
        case ObjectType::Lifeline: return {"view_column", c.primary};
        case ObjectType::InteractionFragment: return {"splitscreen_vertical_add", c.tertiary};
        case ObjectType::InteractionOccurrence: return {"input", c.tertiary};
        case ObjectType::TimeLine: return {"timeline", c.primary};
        // Requirements and project management
        case ObjectType::Requirement: return {"checklist", c.tertiary};
        case ObjectType::Issue: return {"bug_report", c.error};
        case ObjectType::Change: return {"published_with_changes", c.warning};
        case ObjectType::Feature: return {"star", c.tertiary};
        case ObjectType::Risk: return {"warning", c.error};
        // Annotations and user interface
        case ObjectType::Note: return {"sticky_note_2", c.warning};
        case ObjectType::Text: return {"text_fields", c.onSurfaceVariant};
        case ObjectType::Screen: return {"web", c.secondary};
        case ObjectType::GUIElement: return {"smart_button", c.secondary};
        case ObjectType::Unknown: break;
    }
    return {"inventory_2", c.onSurfaceMuted};
}

UmlGlyph umlRelationshipGlyph(ConnectorType type) {
    const auto& c = m3();
    switch (type) {
        case ConnectorType::Association: return {"horizontal_rule", c.secondary};
        case ConnectorType::Aggregation:
        case ConnectorType::Composition: return {"diamond", c.secondary};
        case ConnectorType::Generalization: return {"change_history", c.secondary};
        case ConnectorType::Realization: return {"change_history", c.tertiary};
        case ConnectorType::Dependency:
        case ConnectorType::Usage:
        case ConnectorType::Abstraction: return {"trending_flat", c.onSurfaceVariant};
        case ConnectorType::InformationFlow: return {"double_arrow", c.secondary};
        case ConnectorType::NoteLink: return {"more_horiz", c.warning};
        case ConnectorType::ControlFlow:
        case ConnectorType::Transition: return {"arrow_right_alt", c.secondary};
        case ConnectorType::ObjectFlow: return {"double_arrow", c.secondary};
        case ConnectorType::Message:
        case ConnectorType::CommunicationMessage: return {"send", c.primary};
        case ConnectorType::Nesting: return {"add_circle", c.secondary};
        case ConnectorType::PackageMerge: return {"merge", c.secondary};
        case ConnectorType::PackageImport: return {"input", c.secondary};
        case ConnectorType::Assembly: return {"link", c.primary};
        case ConnectorType::Delegate: return {"subdirectory_arrow_right", c.secondary};
        case ConnectorType::UseCaseLink: return {"call_made", c.tertiary};
        case ConnectorType::Unknown: break;
    }
    return {"arrow_forward", c.onSurfaceMuted};
}

QIcon umlElementIcon(ObjectType type) { return iconOf(umlElementGlyph(type)); }

QIcon umlRelationshipIcon(ConnectorType type) { return iconOf(umlRelationshipGlyph(type)); }

} // namespace ea::gui
