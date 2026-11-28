#include "ModelEditSession.hpp"
#include <QUndoStack>
#include "UndoableEdit.hpp"
#include "ea/editor/EditCommands.hpp"

ModelEditSession::ModelEditSession(ea::editor::ModelGateway& model, QObject* parent)
    : QObject(parent), m_model(model), m_undoStack(new QUndoStack(this)) {}

bool ModelEditSession::modifyElement(int elementId, const QString& description,
                                     const std::function<void(ea::Element&)>& mutation) {
    auto elem = m_model.GetElementByID(elementId);
    if (!elem) return false;
    ea::editor::EditContext ctx{m_model, nullptr, *this};
    auto command = ea::editor::make_edit_element_command(ctx, *elem, mutation, description.toStdString());
    if (!command) return true;
    m_lastEditFailed = false;
    // The handler outlives this call (undo/redo run it later): it must only touch members
    m_undoStack->push(new UndoableEdit(std::move(command), [this](UndoableEdit::Failure, const QString& message) {
        m_lastEditFailed = true;
        emit editFailed(message);
    }));
    return !m_lastEditFailed;
}
