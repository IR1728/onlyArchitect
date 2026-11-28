#ifdef NDEBUG
#undef NDEBUG
#endif
// Canvas features on the real widgets: zoom limits (view + slider), grid snapping while dragging,
// the eight resize handles, marquee selection, dragging elements from the Model Explorer.
#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QSlider>
#include <QTreeWidget>
#include <QUndoStack>
#include <QWheelEvent>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include "ea/repository/Repository.hpp"
#include "gui/DiagramConnectorItem.hpp"
#include "gui/DiagramView.hpp"
#include "gui/ElementDrag.hpp"
#include "gui/MainWindow.hpp"
#include "gui/ModelTreeWidget.hpp"

namespace fs = std::filesystem;

namespace {

void mouse(DiagramView& view, QEvent::Type type, QPointF scenePos, Qt::MouseButtons buttons,
           Qt::KeyboardModifiers mods = Qt::NoModifier) {
    const QPointF at = view.mapFromScene(scenePos);
    QMouseEvent e(type, at, view.viewport()->mapToGlobal(at), Qt::LeftButton, buttons, mods);
    QApplication::sendEvent(view.viewport(), &e);
}

void drag(DiagramView& view, QPointF from, QPointF to, Qt::KeyboardModifiers mods = Qt::NoModifier) {
    mouse(view, QEvent::MouseButtonPress, from, Qt::LeftButton, mods);
    mouse(view, QEvent::MouseMove, (from + to) / 2, Qt::LeftButton, mods);
    mouse(view, QEvent::MouseMove, to, Qt::LeftButton, mods);
    mouse(view, QEvent::MouseButtonRelease, to, Qt::NoButton, mods);
}

// What Qt delivers for a real drag: enter (must be accepted), then drop
void dropOn(DiagramView& view, const QMimeData* mime, QPointF scenePos) {
    const QPoint at = view.mapFromScene(scenePos);
    QDragEnterEvent enter(at, Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &enter);
    assert(enter.isAccepted() && "the canvas accepts explorer elements");
    QDropEvent drop(QPointF(at), Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &drop);
}

int selectedCount(DiagramView& view) {
    int n = 0;
    for (auto* i : view.scene()->selectedItems()) n += dynamic_cast<DiagramElementItem*>(i) != nullptr;
    return n;
}

} // namespace

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    std::cout << "==========================================\n";
    std::cout << " Running onlyArchitect Canvas Tests\n";
    std::cout << "==========================================\n";

    fs::path db = fs::temp_directory_path() / "onlyarchitect_test_canvas.qea";
    fs::remove(db);
    int pkg = 0, loose = 0, linked = 0;
    {
        ea::Repository repo;
        assert(repo.CreateRepository(db.string()));
        pkg = repo.GetModels().GetAt(0)->GetPackageID();
        for (auto [name, id] : {std::pair<const char*, int*>{"Factura", &loose}, {"Cliente", &linked}}) {
            ea::Element e;
            e.SetType(ea::ObjectType::Class);
            e.SetName(name);
            e.SetPackageID(pkg);
            *id = repo.AddElement(e)->GetElementID();
        }
        ea::Connector c;
        c.SetType(ea::ConnectorType::Association);
        c.SetClientID(loose);
        c.SetSupplierID(linked);
        assert(repo.AddConnector(c));
        repo.CloseFile();
    }

    {
    MainWindow window;
    window.resize(1400, 900);
    window.show();
    assert(window.openFile(QString::fromStdString(db.string())));
    const int diagramId = window.createDiagram(pkg, "Lienzo", "class");
    DiagramView* view = nullptr;
    for (auto* v : window.findChildren<DiagramView*>()) if (v->getDiagramId() == diagramId) view = v;
    assert(view);
    view->resize(1000, 800);
    QApplication::processEvents();

    // 1. Zoom limits: view, wheel and status-bar slider
    QSlider* slider = nullptr;
    for (auto* s : window.findChildren<QSlider*>()) if (s->toolTip().startsWith("Zoom")) slider = s;
    assert(slider && slider->minimum() == 10 && slider->maximum() == 400);
    view->setZoom(10.0);
    assert(view->zoom() == 4.0 && slider->value() == 400);
    for (int i = 0; i < 40; ++i) view->zoomOut();
    assert(std::abs(view->zoom() - 0.1) < 1e-9 && slider->value() == 10);
    slider->setValue(150);
    assert(std::abs(view->zoom() - 1.5) < 1e-9);
    QWheelEvent wheel(QPointF(10, 10), QPointF(10, 10), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::ControlModifier,
                      Qt::NoScrollPhase, false);
    QApplication::sendEvent(view->viewport(), &wheel);
    assert(std::abs(view->zoom() - 1.8) < 1e-9 && slider->value() == 180);
    view->resetZoom();
    assert(view->zoom() == 1.0 && slider->value() == 100);
    std::cout << "  ✓ Zoom 10 %–400 % from buttons, Ctrl+wheel and slider, kept in sync\n";

    // 2. Snapping while dragging (programmatic placement is exact)
    const int a = view->addElement(ea::ObjectType::Class, "A", QPointF(103, 207));
    auto* itemA = view->elementItem(a);
    assert(itemA->pos() == QPointF(103, 207));
    const int stepsBefore = view->undoStack()->count();
    drag(*view, itemA->pos() + QPointF(60, 50), itemA->pos() + QPointF(60 + 37, 50 + 23));
    assert(std::fmod(itemA->pos().x(), 10) == 0 && std::fmod(itemA->pos().y(), 10) == 0 && "snapped to the grid");
    assert(itemA->pos() == QPointF(140, 230) && view->undoStack()->count() == stepsBefore + 1);
    view->undoStack()->undo();
    assert(itemA->pos() == QPointF(103, 207));
    view->setSnapToGrid(false);
    drag(*view, itemA->pos() + QPointF(60, 50), itemA->pos() + QPointF(60 + 37, 50 + 23));
    assert(itemA->pos() == QPointF(140, 230) && "without snapping the symbol follows the mouse exactly");
    view->setSnapToGrid(true);
    view->undoStack()->undo();
    std::cout << "  ✓ Dragging snaps to the 10 px grid (undoable); snapping can be switched off\n";

    // 3. Eight resize handles
    view->scene()->clearSelection();
    itemA->setSelected(true);
    const auto start = itemA->sceneBox();
    drag(*view, QPointF(start.left, start.top), QPointF(start.left - 33, start.top - 47)); // top-left corner
    auto box = itemA->sceneBox();
    assert(box.right == start.right && box.bottom == start.bottom && "the opposite corner stays");
    assert(box.left == 70 && box.top == 160 && "moved edges snap");
    view->undoStack()->undo();
    assert(itemA->sceneBox() == start);
    const double midY = (start.top + start.bottom) / 2;
    drag(*view, QPointF(start.right, midY), QPointF(start.right + 52, midY + 80)); // right edge
    box = itemA->sceneBox();
    assert(box.left == start.left && box.top == start.top && box.bottom == start.bottom && box.right == 330);
    drag(*view, QPointF(box.right, midY), QPointF(box.left - 500, midY));
    assert(itemA->sceneBox().width() == ea::editor::MinSymbolSize && "never smaller than the minimum");
    view->undoStack()->undo();
    view->undoStack()->undo();
    assert(itemA->sceneBox() == start);
    itemA->setSelected(false);
    drag(*view, QPointF(start.right, start.bottom), QPointF(start.right + 40, start.bottom + 40));
    assert(itemA->sceneBox().width() == start.width() && "no handles on unselected symbols: that was a move");
    view->undoStack()->undo();
    std::cout << "  ✓ Eight handles: corners and edges resize with snapping, minimum size, undo\n";

    // 4. Marquee selection on the background
    const int b = view->addElement(ea::ObjectType::Class, "B", QPointF(400, 200));
    const int c = view->addElement(ea::ObjectType::Class, "C", QPointF(400, 450));
    (void)b;
    view->scene()->clearSelection();
    drag(*view, QPointF(50, 150), QPointF(620, 350)); // left→right around A and B
    assert(selectedCount(*view) == 2 && !view->elementItem(c)->isSelected());
    drag(*view, QPointF(450, 470), QPointF(380, 430), Qt::ControlModifier); // right→left crossing C, added
    assert(selectedCount(*view) == 3);
    drag(*view, QPointF(800, 700), QPointF(900, 760)); // empty area: clears
    assert(selectedCount(*view) == 0);
    bool marqueeLeftover = false;
    for (auto* i : view->scene()->items()) marqueeLeftover = marqueeLeftover || i->zValue() == 1000;
    assert(!marqueeLeftover);
    std::cout << "  ✓ Marquee: enclosing left→right, crossing right→left, Ctrl adds, no leftovers\n";

    // 5. Drag from the Model Explorer
    view->addElement(ea::ObjectType::Class, "placeholder", QPointF(700, 100)); // unrelated symbol
    auto* tree = window.findChild<ModelTreeWidget*>()->findChild<QTreeWidget*>();
    const auto hits = tree->model()->match(tree->model()->index(0, 0), Qt::DisplayRole, "Factura", 1,
                                           Qt::MatchContains | Qt::MatchRecursive);
    assert(hits.size() == 1);
    std::unique_ptr<QMimeData> payload(tree->model()->mimeData({hits.first()}));
    assert(payload && ea::gui::decodeElementDrag(payload.get()) == loose && "the explorer drags element ids");
    // Cliente first (no relationship drawn yet), then Factura: the association between them appears
    std::unique_ptr<QMimeData> clienteDrag(ea::gui::encodeElementDrag(linked));
    dropOn(*view, clienteDrag.get(), QPointF(150, 600));
    dropOn(*view, payload.get(), QPointF(500, 650));
    assert(view->elementItem(loose) && view->elementItem(linked));
    int connectors = 0;
    for (auto* i : view->scene()->items()) connectors += dynamic_cast<DiagramConnectorItem*>(i) != nullptr;
    assert(connectors == 1 && "its relationship to a symbol already there is drawn");
    view->undoStack()->undo();
    connectors = 0;
    for (auto* i : view->scene()->items()) connectors += dynamic_cast<DiagramConnectorItem*>(i) != nullptr;
    assert(!view->elementItem(loose) && connectors == 0);
    view->undoStack()->redo();
    QString refused;
    QObject::connect(view, &DiagramView::editFailed, [&refused](const QString& m) { refused = m; });
    const int steps = view->undoStack()->count();
    assert(!view->placeElement(loose, QPointF(800, 300)) && !refused.isEmpty() && view->undoStack()->count() == steps);
    std::cout << "  ✓ Explorer → canvas: placed where dropped with its relationships; undo; no duplicates\n";
    } // Close MainWindow & SQLite handle before removing temp .qea (required on Windows)

    std::error_code ec;
    fs::remove(db, ec);
    std::cout << "==========================================\n";
    std::cout << " ALL CANVAS TESTS PASSED!\n";
    std::cout << "==========================================\n";
    return 0;
}
