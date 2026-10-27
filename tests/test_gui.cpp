#include <QApplication>
#include <QGraphicsScene>
#include <cassert>
#include <iostream>
#include "ea/repository/Repository.hpp"
#include "gui/DiagramView.hpp"
#include "gui/DiagramElementItem.hpp"
#include "gui/DiagramConnectorItem.hpp"

int main(int argc, char* argv[]) {
    // Run headless in offscreen mode
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    std::cout << "==========================================\n";
    std::cout << " Running OpenEA GUI Manipulation Tests    \n";
    std::cout << "==========================================\n";

    std::string db_path = "/home/iran/EA/re/bin/EA_Trial/EAExample.qea";
    ea::Repository repo;
    bool ok = repo.OpenFile(db_path);
    assert(ok && "Could not open EAExample.qea");

    // Load Diagram 11
    auto diag = repo.GetDiagramByID(11);
    assert(diag != nullptr && "Diagram 11 not found");

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
    image.save("/tmp/rendered_gui_diag11.png");
    std::cout << "  - Saved rendered scene to /tmp/rendered_gui_diag11.png ("
              << image.width() << "x" << image.height() << ")\n";

    std::cout << "==========================================\n";
    std::cout << " ALL GUI MANIPULATION TESTS PASSED!       \n";
    std::cout << "==========================================\n";

    return 0;
}
