#pragma once

// Editor engine (guiaSecuencia.md §2.4 "Controller / Editor Engine"): every change a user makes
// on a diagram is a Command with execute()/undo(). Commands are plain C++: they talk to the
// model through ModelGateway and report to the presentation through DiagramObserver, so they
// can be tested without a GUI and reused by any front end.
//
// Commands identify elements and connectors by ID, never by presentation object.
// execute() is idempotent with respect to its target state: running it on a model that already
// is in that state (e.g. right after an interactive drag) changes nothing but re-notifies.

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "ea/editor/DiagramObserver.hpp"
#include "ea/editor/ElementGeometry.hpp"
#include "ea/editor/ElementSnapshot.hpp"
#include "ea/editor/ModelGateway.hpp"
#include "ea/diagram/SequenceModel.hpp"
#include "ea/model/Connector.hpp"
#include "ea/model/Diagram.hpp"
#include "ea/model/Element.hpp"

namespace ea::editor {

// A command could not be carried out (typically a persistence failure). The model is left as it
// was before the call.
class CommandError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// The edit breaks a UML rule (see ea::validation); nothing was changed.
class RuleViolation : public CommandError {
public:
    using CommandError::CommandError;
};

// What every command works on
struct EditContext {
    ModelGateway& model;
    std::shared_ptr<Diagram> diagram;
    DiagramObserver& observer;
};

class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0; // do, and redo
    virtual void undo() = 0;
    virtual std::string description() const = 0;
};

// Creates a new element in the model and places it on the diagram.
class AddElementCommand : public Command {
public:
    AddElementCommand(EditContext ctx, std::shared_ptr<Element> newElement, const ElementGeometry& placement);
    void execute() override;
    void undo() override;
    std::string description() const override;
    int elementId() const;

private:
    EditContext m_ctx;
    std::shared_ptr<Element> m_elem;
    std::shared_ptr<DiagramObject> m_dobj;
};

// Takes an element's symbol off the diagram; the element stays in the model.
class RemoveElementFromDiagramCommand : public Command {
public:
    RemoveElementFromDiagramCommand(EditContext ctx, int elementId);
    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    EditContext m_ctx;
    int m_elementId;
    std::string m_name;
    std::shared_ptr<DiagramObject> m_dobj; // with the geometry it had, to put it back
};

// Shows an existing model element on the diagram (dragged from the Model Explorer) together with
// its relationships to the symbols already there, as EA does. Refuses an element already shown.
class PlaceElementCommand : public Command {
public:
    PlaceElementCommand(EditContext ctx, int elementId, ElementGeometry geometry);
    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    EditContext m_ctx;
    int m_elementId;
    ElementGeometry m_geometry;
    std::string m_name;
    std::vector<int> m_addedLinks; // relationships to symbols already on the diagram, drawn by this command
};

// Creates a new relationship in the model and shows it on the diagram.
// Refuses (RuleViolation) relationships that UML does not allow between its two ends.
class AddConnectorCommand : public Command {
public:
    AddConnectorCommand(EditContext ctx, std::shared_ptr<Connector> newConnector);
    void execute() override;
    void undo() override;
    std::string description() const override;
    int connectorId() const;

private:
    EditContext m_ctx;
    std::shared_ptr<Connector> m_conn;
    std::shared_ptr<DiagramLink> m_dlink;
};

// Hides a relationship on the diagram (EA keeps the t_diagramlinks row with Hidden = 1).
class HideConnectorCommand : public Command {
public:
    HideConnectorCommand(EditContext ctx, int connectorId);
    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    EditContext m_ctx;
    int m_connectorId;
};

struct GeometryChange {
    int elementId;
    ElementGeometry before;
    ElementGeometry after;
};

// Several commands as one step, all or nothing: if one fails, those already done are undone.
class CompositeCommand : public Command {
public:
    CompositeCommand(std::vector<std::shared_ptr<Command>> parts, std::string description);
    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    std::vector<std::shared_ptr<Command>> m_parts;
    std::string m_description;
};

// Moves and renumbers interaction messages (a message placed or dragged, the others renumbered).
class PlaceMessagesCommand : public Command {
public:
    PlaceMessagesCommand(EditContext ctx, std::vector<sequence::MessagePlacement> placements, std::string description);
    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    enum class Side { Before, After };
    void applyAll(Side side);

    EditContext m_ctx;
    std::vector<sequence::MessagePlacement> m_placements;
    std::string m_description;
};

// Moves and/or resizes symbols (drag, resize grip, auto-layout) as a single step.
class ChangeGeometryCommand : public Command {
public:
    ChangeGeometryCommand(EditContext ctx, std::vector<GeometryChange> changes);
    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    enum class Side { Before, After };
    void applyAll(Side side);

    EditContext m_ctx;
    std::vector<GeometryChange> m_changes;
    std::unordered_map<int, size_t> m_indexByElement; // built once: one pass over the diagram per apply
};

// Changes the properties of an element (name, stereotype, attributes, operations, ...).
class EditElementCommand : public Command {
public:
    EditElementCommand(EditContext ctx, int elementId, ElementSnapshot before, ElementSnapshot after,
                       std::string description);
    void execute() override;
    void undo() override;
    std::string description() const override;

private:
    void apply(const ElementSnapshot& state);

    EditContext m_ctx;
    int m_elementId;
    ElementSnapshot m_before;
    ElementSnapshot m_after;
    std::string m_description;
};

// The command that turns `elem` into what `mutation` makes of it. `elem` itself is left as it was
// (the command applies and persists the change when executed); nullptr when nothing would change.
std::shared_ptr<EditElementCommand> make_edit_element_command(EditContext ctx, Element& elem,
                                                              const std::function<void(Element&)>& mutation,
                                                              std::string description);

// Shows every non-hidden connector of the diagram that touches the element (after it reappears).
void showConnectorsOf(EditContext& ctx, int elementId);

} // namespace ea::editor
