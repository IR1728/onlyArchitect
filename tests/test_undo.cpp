#ifdef NDEBUG
#undef NDEBUG
#endif
// Undo/Redo of every canvas operation (guiaSecuencia.md §3.2: "Ctrl+Z deshace la última
// operación (crear, mover, eliminar, editar). Ctrl+Y la rehace.")
#include <QApplication>
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QUndoStack>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include "ea/repository/Repository.hpp"
#include "gui/DiagramView.hpp"
#include "gui/MainWindow.hpp"
#include "gui/UndoableEdit.hpp"

using ea::editor::ElementGeometry;
#include <QAction>

namespace fs = std::filesystem;

static int countItems(QGraphicsScene* scene, int type) {
    int n = 0;
    for (auto* item : scene->items()) {
        if (item->type() == type) ++n;
    }
    return n;
}

static void sendMouse(QWidget* viewport, QEvent::Type type, QPoint pos, Qt::MouseButtons buttons) {
    QMouseEvent ev(type, QPointF(pos), viewport->mapToGlobal(QPointF(pos)), Qt::LeftButton, buttons, Qt::NoModifier);
    QApplication::sendEvent(viewport, &ev);
}

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    std::cout << "==========================================\n";
    std::cout << " Running onlyArchitect Undo/Redo Tests\n";
    std::cout << "==========================================\n";

    const char* envSample = std::getenv("ONLYARCHITECT_SAMPLE_QEA");
    fs::path source = argc > 1 ? fs::path(argv[1]) : (envSample && *envSample ? fs::path(envSample) : fs::path{});
    if (source.empty() || !fs::exists(source)) {
        std::cout << "Note: external sample model not provided (set ONLYARCHITECT_SAMPLE_QEA or pass path), skipping undo tests.\n";
        return 0;
    }
    // Work on a copy: these tests write to the repository
    fs::path db = fs::temp_directory_path() / "onlyarchitect_test_undo.qea";
    fs::copy_file(source, db, fs::copy_options::overwrite_existing);

    ea::Repository repo;
    assert(repo.OpenFile(db.string()));
    auto diag = repo.GetDiagramByID(11);
    assert(diag && "Diagram 11 not found");

    DiagramView view(diag, repo);
    view.resize(1200, 900);
    view.show();
    QApplication::processEvents();

    QUndoStack* stack = view.undoStack();
    QGraphicsScene* scene = view.scene();
    const int baseElements = countItems(scene, DiagramElementItem::Type);
    const int baseConnectors = countItems(scene, DiagramConnectorItem::Type);
    assert(stack->count() == 0);

    // 1. Create element -> undo removes it from the diagram AND the model -> redo restores it
    const int a = view.addElement(ea::ObjectType::Class, "UndoA", QPointF(100, 100));
    assert(a > 0);
    assert(view.elementItem(a) && repo.GetElementByID(a));
    stack->undo();
    assert(!view.elementItem(a));
    assert(!repo.GetElementByID(a) && "undo of create must delete the element from the model");
    assert(countItems(scene, DiagramElementItem::Type) == baseElements);
    stack->redo();
    assert(view.elementItem(a) && repo.GetElementByID(a));
    assert(repo.GetElementByID(a)->GetName() == "UndoA");
    std::cout << "  ✓ Add element: undo/redo\n";

    // 2. Create connector -> undo deletes the relationship -> redo restores it
    const int b = view.addElement(ea::ObjectType::Class, "UndoB", QPointF(500, 100));
    const int c = view.addConnector(a, b, ea::ConnectorType::Generalization);
    assert(c > 0 && view.connectorItem(c) && repo.GetConnectorByID(c));
    stack->undo();
    assert(!view.connectorItem(c));
    assert(!repo.GetConnectorByID(c) && "undo of connect must delete the connector from the model");
    stack->redo();
    assert(view.connectorItem(c) && repo.GetConnectorByID(c));
    assert(repo.GetConnectorByID(c)->GetType() == ea::ConnectorType::Generalization);
    std::cout << "  ✓ Add connector: undo/redo\n";

    // 3. Drag with the mouse -> one undoable move
    auto* itemA = view.elementItem(a);
    const ElementGeometry beforeMove = itemA->geometry();
    view.ensureVisible(itemA);
    QApplication::processEvents();
    QPoint start = view.mapFromScene(itemA->pos() + QPointF(60, 40));
    QPoint end = start + QPoint(80, 50);
    const int countBeforeDrag = stack->count();
    sendMouse(view.viewport(), QEvent::MouseButtonPress, start, Qt::LeftButton);
    sendMouse(view.viewport(), QEvent::MouseMove, start + QPoint(40, 25), Qt::LeftButton);
    sendMouse(view.viewport(), QEvent::MouseMove, end, Qt::LeftButton);
    sendMouse(view.viewport(), QEvent::MouseButtonRelease, end, Qt::NoButton);
    const ElementGeometry afterMove = itemA->geometry();
    assert(!(afterMove == beforeMove) && "drag did not move the element");
    assert(stack->count() == countBeforeDrag + 1 && "a drag must push exactly one command");
    stack->undo();
    assert(view.elementItem(a)->geometry() == beforeMove);
    assert(ElementGeometry::of(*view.elementItem(a)->getDiagramObject()) == beforeMove);
    stack->redo();
    assert(view.elementItem(a)->geometry() == afterMove);
    std::cout << "  ✓ Move (mouse drag): undo/redo\n";

    // 3b. A plain click (no movement) must not create an undo step
    const int countBeforeClick = stack->count();
    QPoint clickAt = view.mapFromScene(view.elementItem(a)->pos() + QPointF(60, 40));
    sendMouse(view.viewport(), QEvent::MouseButtonPress, clickAt, Qt::LeftButton);
    sendMouse(view.viewport(), QEvent::MouseButtonRelease, clickAt, Qt::NoButton);
    assert(stack->count() == countBeforeClick);
    std::cout << "  ✓ Click without drag adds no undo step\n";

    // 4. Delete a selection (element + its connector) -> one undo step restores both
    view.scene()->clearSelection();
    view.elementItem(b)->setSelected(true);
    view.connectorItem(c)->setSelected(true);
    const int countBeforeDelete = stack->count();
    view.removeSelection();
    assert(stack->count() == countBeforeDelete + 1 && "multi-delete must be a single undo step");
    assert(!view.elementItem(b) && !view.connectorItem(c));
    assert(repo.GetElementByID(b) && "remove-from-diagram keeps the element in the model");
    stack->undo();
    assert(view.elementItem(b) && view.connectorItem(c));
    stack->redo();
    assert(!view.elementItem(b) && !view.connectorItem(c));
    stack->undo();
    std::cout << "  ✓ Delete selection: undo/redo\n";

    // 5. Edit properties -> undo restores the old name in the model and the symbol
    view.modifyElement(a, "Rename", [](ea::Element& e) {
        e.SetName("RenamedA");
        e.GetAttributes().AddNew("id", "int");
    });
    assert(repo.GetElementByID(a)->GetName() == "RenamedA");
    assert(view.elementItem(a)->getElement()->GetAttributes().size() == 1);
    stack->undo();
    assert(repo.GetElementByID(a)->GetName() == "UndoA");
    assert(view.elementItem(a)->getElement()->GetName() == "UndoA");
    assert(view.elementItem(a)->getElement()->GetAttributes().size() == 0);
    stack->redo();
    assert(repo.GetElementByID(a)->GetName() == "RenamedA");
    assert(view.elementItem(a)->getElement()->GetAttributes().size() == 1);
    std::cout << "  ✓ Edit element: undo/redo\n";

    // 6. Auto-layout is one undoable step
    std::unordered_map<int, ElementGeometry> beforeLayout;
    for (auto* item : scene->items()) {
        if (auto* ei = dynamic_cast<DiagramElementItem*>(item)) beforeLayout[ei->getElementId()] = ei->geometry();
    }
    view.applyAutoLayout();
    bool moved = false;
    for (const auto& [id, g] : beforeLayout) moved |= !(view.elementItem(id)->geometry() == g);
    assert(moved && "auto-layout did not move anything");
    stack->undo();
    for (const auto& [id, g] : beforeLayout) assert(view.elementItem(id)->geometry() == g);
    std::cout << "  ✓ Auto-layout: undo\n";

    // 7. Undo everything -> back to the diagram as it was loaded
    while (stack->canUndo()) stack->undo();
    assert(countItems(scene, DiagramElementItem::Type) == baseElements);
    assert(countItems(scene, DiagramConnectorItem::Type) == baseConnectors);
    assert(!repo.GetElementByID(a) && !repo.GetElementByID(b) && !repo.GetConnectorByID(c));
    // ... and redo everything
    while (stack->canRedo()) stack->redo();
    assert(view.elementItem(a) && repo.GetElementByID(a)->GetName() == "RenamedA");
    std::cout << "  ✓ Full history: undo all / redo all\n";

    // 8. Items are rebuilt on reload; commands use IDs, so the history keeps working
    view.reloadDiagram();
    stack->undo();
    stack->undo();
    assert(view.elementItem(a)->getElement()->GetName() == "UndoA");
    std::cout << "  ✓ History survives reloadDiagram()\n";

    repo.CloseFile();

    // 9. MainWindow wiring: the Undo/Redo actions follow the active diagram tab
    {
        MainWindow window;
        assert(window.openFile(QString::fromStdString(db.string())));
        // Same path as double-clicking the diagram in the Model Explorer
        QMetaObject::invokeMethod(&window, "onDiagramDoubleClicked", Q_ARG(int, 11));
        auto* winView = window.findChild<DiagramView*>();
        assert(winView && "openFile should open a diagram tab");

        QAction* undoAct = nullptr;
        QAction* redoAct = nullptr;
        for (auto* act : window.actions()) {
            if (act->shortcuts().contains(QKeySequence(Qt::CTRL | Qt::Key_Z))) undoAct = act;
            if (act->shortcuts().contains(QKeySequence(Qt::CTRL | Qt::Key_Y))) redoAct = act;
        }
        assert(undoAct && "no window action bound to Ctrl+Z");
        assert(redoAct && "no window action bound to Ctrl+Y");
        assert(!undoAct->isEnabled());

        const int id = winView->addElement(ea::ObjectType::Class, "ShortcutProbe", QPointF(40, 40));
        assert(undoAct->isEnabled());
        undoAct->trigger();
        assert(!winView->elementItem(id));
        assert(redoAct->isEnabled());
        redoAct->trigger();
        assert(winView->elementItem(id));
        std::cout << "  ✓ MainWindow Ctrl+Z / Ctrl+Y actions drive the active diagram\n";
    }
    fs::remove(db);

    // 10. A command that fails is not kept on the stack, and its error is reported
    {
        struct FailingCommand : ea::editor::Command {
            void execute() override { throw ea::editor::CommandError("disco lleno"); }
            void undo() override {}
            std::string description() const override { return "Falla"; }
        };
        QUndoStack failStack;
        QString reported;
        failStack.push(new UndoableEdit(std::make_shared<FailingCommand>(),
                                        [&reported](UndoableEdit::Failure, const QString& m) { reported = m; }));
        assert(failStack.count() == 0 && "a failed edit must not become an undo step");
        assert(reported == "disco lleno");
        std::cout << "  ✓ Failed command is dropped from the undo stack and reported\n";
    }

    std::cout << "==========================================\n";
    std::cout << " ALL UNDO/REDO TESTS PASSED!\n";
    std::cout << "==========================================\n";
    return 0;
}
