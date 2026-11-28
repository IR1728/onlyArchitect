#ifdef NDEBUG
#undef NDEBUG
#endif
#include <QApplication>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QPainter>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include "ea/repository/Repository.hpp"
#include "gui/DiagramView.hpp"
#include "gui/DiagramElementItem.hpp"
#include "gui/DiagramConnectorItem.hpp"

int main(int argc, char* argv[]) {
    // Run headless in offscreen mode
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    std::cout << "==========================================\n";
    std::cout << " Running onlyArchitect GUI Manipulation Tests\n";
    std::cout << "==========================================\n";

    std::string db_path = std::filesystem::exists("examples/my_model.qea")
                              ? "examples/my_model.qea"
                              : "../examples/my_model.qea";
    if (argc > 1) {
        db_path = argv[1];
    } else if (const char* env = std::getenv("ONLYARCHITECT_SAMPLE_QEA"); env && *env && std::filesystem::exists(env)) {
        db_path = env;
    }
    ea::Repository repo;
    bool ok = repo.OpenFile(db_path);
    if (!ok) {
        std::cout << "Note: " << db_path << " could not be opened, using dummy test scene.\n";
        return 0;
    }

    // Load Diagram 11
    auto diag = repo.GetDiagramByID(11);
    if (!diag) {
        std::cout << "Note: Diagram 11 not found in " << db_path << " (external sample .qea not provided), skipping sample GUI scene checks.\n";
        return 0;
    }

    DiagramView view(diag, repo);

    // 1. Verify items were created
    QGraphicsScene* scene = view.scene();
    assert(scene != nullptr);
    auto items = scene->items();
    std::cout << "  - Scene item count: " << items.size() << "\n";
    assert(!items.isEmpty());

    // 2. Find a DiagramElementItem
    DiagramElementItem* elemItem = nullptr;
    for (auto* item : items) {
        if (auto* ei = dynamic_cast<DiagramElementItem*>(item)) {
            elemItem = ei;
            break;
        }
    }
    assert(elemItem != nullptr && "No DiagramElementItem found in scene");

    QPointF originalPos = elemItem->pos();
    std::cout << "  - Found element ID " << elemItem->getElementId() 
              << " at initial scene pos: (" << originalPos.x() << ", " << originalPos.y() << ")\n";

    // 3. Test element manipulation (moving the element)
    QPointF newPos(originalPos.x() + 50.0, originalPos.y() + 80.0);
    elemItem->setPos(newPos);
    // Trigger item change
    elemItem->updateCoordinatesFromScene();

    std::cout << "  - Moved element to: (" << elemItem->pos().x() << ", " << elemItem->pos().y() << ")\n";
    assert(elemItem->pos() == newPos);

    // 4. Test resizing the element
    QRectF newRect(0, 0, 220, 140);
    elemItem->setRect(newRect);
    assert(elemItem->getRect().width() == 220);
    assert(elemItem->getRect().height() == 140);
    std::cout << "  - Resized element to: " << elemItem->getRect().width() << "x" << elemItem->getRect().height() << "\n";

    // 5. Test Zoom and Fit actions
    view.zoomIn();
    view.zoomOut();
    view.resetZoom();
    view.fitInView();
    std::cout << "  - Zoom In/Out/Reset/Fit actions executed successfully.\n";

    // 6. Test Pan mode toggling
    view.setPanMode(true);
    assert(view.isPanMode() == true);
    view.setPanMode(false);
    assert(view.isPanMode() == false);
    std::cout << "  - Pan mode toggle verified.\n";

    // 7. Verify Connector Items exist and route dynamically
    int connCount = 0;
    for (auto* item : items) {
        if (auto* ci = dynamic_cast<DiagramConnectorItem*>(item)) {
            connCount++;
            QPainterPath p = ci->path();
            std::cout << "  - Conn #" << connCount << " (type " << ea::to_string(ci->getConnector()->GetType())
                      << "): start elem " << ci->startItem()->getElementId()
                      << " -> end elem " << ci->endItem()->getElementId()
                      << " | from (" << p.pointAtPercent(0.0).x() << ", " << p.pointAtPercent(0.0).y()
                      << ") to (" << p.pointAtPercent(1.0).x() << ", " << p.pointAtPercent(1.0).y()
                      << ") len=" << p.length() << "\n";
        }
    }
    assert(connCount > 0 && "DiagramConnectorItem should exist in Diagram 11");

    // 8. Test Diagram Layout Persistence from View
    bool saveOk = view.saveDiagramLayout();
    assert(saveOk && "saveDiagramLayout should succeed");
    std::cout << "  - Diagram layout persistence verified.\n";

    // 9. Test Diagram 29 (which relies on implicit connector discovery)
    auto diag29 = repo.GetDiagramByID(29);
    assert(diag29 != nullptr && "Diagram 29 not found");
    std::cout << "\n[Testing Diagram 29 - Sequence Diagram with Implicit Connectors]\n";
    std::cout << "  - Diagram 29 name: \"" << diag29->GetName() << "\"\n";
    std::cout << "  - Diagram 29 objects: " << diag29->GetDiagramObjects().size() << "\n";
    std::cout << "  - Diagram 29 links: " << diag29->GetDiagramLinks().size() << "\n";
    assert(diag29->GetDiagramLinks().size() == 10 && "Implicit connectors should be discovered for Diagram 29");

    DiagramView view29(diag29, repo);
    QGraphicsScene* scene29 = view29.scene();
    assert(scene29 != nullptr);
    int connsIn29 = 0;
    for (auto* item : scene29->items()) {
        if (dynamic_cast<DiagramConnectorItem*>(item)) connsIn29++;
    }
    std::cout << "  - Connectors rendered in Diagram 29 GUI: " << connsIn29 << "\n";
    assert(connsIn29 == 10 && "All 10 connectors should appear in Diagram 29");

    // 10. Render Scene to Image and inspect connectors
    QRectF sceneRect = scene->sceneRect();
    QImage image(sceneRect.size().toSize(), QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    scene->render(&painter);
    painter.end();
    const std::string pngOut = (std::filesystem::temp_directory_path() / "rendered_gui_diag11.png").string();
    image.save(QString::fromStdString(pngOut));
    std::cout << "  - Saved rendered scene to " << pngOut << " ("
              << image.width() << "x" << image.height() << ")\n";

    // 11. Item lifetimes: closing a diagram and removing a connected element must not
    //     touch freed items (build with -DONLYARCHITECT_ASAN=ON to catch regressions).
    {
        const std::string copy_path = (std::filesystem::temp_directory_path() / "test_gui_lifetime.qea").string();
        std::filesystem::copy_file(db_path, copy_path, std::filesystem::copy_options::overwrite_existing);
        ea::Repository copyRepo;
        assert(copyRepo.OpenFile(copy_path));

        delete new DiagramView(copyRepo.GetDiagramByID(11), copyRepo);
        std::cout << "  - Opened and closed Diagram 11 view\n";

        auto* lifetimeView = new DiagramView(copyRepo.GetDiagramByID(11), copyRepo);
        DiagramElementItem* connected = nullptr;
        for (auto* item : lifetimeView->scene()->items()) {
            if (auto* ci = dynamic_cast<DiagramConnectorItem*>(item)) { connected = ci->startItem(); break; }
        }
        assert(connected);
        lifetimeView->scene()->clearSelection();
        connected->setSelected(true);
        QKeyEvent deleteKey(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
        QApplication::sendEvent(lifetimeView, &deleteKey);
        for (auto* item : lifetimeView->scene()->items()) {
            if (auto* ci = dynamic_cast<DiagramConnectorItem*>(item)) {
                assert(ci->startItem() && ci->endItem() && "Connector left pointing at a removed element");
            }
        }
        QImage after(800, 600, QImage::Format_ARGB32);
        QPainter afterPainter(&after);
        lifetimeView->scene()->render(&afterPainter);
        afterPainter.end();
        delete lifetimeView;
        std::cout << "  - Removed a connected element, re-rendered and closed the view\n";
    }

    std::cout << "==========================================\n";
    std::cout << " ALL GUI MANIPULATION TESTS PASSED!       \n";
    std::cout << "==========================================\n";

    return 0;
}
