#pragma once

#include <memory>

namespace ea {
class Element;
class Connector;
class Diagram;
class DiagramObject;
class DiagramLink;
}

namespace ea::editor {

// Input port of the editor engine: the only model operations the edit commands need (ISP).
// Implemented by the persistence layer (ea::Repository), so dependencies point inward (DIP).
// Every mutating operation returns false when it could not be carried out.
class ModelGateway {
public:
    virtual ~ModelGateway() = default;

    virtual std::shared_ptr<Element> GetElementByID(int elementId) = 0;
    virtual std::shared_ptr<Connector> GetConnectorByID(int connectorId) = 0;

    // Model entities. Add* returns the live instance (the one GetElementByID/GetConnectorByID return).
    virtual std::shared_ptr<Element> AddElement(Element& elem) = 0;
    virtual bool SaveElement(Element& elem) = 0;
    virtual bool DeleteElement(int elementId) = 0;
    virtual std::shared_ptr<Connector> AddConnector(Connector& conn) = 0;
    virtual bool DeleteConnector(int connectorId) = 0;
    virtual bool SaveConnector(Connector& conn) = 0;

    // Presence on a diagram
    virtual bool AddElementToDiagram(Diagram& diag, const std::shared_ptr<DiagramObject>& obj) = 0;
    virtual bool RemoveElementFromDiagram(Diagram& diag, int elementId) = 0;
    virtual bool AddConnectorToDiagram(Diagram& diag, const std::shared_ptr<DiagramLink>& link) = 0;
    virtual bool DeleteConnectorFromDiagram(Diagram& diag, int connectorId) = 0;
    virtual bool RemoveConnectorFromDiagram(Diagram& diag, int connectorId) = 0; // hides it
    virtual bool SaveDiagramLink(const DiagramLink& link) = 0;
};

} // namespace ea::editor
