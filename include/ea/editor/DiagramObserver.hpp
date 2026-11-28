#pragma once

#include <memory>
#include "ea/editor/ElementGeometry.hpp"

namespace ea {
class Element;
class Connector;
class DiagramObject;
class DiagramLink;
}

namespace ea::editor {

// Output port of the editor engine: how a command tells the presentation what changed.
// Implemented by the presentation layer; the engine knows nothing about it (Dependency Rule).
class DiagramObserver {
public:
    virtual ~DiagramObserver() = default;

    virtual void elementShown(const std::shared_ptr<DiagramObject>& obj, const std::shared_ptr<Element>& elem) = 0;
    virtual void elementHidden(int elementId) = 0;
    // Called for every connector that should be visible; ignore it if an endpoint is not shown
    virtual void connectorShown(const std::shared_ptr<DiagramLink>& link, const std::shared_ptr<Connector>& conn) = 0;
    virtual void connectorHidden(int connectorId) = 0;
    virtual void geometryChanged(int elementId, const ElementGeometry& geometry) = 0;
    virtual void elementChanged(int elementId) = 0;
    // A connector's own properties changed (e.g. a message moved or was renumbered)
    virtual void connectorChanged(int /*connectorId*/) {}
    // Elements or relationships were created or deleted in the model (e.g. the Model Explorer must refresh)
    virtual void modelEdited() = 0;
};

} // namespace ea::editor
