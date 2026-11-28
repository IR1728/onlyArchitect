#ifdef NDEBUG
#undef NDEBUG
#endif
// Interaction (sequence diagram) core, no Qt: message sorts in EA's encoding, ordering,
// activation bars, persistence, commands, and EA's own sample sequence diagram.
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include "ea/diagram/SequenceModel.hpp"
#include "ea/editor/EditCommands.hpp"
#include "ea/repository/Repository.hpp"
#include "ea/validation/ConnectionRules.hpp"

namespace fs = std::filesystem;
using namespace ea;
using namespace ea::sequence;

namespace {

Connector message(int id, int from, int to, MessageSort sort, int y, int seq = 0) {
    Connector c;
    c.SetConnectorID(id);
    c.SetType(ConnectorType::Message);
    c.SetClientID(from);
    c.SetSupplierID(to);
    set_message_sort(c, sort);
    set_message_y(c, y);
    c.SetSequenceNo(seq);
    return c;
}

std::vector<const Connector*> ptrs(const std::vector<Connector>& v) {
    std::vector<const Connector*> out;
    for (const auto& c : v) out.push_back(&c);
    return out;
}

struct NullObserver : editor::DiagramObserver {
    std::vector<int> changedConnectors;
    void elementShown(const std::shared_ptr<DiagramObject>&, const std::shared_ptr<Element>&) override {}
    void elementHidden(int) override {}
    void connectorShown(const std::shared_ptr<DiagramLink>&, const std::shared_ptr<Connector>&) override {}
    void connectorHidden(int) override {}
    void geometryChanged(int, const editor::ElementGeometry&) override {}
    void elementChanged(int) override {}
    void connectorChanged(int id) override { changedConnectors.push_back(id); }
    void modelEdited() override {}
};

struct Recorder : editor::Command {
    std::vector<std::string>& log;
    std::string name;
    bool failOnExecute;
    Recorder(std::vector<std::string>& l, std::string n, bool fail = false) : log(l), name(std::move(n)), failOnExecute(fail) {}
    void execute() override {
        if (failOnExecute) throw editor::CommandError("boom");
        log.push_back("do " + name);
    }
    void undo() override { log.push_back("undo " + name); }
    std::string description() const override { return name; }
};

} // namespace

int main(int argc, char* argv[]) {
    std::cout << "==========================================\n";
    std::cout << " Running onlyArchitect Sequence Diagram Tests (no Qt)\n";
    std::cout << "==========================================\n";

    // 1. Message sorts ↔ EA columns
    for (auto sort : {MessageSort::Synchronous, MessageSort::Asynchronous, MessageSort::Reply, MessageSort::Create,
                      MessageSort::Delete}) {
        Connector c;
        set_message_sort(c, sort);
        assert(message_sort(c) == sort);
    }
    {
        Connector c;
        set_message_sort(c, MessageSort::Asynchronous);
        assert(c.GetPData(1) == "Asynchronous" && c.GetPData(4) == "0" && c.GetPData(3) == "Call");
        set_message_sort(c, MessageSort::Reply);
        assert(c.GetPData(1) == "Synchronous" && c.GetPData(4) == "1");
        set_message_sort(c, MessageSort::Create);
        assert(c.GetSubtype() == "New");
        set_message_y(c, 329);
        assert(c.GetStartY() == -329 && c.GetEndY() == -329 && message_y(c) == 329);
        c.SetName("login()");
        c.SetSequenceNo(3);
        assert(message_label(c) == "3: login()");
    }
    assert(fragment_operator(0) == "alt" && fragment_operator(4) == "loop" && fragment_operator(99) == "alt");
    assert(is_lifeline_kind(ObjectType::Lifeline) && is_lifeline_kind(ObjectType::Actor) && !is_lifeline_kind(ObjectType::Note));
    assert(default_symbol_size(ObjectType::Lifeline, DiagramType::Sequence).height > default_symbol_size(ObjectType::Class, DiagramType::Logical).height);
    std::cout << "  ✓ Message sorts, positions and labels in EA's encoding (PDATA1/PDATA4/SubType/PtStartY)\n";

    // 2. Ordering: placing renumbers top to bottom; ties go after the existing message
    {
        std::vector<Connector> ms = {message(10, 1, 2, MessageSort::Synchronous, 100, 1),
                                     message(11, 2, 1, MessageSort::Reply, 200, 2),
                                     message(12, 1, 3, MessageSort::Asynchronous, 300, 3)};
        auto added = place_message(ptrs(ms), 0, 150); // new one between the first two
        assert(added.size() == 3);
        assert(added[0].connectorId == 0 && added[0].after.sequenceNo == 2 && added[0].after.y == 150);
        assert(added[1].connectorId == 11 && added[1].before.sequenceNo == 2 && added[1].after.sequenceNo == 3);
        assert(added[2].connectorId == 12 && added[2].after.sequenceNo == 4);

        auto moved = place_message(ptrs(ms), 12, 50); // last one dragged to the top
        assert(moved.size() == 3 && moved[0].connectorId == 12 && moved[0].after == (MessagePosition{50, 1}));
        auto tie = place_message(ptrs(ms), 0, 200);
        assert(tie[0].connectorId == 0 && tie[0].after.sequenceNo == 3 && "equal height: after the existing one");
        auto still = place_message(ptrs(ms), 11, 200);
        assert(still.size() == 1 && still[0].after == still[0].before && "re-placing in place changes nothing else");
    }
    std::cout << "  ✓ Placing a message renumbers the interaction (insert, drag, ties)\n";

    // 3. Activation bars: sync call opens on the receiver, reply closes; nesting; unanswered calls
    {
        std::vector<Connector> ms = {message(1, 1, 2, MessageSort::Synchronous, 100), // A→B
                                     message(2, 2, 3, MessageSort::Synchronous, 130), // B→C
                                     message(3, 3, 2, MessageSort::Reply, 160),       // C→B
                                     message(4, 2, 2, MessageSort::Synchronous, 170), // B→B self call
                                     message(5, 2, 1, MessageSort::Reply, 220),       // B→A (closes the self call)
                                     message(6, 1, 3, MessageSort::Asynchronous, 240),
                                     message(7, 1, 4, MessageSort::Synchronous, 260)}; // no reply
        auto acts = compute_activations(ptrs(ms));
        auto find = [&](int lifeline, int top) {
            for (const auto& a : acts) if (a.lifelineId == lifeline && a.top == top) return a;
            assert(false && "activation missing");
            return Activation{};
        };
        assert(acts.size() == 4);
        const auto b = find(2, 100), self = find(2, 170), c = find(3, 130), d = find(4, 260);
        assert(c.bottom == 160 && c.depth == 0);
        assert(b.depth == 0 && b.bottom == 220 && "B→A replies to A's call: it ends B's outer activation");
        assert(self.depth == 1 && self.bottom == 220 && "...and the self call nested in it");
        assert(d.bottom == 260 + OpenActivationHeight && "unanswered call: default height");
    }
    std::cout << "  ✓ Activation bars from calls and replies (nesting, self calls, unanswered calls)\n";

    // 4. UML rule: messages only between lifelines
    {
        Element lifeline, actor, pkg, frag;
        lifeline.SetType(ObjectType::Lifeline);
        actor.SetType(ObjectType::Actor);
        pkg.SetType(ObjectType::Package);
        frag.SetType(ObjectType::InteractionOccurrence);
        assert(validation::check_connection(ConnectorType::Message, actor, lifeline));
        assert(validation::check_connection(ConnectorType::Message, lifeline, frag) && "gates on a ref");
        assert(!validation::check_connection(ConnectorType::Message, pkg, lifeline));
    }
    std::cout << "  ✓ Messages are refused between non-lifelines\n";

    // 5. Composite command: all or nothing
    {
        std::vector<std::string> log;
        editor::CompositeCommand ok({std::make_shared<Recorder>(log, "a"), std::make_shared<Recorder>(log, "b")}, "ab");
        ok.execute();
        ok.undo();
        assert((log == std::vector<std::string>{"do a", "do b", "undo b", "undo a"}));
        log.clear();
        editor::CompositeCommand bad({std::make_shared<Recorder>(log, "a"), std::make_shared<Recorder>(log, "b", true)}, "ab");
        bool threw = false;
        try { bad.execute(); } catch (const editor::CommandError&) { threw = true; }
        assert(threw && (log == std::vector<std::string>{"do a", "undo a"}));
    }
    std::cout << "  ✓ CompositeCommand rolls back the parts already done\n";

    // 6. Persistence + PlaceMessagesCommand
    fs::path db = fs::temp_directory_path() / "onlyarchitect_test_sequence.qea";
    fs::remove(db);
    int m1 = 0, m2 = 0;
    {
        Repository repo;
        assert(repo.CreateRepository(db.string()));
        const int pkg = repo.GetModels().GetAt(0)->GetPackageID();
        Element proto;
        proto.SetType(ObjectType::Lifeline);
        proto.SetPackageID(pkg);
        const int a = repo.AddElement(proto)->GetElementID();
        const int b = repo.AddElement(proto)->GetElementID();
        auto c1 = message(0, a, b, MessageSort::Synchronous, 100, 1);
        c1.SetName("pedir()");
        c1.SetDiagramID(7);
        auto c2 = message(0, b, a, MessageSort::Reply, 140, 2);
        m1 = repo.AddConnector(c1)->GetConnectorID();
        m2 = repo.AddConnector(c2)->GetConnectorID();

        NullObserver observer;
        editor::EditContext ctx{repo, nullptr, observer};
        std::vector<const Connector*> current = {repo.GetConnectorByID(m1).get(), repo.GetConnectorByID(m2).get()};
        editor::PlaceMessagesCommand move(ctx, place_message(current, m2, 60), "Mover");
        move.execute();
        assert(repo.GetConnectorByID(m2)->GetSequenceNo() == 1 && repo.GetConnectorByID(m1)->GetSequenceNo() == 2);
        assert(observer.changedConnectors.size() == 2);
        move.undo();
        assert(repo.GetConnectorByID(m2)->GetSequenceNo() == 2 && message_y(*repo.GetConnectorByID(m2)) == 140);
        move.execute();
        repo.CloseFile();
    }
    {
        Repository repo;
        assert(repo.OpenFile(db.string()));
        auto c1 = repo.GetConnectorByID(m1), c2 = repo.GetConnectorByID(m2);
        assert(c1->GetType() == ConnectorType::Message && message_sort(*c1) == MessageSort::Synchronous);
        assert(c1->GetSequenceNo() == 2 && message_y(*c1) == 100 && c1->GetDiagramID() == 7 && c1->GetName() == "pedir()");
        assert(message_sort(*c2) == MessageSort::Reply && c2->GetSequenceNo() == 1 && message_y(*c2) == 60);
        repo.CloseFile();
    }
    fs::remove(db);
    std::cout << "  ✓ Messages persist (sort, order, height, diagram) and PlaceMessagesCommand undoes\n";

    // 7. Optional external sequence diagram regression check (argv[1] or ONLYARCHITECT_SAMPLE_QEA)
    const char* envSample = std::getenv("ONLYARCHITECT_SAMPLE_QEA");
    fs::path sample = argc > 1 ? fs::path(argv[1]) : (envSample && *envSample ? fs::path(envSample) : fs::path{});
    if (!sample.empty() && fs::exists(sample)) {
        Repository repo;
        assert(repo.OpenFile(sample.string()));
        auto diagram = repo.GetDiagramByID(273);
        assert(diagram && diagram->GetType() == DiagramType::Sequence);
        std::vector<std::shared_ptr<Connector>> owned;
        for (const auto& link : diagram->GetDiagramLinks()) {
            auto c = link ? repo.GetConnectorByID(link->GetConnectorID()) : nullptr;
            if (c && c->GetType() == ConnectorType::Message) owned.push_back(c);
        }
        assert(owned.size() >= 8);
        std::vector<const Connector*> msgs;
        int replies = 0, lastSeq = 0, lastY = -1;
        std::sort(owned.begin(), owned.end(), [](auto& x, auto& y) { return x->GetSequenceNo() < y->GetSequenceNo(); });
        for (const auto& c : owned) {
            msgs.push_back(c.get());
            replies += message_sort(*c) == MessageSort::Reply;
            assert(c->GetSequenceNo() > lastSeq && message_y(*c) > lastY && "EA's numbering follows the height");
            lastSeq = c->GetSequenceNo();
            lastY = message_y(*c);
        }
        assert(replies > 0);
        const auto acts = compute_activations(msgs);
        assert(!acts.empty());
        for (const auto& a : acts) assert(a.bottom > a.top && a.top >= 235 && "activations inside the lifelines (235..648)");
        auto lifeline = repo.GetElementByID(2064);
        assert(lifeline && lifeline->GetType() == ObjectType::Lifeline && is_lifeline_kind(lifeline->GetType()));
        repo.CloseFile();
        std::cout << "  ✓ Sample diagram 273: " << owned.size() << " messages, " << replies << " replies, "
                  << acts.size() << " activations, numbering consistent with height\n";
    }

    std::cout << "==========================================\n";
    std::cout << " ALL SEQUENCE DIAGRAM TESTS PASSED!\n";
    std::cout << "==========================================\n";
    return 0;
}
