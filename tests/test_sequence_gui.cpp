#ifdef NDEBUG
#undef NDEBUG
#endif
// Sequence diagrams on the canvas: lifelines, fragments, messages created by two clicks at the
// clicked height, renumbering, activation bars, vertical drag, self messages, EA's sample diagram.
#include <QApplication>
#include <QPixmap>
#include <cstdlib>
#include <QMouseEvent>
#include <QUndoStack>
#include <cassert>
#include <filesystem>
#include <iostream>
#include "ea/diagram/DiagramPalette.hpp"
#include "ea/diagram/SequenceModel.hpp"
#include "ea/repository/Repository.hpp"
#include "gui/DiagramConnectorItem.hpp"
#include "gui/DiagramView.hpp"

namespace fs = std::filesystem;
using ea::sequence::MessageSort;
using Style = DiagramElementItem::SymbolStyle;

namespace {

const ea::PaletteTool& tool(const std::string& label) {
    for (const auto& g : ea::palette_for(ea::find_uml_diagram_kind("sequence")).groups)
        for (const auto& t : g.tools)
            if (t.label == label) return t;
    assert(false && "tool not in the sequence palette");
    std::abort();
}

void mouse(DiagramView& view, QEvent::Type type, QPointF scenePos, Qt::MouseButtons buttons) {
    const QPointF at = view.mapFromScene(scenePos);
    QMouseEvent e(type, at, view.viewport()->mapToGlobal(at), Qt::LeftButton, buttons, Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &e);
}

void clickAt(DiagramView& view, QPointF scenePos) {
    mouse(view, QEvent::MouseButtonPress, scenePos, Qt::LeftButton);
    mouse(view, QEvent::MouseButtonRelease, scenePos, Qt::NoButton);
}

QPointF lifelineAt(DiagramView& view, int id, double y) {
    auto* item = view.elementItem(id);
    return {item->pos().x() + item->getRect().width() / 2, y};
}

std::vector<DiagramConnectorItem*> messages(DiagramView& view) {
    std::vector<DiagramConnectorItem*> out;
    for (auto* i : view.scene()->items())
        if (auto* c = dynamic_cast<DiagramConnectorItem*>(i)) out.push_back(c);
    return out;
}

int seqOf(DiagramView& view, int connectorId) {
    return view.connectorItem(connectorId)->getConnector()->GetSequenceNo();
}

} // namespace

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    std::cout << "==========================================\n";
    std::cout << " Running onlyArchitect Sequence Diagram GUI Tests\n";
    std::cout << "==========================================\n";

    fs::path db = fs::temp_directory_path() / "onlyarchitect_test_sequence_gui.qea";
    fs::remove(db);
    ea::Repository repo;
    assert(repo.CreateRepository(db.string()));
    const int pkg = repo.GetModels().GetAt(0)->GetPackageID();
    auto diagram = repo.CreateDiagram(pkg, "Login", ea::DiagramType::Sequence, "");
    assert(diagram);
    {
        DiagramView view(diagram, repo);
        view.resize(1200, 900);
        view.show();
        QApplication::processEvents();

        // 1. Lifelines (heads aligned) and fragments
        const int a = view.applyTool(tool("Lifeline"), QPointF(150, 100));
        const int b = view.applyTool(tool("Lifeline"), QPointF(450, 260)); // dropped lower: aligned to the first
        const int c = view.applyTool(tool("Actor"), QPointF(750, 100));
        auto* la = view.elementItem(a);
        auto* lb = view.elementItem(b);
        assert(la->symbolStyle() == Style::Lifeline && view.elementItem(c)->symbolStyle() == Style::Lifeline);
        assert(la->pos().y() == lb->pos().y() && "lifeline heads share one line");
        assert(la->getRect().height() > 300 && "lifelines are tall");
        const int loop = view.applyTool(tool("Loop"), QPointF(400, 400));
        assert(view.elementItem(loop)->symbolStyle() == Style::Frame && view.elementItem(loop)->zValue() < la->zValue());
        std::cout << "  ✓ Lifelines (aligned heads, tall) and combined fragments behind them\n";

        // 2. Message tool: click source and target; the message is at the height of the first click
        const double y1 = la->pos().y() + 120;
        view.applyToolAtCentre(tool("Message"));
        assert(view.isLinking());
        clickAt(view, lifelineAt(view, a, y1));
        clickAt(view, lifelineAt(view, b, y1 + 15)); // a slightly different height on the target: still horizontal
        auto ms = messages(view);
        assert(ms.size() == 1);
        const int call = ms[0]->getConnectorId();
        auto callConn = ms[0]->getConnector();
        assert(callConn->GetType() == ea::ConnectorType::Message && ea::sequence::message_sort(*callConn) == MessageSort::Synchronous);
        assert(std::abs(ea::sequence::message_y(*callConn) - y1) <= 1 && callConn->GetSequenceNo() == 1);
        assert(ms[0]->isSequenceMessage() && ms[0]->path().boundingRect().height() < 1.0 && "drawn horizontally");
        assert(callConn->GetDiagramID() == diagram->GetDiagramID());

        // Reply below it → activation on B from the call to the reply
        const double y2 = y1 + 120;
        const int reply = view.addMessage(b, a, MessageSort::Reply, static_cast<int>(y2));
        assert(reply && seqOf(view, reply) == 2);
        assert(lb->activations().size() == 1 && lb->activations()[0].top == static_cast<int>(std::lround(y1)) &&
               lb->activations()[0].bottom == static_cast<int>(y2));
        std::cout << "  ✓ Message tool: two clicks → synchronous message at the clicked height, horizontal; activation bar\n";

        // 3. Inserting between renumbers the rest, as one undo step
        const int before = view.undoStack()->count();
        const int async = view.addMessage(b, c, MessageSort::Asynchronous, static_cast<int>(y1 + 60));
        assert(async && seqOf(view, async) == 2 && seqOf(view, reply) == 3 && seqOf(view, call) == 1);
        assert(view.undoStack()->count() == before + 1);
        view.undoStack()->undo();
        assert(!view.connectorItem(async) && seqOf(view, reply) == 2);
        view.undoStack()->redo();
        assert(seqOf(view, reply) == 3);
        std::cout << "  ✓ Inserting a message renumbers the following ones; one undo step\n";

        // 4. Dragging a message vertically moves and renumbers it (real mouse drag on the line)
        auto* asyncItem = view.connectorItem(async);
        const QPointF grab = asyncItem->path().pointAtPercent(0.5);
        mouse(view, QEvent::MouseButtonPress, grab, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, grab + QPointF(0, 150), Qt::LeftButton);
        mouse(view, QEvent::MouseButtonRelease, grab + QPointF(0, 150), Qt::NoButton);
        auto asyncConn = view.connectorItem(async)->getConnector();
        assert(std::abs(ea::sequence::message_y(*asyncConn) - (y1 + 210)) <= 1 && "moved by the drag");
        assert(asyncConn->GetSequenceNo() == 3 && seqOf(view, reply) == 2 && "now below the reply");
        view.undoStack()->undo();
        assert(seqOf(view, async) == 2 && seqOf(view, reply) == 3);
        view.undoStack()->redo();
        std::cout << "  ✓ Dragging a message vertically re-orders the interaction (undoable)\n";

        // 5. Self message: same lifeline twice
        view.applyToolAtCentre(tool("Message"));
        clickAt(view, lifelineAt(view, a, y2 + 80));
        clickAt(view, lifelineAt(view, a, y2 + 80));
        int self = 0;
        for (auto* m : messages(view))
            if (m->getConnector()->GetClientID() == a && m->getConnector()->GetSupplierID() == a) self = m->getConnectorId();
        assert(self && view.connectorItem(self)->path().boundingRect().height() > 10 && "drawn as a loop");
        assert(la->activations().size() == 1 && la->activations()[0].depth == 0);
        std::cout << "  ✓ Self message drawn as a loop, with its own activation\n";

        // 6. Persisted for EA: order and heights in t_connector
        const std::string xml = repo.SQLQuery("SELECT count(*) AS n FROM t_connector WHERE Connector_Type='Sequence' AND SeqNo>0 "
                                              "AND PtStartY<0 AND DiagramID=" + std::to_string(diagram->GetDiagramID()));
        assert(xml.find("<n>4</n>") != std::string::npos);
        std::cout << "  ✓ Messages stored as EA expects (Connector_Type Sequence, SeqNo, PtStartY, DiagramID)\n";
    }
    repo.CloseFile();
    fs::remove(db);

    // 7. Optional external sequence diagram regression check (argv[1] or ONLYARCHITECT_SAMPLE_QEA)
    const char* envSample = std::getenv("ONLYARCHITECT_SAMPLE_QEA");
    fs::path sample = argc > 1 ? fs::path(argv[1]) : (envSample && *envSample ? fs::path(envSample) : fs::path{});
    if (!sample.empty() && fs::exists(sample)) {
        fs::path copy = fs::temp_directory_path() / "onlyarchitect_test_sequence_gui_ea.qea";
        fs::copy_file(sample, copy, fs::copy_options::overwrite_existing);
        ea::Repository ea;
        assert(ea.OpenFile(copy.string()));
        DiagramView view(ea.GetDiagramByID(273), ea);
        if (const char* shot = std::getenv("SEQUENCE_SNAPSHOT")) { // visual check: SEQUENCE_SNAPSHOT=/path.png
            view.resize(900, 760);
            view.show();
            view.centerOn(380, 420);
            QApplication::processEvents();
            view.grab().save(shot);
        }
        int lifelines = 0, frames = 0, horizontal = 0, activations = 0;
        for (auto* i : view.scene()->items()) {
            if (auto* e = dynamic_cast<DiagramElementItem*>(i)) {
                lifelines += e->symbolStyle() == Style::Lifeline;
                frames += e->symbolStyle() == Style::Frame;
                activations += static_cast<int>(e->activations().size());
            }
            if (auto* c = dynamic_cast<DiagramConnectorItem*>(i)) horizontal += c->isSequenceMessage();
        }
        assert(lifelines == 5 && frames == 1 && horizontal >= 8 && activations == 5);
        ea.CloseFile();
        fs::remove(copy);
        std::cout << "  ✓ Sample diagram 273: " << lifelines << " lifelines, " << frames << " fragment, "
                  << horizontal << " messages, " << activations << " activation bars\n";
    }

    std::cout << "==========================================\n";
    std::cout << " ALL SEQUENCE DIAGRAM GUI TESTS PASSED!\n";
    std::cout << "==========================================\n";
    return 0;
}
