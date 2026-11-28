#ifdef NDEBUG
#undef NDEBUG
#endif
// UML validation (guiaSecuencia.md §1.1 "Validación UML", §3.2 "Validación"). Core only, no Qt.
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include "ea/editor/EditCommands.hpp"
#include "ea/repository/Repository.hpp"
#include "ea/validation/ConnectionRules.hpp"
#include "ea/validation/ModelValidator.hpp"

namespace fs = std::filesystem;
using namespace ea;
using validation::check_connection;
using validation::check_multiplicity;

// Element is neither copyable nor movable: hand out heap instances
static std::shared_ptr<Element> make(ObjectType type, int id, const std::string& name = "E") {
    auto e = std::make_shared<Element>();
    e->SetType(type);
    e->SetElementID(id);
    e->SetName(name);
    return e;
}

static validation::RuleVerdict check(ConnectorType c, const std::shared_ptr<Element>& s, const std::shared_ptr<Element>& t) {
    return check_connection(c, *s, *t);
}

static bool allowed(ConnectorType c, ObjectType s, ObjectType t) {
    return static_cast<bool>(check(c, make(s, 1), make(t, 2)));
}

struct NullObserver : editor::DiagramObserver {
    void elementShown(const std::shared_ptr<DiagramObject>&, const std::shared_ptr<Element>&) override {}
    void elementHidden(int) override {}
    void connectorShown(const std::shared_ptr<DiagramLink>&, const std::shared_ptr<Connector>&) override {}
    void connectorHidden(int) override {}
    void geometryChanged(int, const editor::ElementGeometry&) override {}
    void elementChanged(int) override {}
    void modelEdited() override {}
};

int main(int argc, char* argv[]) {
    std::cout << "==========================================\n";
    std::cout << " Running onlyArchitect UML Validation Tests (no Qt)\n";
    std::cout << "==========================================\n";
    using OT = ObjectType;
    using CT = ConnectorType;

    // 1. The guide's acceptance example: generalization from an interface to a class is refused, with a reason
    auto verdict = check(CT::Generalization, make(OT::Interface, 1, "IRepo"), make(OT::Class, 2, "Repo"));
    assert(!verdict && verdict.reason.find("Realización") != std::string::npos);
    assert(!allowed(CT::Generalization, OT::Class, OT::Interface));
    assert(allowed(CT::Generalization, OT::Class, OT::Class));
    assert(allowed(CT::Generalization, OT::Interface, OT::Interface));
    assert(!check(CT::Generalization, make(OT::Class, 7), make(OT::Class, 7)) && "no self-generalization");
    assert(!allowed(CT::Generalization, OT::Package, OT::Package));
    std::cout << "  ✓ Generalization rules (incl. Interface → Class rejected)\n";

    assert(allowed(CT::Realization, OT::Class, OT::Interface));
    assert(allowed(CT::Realization, OT::Component, OT::Interface));
    assert(allowed(CT::Realization, OT::UseCase, OT::Requirement));   // common in EA models
    assert(allowed(CT::Realization, OT::Class, OT::Class));           // UML Abstraction between classifiers
    assert(!allowed(CT::Realization, OT::Interface, OT::Interface));  // interfaces extend, not realize
    assert(allowed(CT::Realization, OT::Action, OT::Class));          // Abstraction between NamedElements
    std::cout << "  ✓ Realization rules\n";

    assert(allowed(CT::Association, OT::Actor, OT::UseCase));
    assert(allowed(CT::Association, OT::Class, OT::Class));
    assert(check(CT::Association, make(OT::Class, 3), make(OT::Class, 3)) && "reflexive association is fine");
    assert(!allowed(CT::Association, OT::Actor, OT::Actor));
    assert(!allowed(CT::Association, OT::UseCase, OT::UseCase));
    assert(!allowed(CT::Association, OT::Actor, OT::Interface));
    assert(!allowed(CT::Composition, OT::Actor, OT::UseCase));
    assert(allowed(CT::Composition, OT::Class, OT::Class));
    assert(!allowed(CT::Association, OT::Action, OT::Class));
    assert(allowed(CT::Association, OT::Node, OT::Port));             // EA: communication path to a port
    assert(!allowed(CT::Generalization, OT::Port, OT::Port));
    std::cout << "  ✓ Association / Aggregation / Composition rules\n";

    assert(allowed(CT::NoteLink, OT::Note, OT::Class));
    assert(allowed(CT::NoteLink, OT::Class, OT::Note));
    assert(!allowed(CT::NoteLink, OT::Class, OT::Class));
    assert(!allowed(CT::Dependency, OT::Note, OT::Class));
    assert(allowed(CT::Dependency, OT::Class, OT::Package));
    std::cout << "  ✓ Notes only through Note Link\n";

    assert(allowed(CT::Transition, OT::InitialNode, OT::State));
    assert(!allowed(CT::Transition, OT::State, OT::InitialNode));
    assert(!allowed(CT::Transition, OT::FinalState, OT::State));
    assert(!allowed(CT::Transition, OT::Class, OT::State));
    assert(allowed(CT::ControlFlow, OT::Action, OT::Decision));
    assert(!allowed(CT::ControlFlow, OT::Action, OT::InitialNode));
    assert(!allowed(CT::PackageImport, OT::Class, OT::Package));
    assert(allowed(CT::Assembly, OT::Component, OT::Component));
    std::cout << "  ✓ State machine, activity, package and component rules\n";

    // Kinds the model does not recognise, and elements extended by a profile (ArchiMate, BPMN...), are not judged
    assert(allowed(CT::Generalization, OT::Unknown, OT::Class));
    assert(allowed(CT::Association, OT::Unknown, OT::Actor));
    {
        auto goal = make(OT::Class, 1, "Revenue is declining");
        goal->SetStereotype("ArchiMate_Assessment");
        auto outcome = make(OT::Class, 2, "Profitability is declining");
        outcome->SetStereotype("ArchiMate_Assessment");
        assert(check(CT::ControlFlow, goal, outcome) && "profile elements: their MDG decides");
        assert(!check(CT::ControlFlow, goal, make(OT::Class, 5)) && "a plain Class end is still judged");
        auto entity = make(OT::Class, 3, "Cliente");
        entity->SetStereotype("entity"); // UML standard stereotype: still a Class
        assert(!check(CT::Generalization, make(OT::Interface, 4), entity));
        // EA activity initial/final nodes are StateNodes, loaded as State
        assert(allowed(CT::ControlFlow, OT::State, OT::Action));
    }
    std::cout << "  ✓ Unrecognised kinds and profile-extended elements are not flagged\n";

    // 2. Multiplicities
    for (const char* ok : {"", "1", "*", "0..1", "1..*", "0..*", "2..5", "3", "0..n", "[BookCount]", " 1 .. * "}) {
        assert(check_multiplicity(ok) && ok);
    }
    for (const char* bad : {"1..", "..3", "*..1", "5..2", "-1", "0..-3"}) {
        assert(!check_multiplicity(bad) && bad);
    }
    std::cout << "  ✓ Multiplicity syntax (bounds, order, expressions)\n";

    // 3. Through the editor engine: a rule violation creates nothing; a generalization cycle is refused
    fs::path db = fs::temp_directory_path() / "onlyarchitect_test_validation.qea";
    fs::remove(db);
    Repository repo;
    assert(repo.CreateRepository(db.string()));
    const int pkg = repo.GetModels().GetAt(0)->GetPackageID();
    auto diagram = repo.CreateDiagram(pkg, "Rules", DiagramType::Logical);
    NullObserver observer;
    editor::EditContext ctx{repo, diagram, observer};

    auto addElement = [&](ObjectType type, const std::string& name) {
        auto e = std::make_shared<Element>();
        e->SetType(type);
        e->SetName(name);
        e->SetPackageID(pkg);
        editor::AddElementCommand cmd(ctx, e, editor::ElementGeometry{0, 0, 170, -100});
        cmd.execute();
        return cmd.elementId();
    };
    auto connect = [&](int from, int to, ConnectorType type) {
        auto c = std::make_shared<Connector>();
        c->SetType(type);
        c->SetClientID(from);
        c->SetSupplierID(to);
        editor::AddConnectorCommand cmd(ctx, c);
        cmd.execute();
        return cmd.connectorId();
    };
    auto rejected = [&](int from, int to, ConnectorType type) {
        try {
            connect(from, to, type);
        } catch (const editor::RuleViolation& e) {
            return std::string(e.what());
        }
        return std::string();
    };

    const int iface = addElement(OT::Interface, "IRepositorio");
    const int cls = addElement(OT::Class, "Repositorio");
    const auto countsBefore = repo.GetCounts().connectors;
    assert(!rejected(iface, cls, CT::Generalization).empty());
    assert(repo.GetCounts().connectors == countsBefore && "a refused relationship is not stored");
    assert(diagram->GetDiagramLinks().size() == 0);
    assert(connect(cls, iface, CT::Realization) > 0 && "the right relationship is accepted");

    const int a = addElement(OT::Class, "A");
    const int b = addElement(OT::Class, "B");
    const int c = addElement(OT::Class, "C");
    connect(a, b, CT::Generalization);
    connect(b, c, CT::Generalization);
    const auto cycle = rejected(c, a, CT::Generalization);
    assert(cycle.find("ciclo") != std::string::npos && "C → A would make A inherit from itself");
    assert(rejected(a, c, CT::Generalization).empty() && "a redundant but acyclic generalization is allowed");
    std::cout << "  ✓ AddConnectorCommand enforces the rules and refuses inheritance cycles\n";

    // 4. Whole-model validation finds problems that got into the model by other means (import, SQL, old versions)
    {
        auto dup = std::make_shared<Element>();
        dup->SetType(OT::Class);
        dup->SetName("A"); // same package, same kind, same name as A
        dup->SetPackageID(pkg);
        repo.AddElement(*dup);
        auto noteDep = std::make_shared<Element>();
        noteDep->SetType(OT::Note);
        noteDep->SetPackageID(pkg);
        auto note = repo.AddElement(*noteDep);

        Connector illegal; // stored directly, bypassing the editor's rules
        illegal.SetType(CT::Dependency);
        illegal.SetClientID(note->GetElementID());
        illegal.SetSupplierID(cls);
        repo.AddConnector(illegal);

        Connector loop; // closes A → B → C → A
        loop.SetType(CT::Generalization);
        loop.SetClientID(c);
        loop.SetSupplierID(a);
        repo.AddConnector(loop);

        Connector badMult;
        badMult.SetType(CT::Association);
        badMult.SetClientID(a);
        badMult.SetSupplierID(cls);
        badMult.SetSupplierMultiplicity("5..2");
        repo.AddConnector(badMult);
    }
    auto report = validation::validate_model(repo.GetModels());
    std::map<std::string, int> byRule;
    for (const auto& issue : report.issues) ++byRule[issue.rule];
    assert(byRule["unique-name"] == 1);
    assert(byRule["relationship-rule"] == 1);
    assert(byRule["generalization-cycle"] >= 1);
    assert(byRule["multiplicity"] == 1);
    const size_t warnings = byRule["missing-name"] + byRule["unresolved-end"] + byRule["unique-name"];
    assert(report.count(validation::Severity::Warning) == warnings);
    assert(report.count(validation::Severity::Error) == report.issues.size() - warnings);
    assert(report.issues.front().severity == validation::Severity::Error && "errors are listed first");
    std::cout << "  ✓ validate_model: duplicate names, illegal relationship, cycle, bad multiplicity\n";
    repo.CloseFile();
    fs::remove(db);

    // 5. Optional external large model benchmark (argv[1] or ONLYARCHITECT_SAMPLE_QEA)
    const char* envSample = std::getenv("ONLYARCHITECT_SAMPLE_QEA");
    fs::path sample = argc > 1 ? fs::path(argv[1]) : (envSample && *envSample ? fs::path(envSample) : fs::path{});
    if (!sample.empty() && fs::exists(sample)) {
        Repository ea;
        assert(ea.OpenFile(sample.string()));
        const auto t0 = std::chrono::steady_clock::now();
        auto big = validation::validate_model(ea.GetModels(), [&ea](int id) { return ea.GetElementByID(id); });
        const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        std::map<std::string, int> rules;
        for (const auto& issue : big.issues) ++rules[issue.rule];
        std::cout << "  ✓ Sample .qea: " << big.elementsChecked << " elementos, " << big.connectorsChecked
                  << " relaciones en " << ms << " ms\n";
        for (const auto& [rule, n] : rules) std::cout << "      " << rule << ": " << n << "\n";
        assert(big.elementsChecked > 1000);
        assert(ms < 2000 && "guide: a 1000-element project must be handled in < 2 s");
    }

    std::cout << "==========================================\n";
    std::cout << " ALL UML VALIDATION TESTS PASSED!\n";
    std::cout << "==========================================\n";
    return 0;
}
