#ifdef NDEBUG
#undef NDEBUG
#endif
// Contextual toolbox content and EA-faithful storage of what it creates (core only, no Qt)
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <set>
#include "ea/diagram/DiagramPalette.hpp"
#include "ea/repository/Repository.hpp"
#include "ea/validation/ConnectionRules.hpp"

namespace fs = std::filesystem;
using namespace ea;

static std::shared_ptr<Element> elementFrom(const PaletteTool& tool, int id) {
    auto e = std::make_shared<Element>();
    e->SetElementID(id);
    e->SetType(tool.element);
    e->SetStereotype(tool.stereotype);
    e->SetNType(tool.ntype);
    e->SetName(tool.label);
    return e;
}

static std::set<std::string> labels(const Palette& p) {
    std::set<std::string> out;
    for (const auto& g : p.groups) for (const auto& t : g.tools) out.insert(t.label);
    return out;
}

// Single value of a one-row query, as returned in SQLQuery's XML
static std::string value(Repository& repo, const std::string& sql, const std::string& column) {
    const std::string xml = repo.SQLQuery(sql);
    const auto open = xml.find("<" + column + ">");
    if (open == std::string::npos) return "";
    const auto start = open + column.size() + 2;
    return xml.substr(start, xml.find("</" + column + ">", start) - start);
}

int main(int argc, char* argv[]) {
    std::cout << "==========================================\n";
    std::cout << " Running onlyArchitect Toolbox Palette Tests (no Qt)\n";
    std::cout << "==========================================\n";

    // 1. One palette per UML kind, contextual, always ending with the common tools
    std::set<std::string> titles;
    for (const auto& kind : uml_diagram_kinds()) {
        const auto& p = palette_for(&kind);
        assert(p.title != palette_for(nullptr).title && "every UML kind has its own palette");
        titles.insert(p.title);
        assert(p.groups.back().title == "Comunes" && labels(p).count("Note Link"));
    }
    assert(titles.size() == uml_diagram_kinds().size());
    const auto sequence = labels(palette_for(find_uml_diagram_kind("sequence")));
    const auto classes = labels(palette_for(find_uml_diagram_kind("class")));
    for (const char* t : {"Lifeline", "Alt", "Loop", "Message"}) assert(sequence.count(t) && t);
    for (const char* t : {"Class", "Interface", "Generalization", "Realization", "Composition"}) assert(classes.count(t) && t);
    assert(!classes.count("Lifeline") && !sequence.count("Composition") && "only what is valid for the diagram");
    // Existing diagrams resolve by type + stereotype; non-UML ones get the general palette
    assert(palette_for(DiagramType::Sequence, "").title == "Secuencia");
    assert(palette_for(DiagramType::Package, "profile").title == "Perfil");
    assert(palette_for(DiagramType::Analysis, "").title == "General");
    std::cout << "  ✓ 14 contextual palettes (+ general), common tools in every one\n";

    // 2. Coherence with the UML rules: every relationship tool can join some pair of the palette's elements
    for (const auto& kind : uml_diagram_kinds()) {
        const auto& p = palette_for(&kind);
        std::vector<PaletteTool> elements;
        for (const auto& g : p.groups) for (const auto& t : g.tools) if (t.isElement()) elements.push_back(t);
        for (const auto& g : p.groups) {
            for (const auto& r : g.tools) {
                if (r.isElement()) continue;
                bool usable = false;
                for (size_t i = 0; i < elements.size() && !usable; ++i) {
                    for (size_t j = 0; j < elements.size() && !usable; ++j) {
                        usable = static_cast<bool>(validation::check_connection(
                            r.relationship, *elementFrom(elements[i], 1), *elementFrom(elements[j], 2)));
                    }
                }
                if (!usable) std::cerr << "    unusable: " << p.title << " / " << r.label << "\n";
                assert(usable && "a palette offers a relationship no pair of its elements can use");
            }
        }
    }
    std::cout << "  ✓ Every relationship tool is usable with the elements of its own palette\n";

    // 3. What the tools create persists in the .qea schema and reads back identically
    fs::path db = fs::temp_directory_path() / "onlyarchitect_test_palette.qea";
    fs::remove(db);
    {
        Repository repo;
        assert(repo.CreateRepository(db.string()));
        const int pkg = repo.GetModels().GetAt(0)->GetPackageID();
        struct Expect { const char* kind; const char* label; const char* objectType; int ntype; ObjectType loaded; };
        const Expect cases[] = {
            {"activity", "Initial", "StateNode", 100, ObjectType::InitialNode},
            {"activity", "Activity Final", "StateNode", 101, ObjectType::FinalState},
            {"activity", "Flow Final", "StateNode", 102, ObjectType::FinalState},
            {"state_machine", "Initial", "StateNode", 3, ObjectType::InitialNode},
            {"state_machine", "Final", "StateNode", 4, ObjectType::FinalState},
            {"activity", "Fork / Join", "Synchronization", 0, ObjectType::ForkNode},
            {"sequence", "Lifeline", "Sequence", 0, ObjectType::Lifeline},
            {"sequence", "Loop", "InteractionFragment", 4, ObjectType::InteractionFragment},
            {"sequence", "Alt", "InteractionFragment", 0, ObjectType::InteractionFragment},
            {"object", "Object", "Object", 0, ObjectType::Object},
            {"timing", "State Lifeline", "TimeLine", 0, ObjectType::TimeLine},
        };
        std::vector<std::pair<int, const Expect*>> created;
        for (const auto& c : cases) {
            const PaletteTool* tool = nullptr;
            for (const auto& g : palette_for(find_uml_diagram_kind(c.kind)).groups)
                for (const auto& t : g.tools) if (t.label == c.label) tool = &t;
            assert(tool && c.label);
            auto e = elementFrom(*tool, 0);
            e->SetPackageID(pkg);
            auto live = repo.AddElement(*e);
            assert(live);
            const auto id = std::to_string(live->GetElementID());
            assert(value(repo, "SELECT Object_Type FROM t_object WHERE Object_ID=" + id, "Object_Type") == c.objectType);
            assert(value(repo, "SELECT NType FROM t_object WHERE Object_ID=" + id, "NType") == std::to_string(c.ntype));
            created.emplace_back(live->GetElementID(), &c);
        }
        // Relationships: EA's Connector_Type names
        auto uc1 = std::make_shared<Element>(); uc1->SetType(ObjectType::UseCase); uc1->SetPackageID(pkg);
        auto uc2 = std::make_shared<Element>(); uc2->SetType(ObjectType::UseCase); uc2->SetPackageID(pkg);
        const int a = repo.AddElement(*uc1)->GetElementID(), b = repo.AddElement(*uc2)->GetElementID();
        Connector include; include.SetType(ConnectorType::UseCaseLink); include.SetStereotype("include");
        include.SetClientID(a); include.SetSupplierID(b);
        Connector transition; transition.SetType(ConnectorType::Transition); transition.SetClientID(a); transition.SetSupplierID(b);
        const auto inc = std::to_string(repo.AddConnector(include)->GetConnectorID());
        const auto tr = std::to_string(repo.AddConnector(transition)->GetConnectorID());
        assert(value(repo, "SELECT Connector_Type FROM t_connector WHERE Connector_ID=" + inc, "Connector_Type") == "UseCase");
        assert(value(repo, "SELECT Stereotype FROM t_connector WHERE Connector_ID=" + inc, "Stereotype") == "include");
        assert(value(repo, "SELECT Connector_Type FROM t_connector WHERE Connector_ID=" + tr, "Connector_Type") == "StateFlow");
        repo.CloseFile();

        Repository reopened;
        assert(reopened.OpenFile(db.string()));
        for (const auto& [id, c] : created) {
            auto e = reopened.GetElementByID(id);
            assert(e && e->GetType() == c->loaded && e->GetNType() == c->ntype && c->label);
        }
        assert(reopened.GetConnectorByID(std::stoi(inc))->GetType() == ConnectorType::UseCaseLink);
        assert(reopened.GetConnectorByID(std::stoi(tr))->GetType() == ConnectorType::Transition);
        reopened.CloseFile();
    }
    fs::remove(db);
    std::cout << "  ✓ Tools create EA-encoded elements (StateNode/NType, Synchronization, Sequence, UseCase, StateFlow)\n";

    // 4. Optional external model check (argv[1] or ONLYARCHITECT_SAMPLE_QEA)
    const char* envSample = std::getenv("ONLYARCHITECT_SAMPLE_QEA");
    fs::path sample = argc > 1 ? fs::path(argv[1]) : (envSample && *envSample ? fs::path(envSample) : fs::path{});
    if (!sample.empty() && fs::exists(sample)) {
        fs::path copy = fs::temp_directory_path() / "onlyarchitect_test_palette_ea.qea";
        fs::copy_file(sample, copy, fs::copy_options::overwrite_existing);
        Repository repo;
        assert(repo.OpenFile(copy.string()));
        for (const char* where : {"Object_Type='StateNode' AND NType=11",   // choice → loads as State
                                  "Object_Type='Synchronization' AND NType=1", // → loads as ForkNode
                                  "Object_Type='Sequence'"}) {                 // lifeline
            const auto id = value(repo, std::string("SELECT Object_ID FROM t_object WHERE ") + where + " LIMIT 1", "Object_ID");
            assert(!id.empty() && where);
            const auto before = value(repo, "SELECT Object_Type||'/'||NType AS k FROM t_object WHERE Object_ID=" + id, "k");
            auto e = repo.GetElementByID(std::stoi(id));
            e->SetNotes("edited by onlyArchitect");
            assert(repo.SaveElement(*e));
            const auto after = value(repo, "SELECT Object_Type||'/'||NType AS k FROM t_object WHERE Object_ID=" + id, "k");
            assert(before == after && "saving must not change EA's Object_Type/NType");
        }
        repo.CloseFile();
        fs::remove(copy);
        std::cout << "  ✓ Saving EA's StateNode / Synchronization / Sequence elements keeps their encoding\n";
    }

    std::cout << "==========================================\n";
    std::cout << " ALL PALETTE TESTS PASSED!\n";
    std::cout << "==========================================\n";
    return 0;
}
