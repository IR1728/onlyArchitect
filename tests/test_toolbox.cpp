#ifdef NDEBUG
#undef NDEBUG
#endif
// guiaSecuencia.md §2.2 steps 5-7: the Toolbox follows the active diagram; elements are dragged
// from the Toolbox onto the canvas; relationships are drawn between two symbols.
#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QTreeWidget>
#include <QUndoStack>
#include <cassert>
#include <filesystem>
#include <iostream>
#include "ea/diagram/DiagramPalette.hpp"
#include "ea/repository/Repository.hpp"
#include "gui/DiagramView.hpp"
#include "gui/MainWindow.hpp"
#include "gui/ToolboxWidget.hpp"
#include "gui/SequenceSymbols.hpp"

namespace fs = std::filesystem;

static DiagramView* findView(QWidget& window, int diagramId) {
    for (auto* v : window.findChildren<DiagramView*>()) {
        if (v->getDiagramId() == diagramId) return v;
    }
    return nullptr;
}

static const ea::PaletteTool& tool(const ea::Palette& palette, const std::string& label) {
    for (const auto& g : palette.groups)
        for (const auto& t : g.tools)
            if (t.label == label) return t;
    assert(false && "tool not in palette");
    std::abort();
}

static void dropTool(DiagramView* view, const ea::PaletteTool& t, QPoint at) {
    std::unique_ptr<QMimeData> mime(ea::gui::encodePaletteTool(t));
    QDragEnterEvent enter(at, Qt::CopyAction, mime.get(), Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &enter);
    assert(enter.isAccepted() && "the canvas accepts element tools");
    QDropEvent drop(QPointF(at), Qt::CopyAction, mime.get(), Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &drop);
}

static void click(DiagramView* view, int elementId) {
    auto* item = view->elementItem(elementId);
    const QPoint at = view->mapFromScene(item->pos() + QPointF(20, 20));
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(at), view->viewport()->mapToGlobal(QPointF(at)), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &press);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(at), view->viewport()->mapToGlobal(QPointF(at)),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &release);
}

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    std::cout << "==========================================\n";
    std::cout << " Running onlyArchitect Toolbox Tests\n";
    std::cout << "==========================================\n";

    // 1. Mime payload round trip
    {
        const auto& sequencePalette = ea::palette_for(ea::find_uml_diagram_kind("sequence"));
        const ea::PaletteTool loop = tool(sequencePalette, "Loop");
        std::unique_ptr<QMimeData> mime(ea::gui::encodePaletteTool(loop));
        auto back = ea::gui::decodePaletteTool(mime.get());
        assert(back && back->isElement() && back->element == loop.element && back->ntype == loop.ntype &&
               back->label == loop.label);
        QMimeData other;
        other.setText("not a tool");
        assert(!ea::gui::decodePaletteTool(&other));
    }
    std::cout << "  ✓ Drag payload encodes/decodes a palette tool\n";

    fs::path db = fs::temp_directory_path() / "onlyarchitect_test_toolbox.qea";
    fs::remove(db);
    {
        ea::Repository repo;
        assert(repo.CreateRepository(db.string()));
        repo.CloseFile();
    }

    {
    MainWindow window;
    assert(window.openFile(QString::fromStdString(db.string())));
    ea::Repository probe;
    assert(probe.OpenFile(db.string()));
    const int pkg = probe.GetModels().GetAt(0)->GetPackageID();
    probe.CloseFile();
    auto* toolbox = window.findChild<ea::gui::ToolboxWidget*>();
    assert(toolbox);

    // 2. The Toolbox follows the active diagram
    const int seqId = window.createDiagram(pkg, "Login", "sequence");
    assert(toolbox->palette().title == "Secuencia");
    const int classId = window.createDiagram(pkg, "Dominio", "class");
    assert(toolbox->palette().title == "Clases");
    auto* seqView = findView(window, seqId);
    auto* classView = findView(window, classId);
    auto* tabs = [&]() -> QTabWidget* {
        for (auto* tw : window.findChildren<QTabWidget*>()) if (tw->indexOf(seqView) >= 0) return tw;
        return nullptr;
    }();
    tabs->setCurrentWidget(seqView);
    assert(toolbox->palette().title == "Secuencia" && "switching tabs switches the palette");
    std::cout << "  ✓ Toolbox palette follows the active diagram tab\n";

    // 3. Drag & drop from the Toolbox creates the element where it is dropped, as one undo step
    seqView->resize(1000, 800);
    seqView->show();
    QApplication::processEvents();
    const auto& seqPalette = ea::palette_for(ea::find_uml_diagram_kind("sequence"));
    dropTool(seqView, tool(seqPalette, "Lifeline"), QPoint(300, 200));
    dropTool(seqView, tool(seqPalette, "Loop"), QPoint(600, 300));
    assert(seqView->undoStack()->count() == 2);
    int lifelineId = 0, loopId = 0;
    for (auto* item : seqView->scene()->items()) {
        if (auto* e = dynamic_cast<DiagramElementItem*>(item)) {
            if (e->getElement()->GetType() == ea::ObjectType::Lifeline) lifelineId = e->getElementId();
            if (e->getElement()->GetType() == ea::ObjectType::InteractionFragment) loopId = e->getElementId();
        }
    }
    assert(lifelineId && loopId);
    auto* lifeline = seqView->elementItem(lifelineId);
    assert(lifeline->getElement()->GetName() == "Lifeline1");
    // On a sequence diagram a lifeline is placed by its head (the line hangs below it)
    const QPointF head = lifeline->pos() + QPointF(lifeline->getRect().width() / 2, sequence_symbols::HeadHeight / 2);
    assert((head - seqView->mapToScene(QPoint(300, 200))).manhattanLength() < 4 && "created where it was dropped");
    assert(seqView->elementItem(loopId)->getElement()->GetNType() == 4 && "the Loop operator is kept (NType)");
    seqView->undoStack()->undo();
    assert(!seqView->elementItem(loopId));
    seqView->undoStack()->redo();
    std::cout << "  ✓ Drag & drop from the Toolbox: placed at the drop point, undoable, variant kept\n";

    // 4. A relationship tool: click it, then source and target → created with that type, no dialog
    tabs->setCurrentWidget(classView);
    classView->resize(1000, 800);
    classView->show();
    QApplication::processEvents();
    const auto& classPalette = toolbox->palette();
    const int child = classView->applyTool(tool(classPalette, "Class"), QPointF(150, 150));
    const int parent = classView->applyTool(tool(classPalette, "Class"), QPointF(150, 450));
    const int iface = classView->applyTool(tool(classPalette, "Interface"), QPointF(600, 150));
    assert(classView->elementItem(child)->getElement()->GetName() == "Class1");
    assert(classView->elementItem(parent)->getElement()->GetName() == "Class2");

    emit toolbox->toolActivated(tool(classPalette, "Generalization"));
    assert(classView->isLinking());
    click(classView, child);
    click(classView, parent);
    assert(!classView->isLinking());
    int generalization = 0;
    for (auto* item : classView->scene()->items()) {
        if (auto* c = dynamic_cast<DiagramConnectorItem*>(item)) generalization = c->getConnectorId();
    }
    assert(generalization && classView->connectorItem(generalization)->getConnector()->GetType() ==
                                  ea::ConnectorType::Generalization);
    std::cout << "  ✓ Relationship tool: two clicks create that relationship type, no dialog\n";

    // 5. ... and the UML rules still apply (Interface → Class generalization)
    QString rejected;
    QObject::connect(classView, &DiagramView::editRejected, [&rejected](const QString& r) { rejected = r; });
    emit toolbox->toolActivated(tool(classPalette, "Generalization"));
    click(classView, iface);
    click(classView, child);
    assert(!rejected.isEmpty());
    int connectors = 0;
    for (auto* item : classView->scene()->items()) connectors += dynamic_cast<DiagramConnectorItem*>(item) != nullptr;
    assert(connectors == 1 && "the invalid generalization was not created");
    std::cout << "  ✓ Relationship tools are validated (Interface → Class refused)\n";

    // 6. Escape cancels a pending relationship
    emit toolbox->toolActivated(tool(classPalette, "Association"));
    assert(classView->isLinking());
    QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(classView, &esc);
    assert(!classView->isLinking());
    std::cout << "  ✓ Esc cancels link mode\n";
    } // Close MainWindow & SQLite handle before removing temp .qea (required on Windows)

    std::error_code ec;
    fs::remove(db, ec);
    std::cout << "==========================================\n";
    std::cout << " ALL TOOLBOX TESTS PASSED!\n";
    std::cout << "==========================================\n";
    return 0;
}
