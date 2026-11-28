#ifdef NDEBUG
#undef NDEBUG
#endif
// Editor engine (ea::editor) tested on its own: no Qt, no GUI. Links only onlyarchitect_core,
// which is what the Dependency Rule buys: the edit policy is testable without its front end.
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include "ea/editor/EditCommands.hpp"
#include "ea/repository/Repository.hpp"

namespace fs = std::filesystem;
using namespace ea::editor;

// Test double for the output port: records what the engine reports
struct RecordingObserver : DiagramObserver {
    std::vector<std::string> events;
    void elementShown(const std::shared_ptr<ea::DiagramObject>& obj, const std::shared_ptr<ea::Element>&) override {
        events.push_back("shown:" + std::to_string(obj->GetElementID()));
    }
    void elementHidden(int id) override { events.push_back("hidden:" + std::to_string(id)); }
    void connectorShown(const std::shared_ptr<ea::DiagramLink>&, const std::shared_ptr<ea::Connector>& c) override {
        events.push_back("conn-shown:" + std::to_string(c->GetConnectorID()));
    }
    void connectorHidden(int id) override { events.push_back("conn-hidden:" + std::to_string(id)); }
    void geometryChanged(int id, const ElementGeometry& g) override {
        events.push_back("geometry:" + std::to_string(id) + "@" + std::to_string(g.left));
    }
    void elementChanged(int id) override { events.push_back("changed:" + std::to_string(id)); }
    void modelEdited() override { events.push_back("model"); }

    bool saw(const std::string& e) const { return std::find(events.begin(), events.end(), e) != events.end(); }
};

// Test double for the input port: delegates to a real repository, but can be told to fail one operation
struct FailingGateway : ModelGateway {
    ModelGateway& real;
    std::string failOn;
    explicit FailingGateway(ModelGateway& r) : real(r) {}
    bool fails(const char* op) const { return failOn == op; }

    std::shared_ptr<ea::Element> GetElementByID(int id) override { return real.GetElementByID(id); }
    std::shared_ptr<ea::Connector> GetConnectorByID(int id) override { return real.GetConnectorByID(id); }
    std::shared_ptr<ea::Element> AddElement(ea::Element& e) override { return fails("AddElement") ? nullptr : real.AddElement(e); }
    bool SaveElement(ea::Element& e) override { return !fails("SaveElement") && real.SaveElement(e); }
    bool DeleteElement(int id) override { return !fails("DeleteElement") && real.DeleteElement(id); }
    std::shared_ptr<ea::Connector> AddConnector(ea::Connector& c) override { return fails("AddConnector") ? nullptr : real.AddConnector(c); }
    bool DeleteConnector(int id) override { return !fails("DeleteConnector") && real.DeleteConnector(id); }
    bool SaveConnector(ea::Connector& c) override { return !fails("SaveConnector") && real.SaveConnector(c); }
    bool AddElementToDiagram(ea::Diagram& d, const std::shared_ptr<ea::DiagramObject>& o) override {
        return !fails("AddElementToDiagram") && real.AddElementToDiagram(d, o);
    }
    bool RemoveElementFromDiagram(ea::Diagram& d, int id) override {
        return !fails("RemoveElementFromDiagram") && real.RemoveElementFromDiagram(d, id);
    }
    bool AddConnectorToDiagram(ea::Diagram& d, const std::shared_ptr<ea::DiagramLink>& l) override {
        return !fails("AddConnectorToDiagram") && real.AddConnectorToDiagram(d, l);
    }
    bool DeleteConnectorFromDiagram(ea::Diagram& d, int id) override { return real.DeleteConnectorFromDiagram(d, id); }
    bool RemoveConnectorFromDiagram(ea::Diagram& d, int id) override { return real.RemoveConnectorFromDiagram(d, id); }
    bool SaveDiagramLink(const ea::DiagramLink& l) override { return !fails("SaveDiagramLink") && real.SaveDiagramLink(l); }
};

static std::shared_ptr<ea::Element> newClass(const std::string& name, int packageId) {
    auto e = std::make_shared<ea::Element>();
    e->SetName(name);
    e->SetType(ea::ObjectType::Class);
    e->SetPackageID(packageId);
    return e;
}

static bool onDiagram(const ea::Diagram& d, int elementId) {
    for (const auto& o : d.GetDiagramObjects()) {
        if (o && o->GetElementID() == elementId) return true;
    }
    return false;
}

int main() {
    std::cout << "==========================================\n";
    std::cout << " Running onlyArchitect Editor Engine Tests (no Qt)\n";
    std::cout << "==========================================\n";

    fs::path db = fs::temp_directory_path() / "onlyarchitect_test_editor.qea";
    fs::remove(db);
    ea::Repository repo;
    assert(repo.CreateRepository(db.string()));
    const int pkg = repo.GetModels().GetAt(0)->GetPackageID();
    auto diagram = repo.CreateDiagram(pkg, "Engine", ea::DiagramType::Logical);
    assert(diagram);

    RecordingObserver observer;
    FailingGateway gateway(repo);
    EditContext ctx{gateway, diagram, observer};

    // 1. Create: model + diagram + notifications; undo removes it from both
    AddElementCommand addA(ctx, newClass("A", pkg), ElementGeometry{10, -10, 180, -110});
    addA.execute();
    const int a = addA.elementId();
    assert(a > 0 && repo.GetElementByID(a) && onDiagram(*diagram, a));
    assert(observer.saw("shown:" + std::to_string(a)) && observer.saw("model"));
    addA.undo();
    assert(!repo.GetElementByID(a) && !onDiagram(*diagram, a));
    assert(observer.saw("hidden:" + std::to_string(a)));
    addA.execute(); // redo keeps the ID
    assert(addA.elementId() == a && repo.GetElementByID(a)->GetName() == "A");
    std::cout << "  ✓ AddElementCommand: execute / undo / redo\n";

    // 2. A persistence failure halfway leaves the model as it was (rollback) and reports it
    gateway.failOn = "AddElementToDiagram";
    observer.events.clear();
    AddElementCommand addBroken(ctx, newClass("Broken", pkg), ElementGeometry{0, 0, 170, -100});
    bool threw = false;
    try {
        addBroken.execute();
    } catch (const CommandError& e) {
        threw = true;
        assert(std::string(e.what()).find("Broken") != std::string::npos);
    }
    assert(threw && "a failed step must surface as CommandError");
    assert(addBroken.elementId() > 0 && !repo.GetElementByID(addBroken.elementId()) && "element saved in step 1 is rolled back");
    assert(observer.events.empty() && "the presentation is not told about an edit that did not happen");
    gateway.failOn.clear();
    std::cout << "  ✓ Failure in the middle of a command rolls back and throws CommandError\n";

    // 3. Connectors; removing an element and undoing brings its connectors back
    AddElementCommand addB(ctx, newClass("B", pkg), ElementGeometry{300, -10, 470, -110});
    addB.execute();
    const int b = addB.elementId();
    auto conn = std::make_shared<ea::Connector>();
    conn->SetType(ea::ConnectorType::Association);
    conn->SetClientID(a);
    conn->SetSupplierID(b);
    AddConnectorCommand link(ctx, conn);
    link.execute();
    const int c = link.connectorId();
    assert(repo.GetConnectorByID(c));

    RemoveElementFromDiagramCommand removeB(ctx, b);
    removeB.execute();
    assert(!onDiagram(*diagram, b) && repo.GetElementByID(b) && "removal keeps the element in the model");
    observer.events.clear();
    removeB.undo();
    assert(onDiagram(*diagram, b));
    assert(observer.saw("shown:" + std::to_string(b)) && observer.saw("conn-shown:" + std::to_string(c)));
    std::cout << "  ✓ RemoveElementFromDiagramCommand: undo restores the symbol and its connectors\n";

    // 4. Hide / show a connector
    HideConnectorCommand hide(ctx, c);
    hide.execute();
    assert(observer.saw("conn-hidden:" + std::to_string(c)));
    gateway.failOn = "SaveDiagramLink";
    threw = false;
    try { hide.undo(); } catch (const CommandError&) { threw = true; }
    assert(threw);
    for (const auto& l : diagram->GetDiagramLinks()) {
        if (l->GetConnectorID() == c) assert(l->IsHidden() && "failed unhide leaves the link hidden");
    }
    gateway.failOn.clear();
    hide.undo();
    link.undo();
    assert(!repo.GetConnectorByID(c));
    std::cout << "  ✓ HideConnectorCommand / AddConnectorCommand undo, with failure handling\n";

    // 5. Geometry: execute is idempotent, so the same command serves a drag already on screen
    ChangeGeometryCommand move(ctx, {{a, ElementGeometry{10, -10, 180, -110}, ElementGeometry{50, -60, 220, -160}}});
    move.execute();
    move.execute();
    for (const auto& o : diagram->GetDiagramObjects()) {
        if (o->GetElementID() == a) assert(ElementGeometry::of(*o) == (ElementGeometry{50, -60, 220, -160}));
    }
    move.undo();
    for (const auto& o : diagram->GetDiagramObjects()) {
        if (o->GetElementID() == a) assert(ElementGeometry::of(*o) == (ElementGeometry{10, -10, 180, -110}));
    }
    assert(move.description() == "Mover elemento");
    std::cout << "  ✓ ChangeGeometryCommand: idempotent execute, undo\n";

    // 6. Editing properties; a failed save restores the previous state in memory
    auto elemA = repo.GetElementByID(a);
    auto before = ElementSnapshot::capture(*elemA);
    auto after = before;
    after.name = "Cliente";
    after.attributes.emplace_back();
    after.attributes.back().SetName("id");
    EditElementCommand rename(ctx, a, before, after, "Renombrar");
    rename.execute();
    assert(elemA->GetName() == "Cliente" && elemA->GetAttributes().size() == 1);
    rename.undo();
    assert(elemA->GetName() == "A" && elemA->GetAttributes().size() == 0);
    gateway.failOn = "SaveElement";
    threw = false;
    try { rename.execute(); } catch (const CommandError&) { threw = true; }
    assert(threw && elemA->GetName() == "A" && "failed save leaves the element untouched");
    gateway.failOn.clear();
    std::cout << "  ✓ EditElementCommand: execute / undo, failed save rolls back\n";

    repo.CloseFile();
    fs::remove(db);
    std::cout << "==========================================\n";
    std::cout << " ALL EDITOR ENGINE TESTS PASSED!\n";
    std::cout << "==========================================\n";
    return 0;
}
