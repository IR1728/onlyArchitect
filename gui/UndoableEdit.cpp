#include "UndoableEdit.hpp"

UndoableEdit::UndoableEdit(std::shared_ptr<ea::editor::Command> command, ErrorHandler onError, QUndoCommand* parent)
    : QUndoCommand(QString::fromStdString(command->description()), parent),
      m_command(std::move(command)), m_onError(std::move(onError)) {}

void UndoableEdit::redo() {
    run(&ea::editor::Command::execute);
}

void UndoableEdit::undo() {
    run(&ea::editor::Command::undo);
}

void UndoableEdit::run(void (ea::editor::Command::*step)()) {
    // Exceptions must not cross Qt's event loop: translate them into an obsolete command + a message
    try {
        ((*m_command).*step)();
    } catch (const ea::editor::RuleViolation& e) {
        fail(Failure::RuleViolation, e.what());
    } catch (const ea::editor::CommandError& e) {
        fail(Failure::Error, e.what());
    }
}

void UndoableEdit::fail(Failure kind, const char* message) {
    setObsolete(true);
    if (m_onError) m_onError(kind, QString::fromUtf8(message));
}
