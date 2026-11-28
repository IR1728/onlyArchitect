#pragma once

#include <QUndoCommand>
#include <functional>
#include <memory>
#include "ea/editor/EditCommands.hpp"

// Presentation adapter: puts an editor-engine command on a QUndoStack, so Qt's undo framework
// (stacks per diagram, QUndoGroup, undo/redo actions) drives commands that know nothing about Qt.
//
// A command that throws ea::editor::CommandError marks itself obsolete: QUndoStack then drops it
// instead of keeping a step whose model change never happened. The error goes to `onError`.
class UndoableEdit : public QUndoCommand {
public:
    enum class Failure { RuleViolation, Error };
    using ErrorHandler = std::function<void(Failure kind, const QString& message)>;

    UndoableEdit(std::shared_ptr<ea::editor::Command> command, ErrorHandler onError, QUndoCommand* parent = nullptr);

    void redo() override;
    void undo() override;

private:
    void run(void (ea::editor::Command::*step)());
    void fail(Failure kind, const char* message);

    std::shared_ptr<ea::editor::Command> m_command;
    ErrorHandler m_onError;
};
