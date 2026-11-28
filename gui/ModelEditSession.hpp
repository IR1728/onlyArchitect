#pragma once

#include <QObject>
#include <functional>
#include "ea/editor/DiagramObserver.hpp"
#include "ea/editor/ModelGateway.hpp"
#include "ea/model/Element.hpp"

class QUndoStack;

// Undoable model edits that do not belong to a diagram (e.g. the Property Editor showing an element
// picked in the Model Explorer while no diagram is open). Same commands as DiagramView, own undo stack.
class ModelEditSession : public QObject, private ea::editor::DiagramObserver {
    Q_OBJECT
public:
    explicit ModelEditSession(ea::editor::ModelGateway& model, QObject* parent = nullptr);

    QUndoStack* undoStack() const { return m_undoStack; }
    // false when the edit failed (the reason goes to editFailed); true when applied or nothing changed
    bool modifyElement(int elementId, const QString& description, const std::function<void(ea::Element&)>& mutation);

signals:
    void elementEdited(int elementId);
    void modelChanged();
    void editFailed(const QString& message);

private:
    // DiagramObserver: no diagram, so only element and model notifications matter
    void elementShown(const std::shared_ptr<ea::DiagramObject>&, const std::shared_ptr<ea::Element>&) override {}
    void elementHidden(int) override {}
    void connectorShown(const std::shared_ptr<ea::DiagramLink>&, const std::shared_ptr<ea::Connector>&) override {}
    void connectorHidden(int) override {}
    void geometryChanged(int, const ea::editor::ElementGeometry&) override {}
    void elementChanged(int elementId) override { emit elementEdited(elementId); }
    void modelEdited() override { emit modelChanged(); }

    ea::editor::ModelGateway& m_model;
    QUndoStack* m_undoStack{nullptr};
    bool m_lastEditFailed{false};
};
