#include "ea/editor/EditCommands.hpp"
#include <unordered_set>
#include "ea/validation/ConnectionRules.hpp"

namespace ea::editor {

namespace {

std::shared_ptr<DiagramObject> findDiagramObject(const Diagram& diagram, int elementId) {
    for (const auto& obj : diagram.GetDiagramObjects()) {
        if (obj && obj->GetElementID() == elementId) return obj;
    }
    return nullptr;
}

std::shared_ptr<DiagramLink> findDiagramLink(const Diagram& diagram, int connectorId) {
    for (const auto& link : diagram.GetDiagramLinks()) {
        if (link && link->GetConnectorID() == connectorId) return link;
    }
    return nullptr;
}

std::string in_quotes(const std::string& name) {
    return "\"" + name + "\"";
}

} // namespace

void showConnectorsOf(EditContext& ctx, int elementId) {
    for (const auto& link : ctx.diagram->GetDiagramLinks()) {
        if (!link || link->IsHidden()) continue;
        auto conn = ctx.model.GetConnectorByID(link->GetConnectorID());
        if (conn && (conn->GetClientID() == elementId || conn->GetSupplierID() == elementId)) {
            ctx.observer.connectorShown(link, conn);
        }
    }
}

// ============================================================================
// AddElementCommand
// ============================================================================

AddElementCommand::AddElementCommand(EditContext ctx, std::shared_ptr<Element> newElement,
                                     const ElementGeometry& placement)
    : m_ctx(ctx), m_elem(std::move(newElement)), m_dobj(std::make_shared<DiagramObject>()) {
    m_dobj->SetDiagramID(m_ctx.diagram->GetDiagramID());
    placement.applyTo(*m_dobj);
}

int AddElementCommand::elementId() const {
    return m_elem->GetElementID();
}

std::string AddElementCommand::description() const {
    return "Crear " + in_quotes(m_elem->GetName());
}

void AddElementCommand::execute() {
    // The first run assigns the Object_ID; a redo re-inserts the row under the same ID.
    // From then on the command holds the instance the model hands out (one object per element).
    auto live = m_ctx.model.AddElement(*m_elem);
    if (!live) throw CommandError("No se pudo guardar el elemento " + in_quotes(m_elem->GetName()));
    m_elem = live;
    m_dobj->SetElementID(live->GetElementID());

    if (!m_ctx.model.AddElementToDiagram(*m_ctx.diagram, m_dobj)) {
        m_ctx.model.DeleteElement(live->GetElementID());
        throw CommandError("No se pudo colocar " + in_quotes(live->GetName()) + " en el diagrama");
    }
    m_ctx.observer.elementShown(m_dobj, m_elem);
    m_ctx.observer.modelEdited();
}

void AddElementCommand::undo() {
    const int id = elementId();
    if (!m_ctx.model.RemoveElementFromDiagram(*m_ctx.diagram, id)) {
        throw CommandError("No se pudo quitar " + in_quotes(m_elem->GetName()) + " del diagrama");
    }
    if (!m_ctx.model.DeleteElement(id)) {
        m_ctx.model.AddElementToDiagram(*m_ctx.diagram, m_dobj);
        throw CommandError("No se pudo borrar " + in_quotes(m_elem->GetName()) + " del modelo");
    }
    m_ctx.observer.elementHidden(id);
    m_ctx.observer.modelEdited();
}

// ============================================================================
// RemoveElementFromDiagramCommand
// ============================================================================

RemoveElementFromDiagramCommand::RemoveElementFromDiagramCommand(EditContext ctx, int elementId)
    : m_ctx(ctx), m_elementId(elementId) {
    auto elem = m_ctx.model.GetElementByID(elementId);
    m_name = elem ? elem->GetName() : std::to_string(elementId);
}

std::string RemoveElementFromDiagramCommand::description() const {
    return "Quitar " + in_quotes(m_name) + " del diagrama";
}

void RemoveElementFromDiagramCommand::execute() {
    // The live DiagramObject carries the current geometry; keep it to restore the symbol as it was
    m_dobj = findDiagramObject(*m_ctx.diagram, m_elementId);
    if (!m_dobj) throw CommandError(in_quotes(m_name) + " no está en el diagrama");
    if (!m_ctx.model.RemoveElementFromDiagram(*m_ctx.diagram, m_elementId)) {
        throw CommandError("No se pudo quitar " + in_quotes(m_name) + " del diagrama");
    }
    m_ctx.observer.elementHidden(m_elementId);
}

void RemoveElementFromDiagramCommand::undo() {
    if (!m_ctx.model.AddElementToDiagram(*m_ctx.diagram, m_dobj)) {
        throw CommandError("No se pudo volver a colocar " + in_quotes(m_name) + " en el diagrama");
    }
    m_ctx.observer.elementShown(m_dobj, m_ctx.model.GetElementByID(m_elementId));
    showConnectorsOf(m_ctx, m_elementId);
}

// ============================================================================
// PlaceElementCommand
// ============================================================================

PlaceElementCommand::PlaceElementCommand(EditContext ctx, int elementId, ElementGeometry geometry)
    : m_ctx(ctx), m_elementId(elementId), m_geometry(geometry) {
    auto elem = m_ctx.model.GetElementByID(elementId);
    m_name = elem ? elem->GetName() : std::to_string(elementId);
}

std::string PlaceElementCommand::description() const {
    return "Colocar " + in_quotes(m_name) + " en el diagrama";
}

void PlaceElementCommand::execute() {
    auto elem = m_ctx.model.GetElementByID(m_elementId);
    if (!elem) throw CommandError("El elemento " + in_quotes(m_name) + " ya no existe");
    if (findDiagramObject(*m_ctx.diagram, m_elementId)) throw CommandError(in_quotes(m_name) + " ya está en el diagrama");
    auto obj = std::make_shared<DiagramObject>();
    obj->SetDiagramID(m_ctx.diagram->GetDiagramID());
    obj->SetElementID(m_elementId);
    m_geometry.applyTo(*obj);
    if (!m_ctx.model.AddElementToDiagram(*m_ctx.diagram, obj)) {
        throw CommandError("No se pudo colocar " + in_quotes(m_name) + " en el diagrama");
    }
    // Relationships to symbols already on the diagram that the diagram does not draw yet
    m_addedLinks.clear();
    std::unordered_set<int> linked;
    for (const auto& l : m_ctx.diagram->GetDiagramLinks()) if (l) linked.insert(l->GetConnectorID());
    for (const auto& conn : elem->GetConnectors()) {
        if (!conn || linked.count(conn->GetConnectorID())) continue;
        const int other = conn->GetClientID() == m_elementId ? conn->GetSupplierID() : conn->GetClientID();
        if (other != m_elementId && !findDiagramObject(*m_ctx.diagram, other)) continue;
        auto link = std::make_shared<DiagramLink>();
        link->SetDiagramID(m_ctx.diagram->GetDiagramID());
        link->SetConnectorID(conn->GetConnectorID());
        if (!m_ctx.model.AddConnectorToDiagram(*m_ctx.diagram, link)) {
            for (int id : m_addedLinks) m_ctx.model.DeleteConnectorFromDiagram(*m_ctx.diagram, id);
            m_ctx.model.RemoveElementFromDiagram(*m_ctx.diagram, m_elementId);
            throw CommandError("No se pudo mostrar una relación de " + in_quotes(m_name));
        }
        m_addedLinks.push_back(conn->GetConnectorID());
    }
    m_ctx.observer.elementShown(obj, elem);
    showConnectorsOf(m_ctx, m_elementId);
}

void PlaceElementCommand::undo() {
    for (int id : m_addedLinks) {
        m_ctx.observer.connectorHidden(id);
        m_ctx.model.DeleteConnectorFromDiagram(*m_ctx.diagram, id);
    }
    if (!m_ctx.model.RemoveElementFromDiagram(*m_ctx.diagram, m_elementId)) {
        throw CommandError("No se pudo quitar " + in_quotes(m_name) + " del diagrama");
    }
    m_ctx.observer.elementHidden(m_elementId);
}

// ============================================================================
// AddConnectorCommand
// ============================================================================

AddConnectorCommand::AddConnectorCommand(EditContext ctx, std::shared_ptr<Connector> newConnector)
    : m_ctx(ctx), m_conn(std::move(newConnector)) {}

int AddConnectorCommand::connectorId() const {
    return m_conn->GetConnectorID();
}

std::string AddConnectorCommand::description() const {
    return "Crear " + to_string(m_conn->GetType());
}

void AddConnectorCommand::execute() {
    auto source = m_ctx.model.GetElementByID(m_conn->GetClientID());
    auto target = m_ctx.model.GetElementByID(m_conn->GetSupplierID());
    if (!source || !target) throw CommandError("Uno de los extremos de la relación no existe");
    const auto lookup = [this](int id) { return m_ctx.model.GetElementByID(id); };
    if (auto verdict = validation::check_new_connection(m_conn->GetType(), *source, *target, lookup); !verdict) {
        throw RuleViolation(verdict.reason);
    }

    auto live = m_ctx.model.AddConnector(*m_conn);
    if (!live) throw CommandError("No se pudo guardar la relación " + to_string(m_conn->GetType()));
    m_conn = live;
    if (!m_dlink) {
        m_dlink = std::make_shared<DiagramLink>(m_ctx.diagram->GetDiagramID(), live->GetConnectorID());
    }
    if (!m_ctx.model.AddConnectorToDiagram(*m_ctx.diagram, m_dlink)) {
        m_ctx.model.DeleteConnector(live->GetConnectorID());
        throw CommandError("No se pudo mostrar la relación en el diagrama");
    }
    m_ctx.observer.connectorShown(m_dlink, m_conn);
    m_ctx.observer.modelEdited();
}

void AddConnectorCommand::undo() {
    const int id = connectorId();
    if (!m_ctx.model.DeleteConnectorFromDiagram(*m_ctx.diagram, id)) {
        throw CommandError("No se pudo quitar la relación del diagrama");
    }
    if (!m_ctx.model.DeleteConnector(id)) {
        m_ctx.model.AddConnectorToDiagram(*m_ctx.diagram, m_dlink);
        throw CommandError("No se pudo borrar la relación del modelo");
    }
    m_ctx.observer.connectorHidden(id);
    m_ctx.observer.modelEdited();
}

// ============================================================================
// HideConnectorCommand
// ============================================================================

HideConnectorCommand::HideConnectorCommand(EditContext ctx, int connectorId)
    : m_ctx(ctx), m_connectorId(connectorId) {}

std::string HideConnectorCommand::description() const {
    return "Quitar relación del diagrama";
}

void HideConnectorCommand::execute() {
    if (!m_ctx.model.RemoveConnectorFromDiagram(*m_ctx.diagram, m_connectorId)) {
        throw CommandError("No se pudo quitar la relación del diagrama");
    }
    m_ctx.observer.connectorHidden(m_connectorId);
}

void HideConnectorCommand::undo() {
    auto link = findDiagramLink(*m_ctx.diagram, m_connectorId);
    if (!link) throw CommandError("La relación ya no pertenece al diagrama");
    link->SetHidden(false);
    if (!m_ctx.model.SaveDiagramLink(*link)) {
        link->SetHidden(true);
        throw CommandError("No se pudo volver a mostrar la relación");
    }
    m_ctx.observer.connectorShown(link, m_ctx.model.GetConnectorByID(m_connectorId));
}

// ============================================================================
// ChangeGeometryCommand
// ============================================================================

ChangeGeometryCommand::ChangeGeometryCommand(EditContext ctx, std::vector<GeometryChange> changes)
    : m_ctx(ctx), m_changes(std::move(changes)) {
    m_indexByElement.reserve(m_changes.size());
    for (size_t i = 0; i < m_changes.size(); ++i) {
        m_indexByElement.emplace(m_changes[i].elementId, i);
    }
}

std::string ChangeGeometryCommand::description() const {
    if (m_changes.size() != 1) return "Mover " + std::to_string(m_changes.size()) + " elementos";
    const auto& c = m_changes.front();
    const bool resized = c.before.width() != c.after.width() || c.before.height() != c.after.height();
    return resized ? "Redimensionar elemento" : "Mover elemento";
}

void ChangeGeometryCommand::applyAll(Side side) {
    // Geometry is persisted with the diagram on save, like any other layout change
    for (const auto& obj : m_ctx.diagram->GetDiagramObjects()) {
        if (!obj) continue;
        auto it = m_indexByElement.find(obj->GetElementID());
        if (it == m_indexByElement.end()) continue;
        const auto& change = m_changes[it->second];
        const ElementGeometry& target = side == Side::After ? change.after : change.before;
        target.applyTo(*obj);
        m_ctx.observer.geometryChanged(change.elementId, target);
    }
}

void ChangeGeometryCommand::execute() {
    applyAll(Side::After);
}

void ChangeGeometryCommand::undo() {
    applyAll(Side::Before);
}

// ============================================================================
// EditElementCommand
// ============================================================================

EditElementCommand::EditElementCommand(EditContext ctx, int elementId, ElementSnapshot before,
                                       ElementSnapshot after, std::string description)
    : m_ctx(ctx), m_elementId(elementId), m_before(std::move(before)), m_after(std::move(after)),
      m_description(std::move(description)) {}

std::string EditElementCommand::description() const {
    return m_description;
}

void EditElementCommand::apply(const ElementSnapshot& state) {
    auto elem = m_ctx.model.GetElementByID(m_elementId);
    if (!elem) throw CommandError("El elemento " + std::to_string(m_elementId) + " ya no existe");
    const auto previous = ElementSnapshot::capture(*elem);
    state.restoreInto(*elem);
    if (!m_ctx.model.SaveElement(*elem)) {
        previous.restoreInto(*elem);
        throw CommandError("No se pudo guardar " + in_quotes(state.name));
    }
    m_ctx.observer.elementChanged(m_elementId);
    m_ctx.observer.modelEdited();
}

void EditElementCommand::execute() {
    apply(m_after);
}

void EditElementCommand::undo() {
    apply(m_before);
}

// ============================================================================
// CompositeCommand
// ============================================================================

CompositeCommand::CompositeCommand(std::vector<std::shared_ptr<Command>> parts, std::string description)
    : m_parts(std::move(parts)), m_description(std::move(description)) {}

std::string CompositeCommand::description() const {
    return m_description;
}

void CompositeCommand::execute() {
    size_t done = 0;
    try {
        for (; done < m_parts.size(); ++done) m_parts[done]->execute();
    } catch (...) {
        while (done > 0) m_parts[--done]->undo();
        throw;
    }
}

void CompositeCommand::undo() {
    size_t pending = m_parts.size();
    try {
        for (; pending > 0; --pending) m_parts[pending - 1]->undo();
    } catch (...) {
        for (; pending < m_parts.size(); ++pending) m_parts[pending]->execute();
        throw;
    }
}

// ============================================================================
// PlaceMessagesCommand
// ============================================================================

PlaceMessagesCommand::PlaceMessagesCommand(EditContext ctx, std::vector<sequence::MessagePlacement> placements,
                                           std::string description)
    : m_ctx(ctx), m_placements(std::move(placements)), m_description(std::move(description)) {}

std::string PlaceMessagesCommand::description() const {
    return m_description;
}

void PlaceMessagesCommand::execute() {
    applyAll(Side::After);
}

void PlaceMessagesCommand::undo() {
    applyAll(Side::Before);
}

void PlaceMessagesCommand::applyAll(Side side) {
    std::vector<std::pair<std::shared_ptr<Connector>, sequence::MessagePosition>> applied; // for rollback
    applied.reserve(m_placements.size());
    for (const auto& p : m_placements) {
        auto conn = m_ctx.model.GetConnectorByID(p.connectorId);
        if (!conn) continue; // removed meanwhile: nothing to place
        const sequence::MessagePosition previous{sequence::message_y(*conn), conn->GetSequenceNo()};
        const auto& target = side == Side::After ? p.after : p.before;
        sequence::set_message_y(*conn, target.y);
        conn->SetSequenceNo(target.sequenceNo);
        if (!m_ctx.model.SaveConnector(*conn)) {
            sequence::set_message_y(*conn, previous.y);
            conn->SetSequenceNo(previous.sequenceNo);
            for (auto& [c, pos] : applied) {
                sequence::set_message_y(*c, pos.y);
                c->SetSequenceNo(pos.sequenceNo);
                m_ctx.model.SaveConnector(*c);
            }
            throw CommandError("No se pudo guardar el mensaje " + in_quotes(conn->GetName()));
        }
        applied.emplace_back(conn, previous);
    }
    for (const auto& [c, pos] : applied) m_ctx.observer.connectorChanged(c->GetConnectorID());
}

std::shared_ptr<EditElementCommand> make_edit_element_command(EditContext ctx, Element& elem,
                                                              const std::function<void(Element&)>& mutation,
                                                              std::string description) {
    auto before = ElementSnapshot::capture(elem);
    mutation(elem);
    auto after = ElementSnapshot::capture(elem);
    before.restoreInto(elem);
    if (before == after) return nullptr;
    return std::make_shared<EditElementCommand>(ctx, elem.GetElementID(), std::move(before), std::move(after),
                                                std::move(description));
}

} // namespace ea::editor
