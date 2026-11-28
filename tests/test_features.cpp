#ifdef NDEBUG
#undef NDEBUG
#endif
// Property Editor core (no Qt): feature notation and persistence of attributes/operations
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include "ea/editor/EditCommands.hpp"
#include "ea/editor/FeatureText.hpp"
#include "ea/repository/Repository.hpp"

namespace fs = std::filesystem;
using namespace ea;
using namespace ea::editor;

namespace {

struct NullObserver : DiagramObserver {
    int changed = 0;
    void elementShown(const std::shared_ptr<DiagramObject>&, const std::shared_ptr<Element>&) override {}
    void elementHidden(int) override {}
    void connectorShown(const std::shared_ptr<DiagramLink>&, const std::shared_ptr<Connector>&) override {}
    void connectorHidden(int) override {}
    void geometryChanged(int, const ElementGeometry&) override {}
    void elementChanged(int) override { ++changed; }
    void modelEdited() override {}
};

std::string value(Repository& repo, const std::string& sql, const std::string& column) {
    const std::string xml = repo.SQLQuery(sql);
    const auto open = xml.find("<" + column + ">");
    if (open == std::string::npos) return "";
    const auto start = open + column.size() + 2;
    return xml.substr(start, xml.find("</" + column + ">", start) - start);
}

} // namespace

int main(int argc, char* argv[]) {
    std::cout << "==========================================\n";
    std::cout << " Running onlyArchitect Feature Editing Tests (no Qt)\n";
    std::cout << "==========================================\n";

    // 1. Notation
    assert(parse_visibility("-") == Scope::Private && parse_visibility(" Protected ") == Scope::Protected);
    assert(parse_visibility("~") == Scope::Package && !parse_visibility("?"));
    for (auto s : {Scope::Public, Scope::Private, Scope::Protected, Scope::Package})
        assert(parse_visibility(visibility_symbol(s)) == s);
    assert((parse_multiplicity("0..*") == Multiplicity{"0", "*"}));
    assert((parse_multiplicity("*") == Multiplicity{"0", "*"}));
    assert((parse_multiplicity("3") == Multiplicity{"3", "3"}));
    assert((parse_multiplicity("") == Multiplicity{"1", "1"}));
    assert(!parse_multiplicity("2..1") && !parse_multiplicity("*..1") && !parse_multiplicity("a") && !parse_multiplicity("1..x"));
    for (const char* m : {"1", "0..1", "*", "1..*", "2..5"}) assert(format_multiplicity(*parse_multiplicity(m)) == m);

    auto params = parse_parameters("a: int, out b: std::string = \"x, y\", inout c");
    assert(params.ok() && params.parameters.size() == 3);
    assert(params.parameters[0].name == "a" && params.parameters[0].type == "int" && params.parameters[0].kind == "in");
    assert(params.parameters[1].kind == "out" && params.parameters[1].type == "std::string" &&
           params.parameters[1].default_value == "\"x, y\"" && params.parameters[1].pos == 1);
    assert(params.parameters[2].kind == "inout" && params.parameters[2].type.empty());
    assert(format_parameters(params.parameters) == "a: int, out b: std::string = \"x, y\", inout c");
    assert(parse_parameters("").ok() && parse_parameters("").parameters.empty());
    assert(!parse_parameters("a: int,").ok() && !parse_parameters("two words: int").ok() && !parse_parameters(": int").ok());
    assert(is_valid_feature_name("x") && !is_valid_feature_name("") && !is_valid_feature_name("  ") &&
           !is_valid_feature_name("a\nb"));
    std::cout << "  ✓ Visibility, multiplicity and parameter notation parse and format symmetrically\n";

    fs::path db = fs::temp_directory_path() / "onlyarchitect_test_features.qea";
    fs::remove(db);
    int classId = 0;
    {
        Repository repo;
        assert(repo.CreateRepository(db.string()));
        Element proto;
        proto.SetType(ObjectType::Class);
        proto.SetName("Pedido");
        proto.SetPackageID(repo.GetModels().GetAt(0)->GetPackageID());
        auto cls = repo.AddElement(proto);
        classId = cls->GetElementID();
        NullObserver observer;
        EditContext ctx{repo, nullptr, observer};

        // 2. An edit made through the command is persisted: attributes, operations, parameters, alias
        auto cmd = make_edit_element_command(ctx, *cls, [](Element& e) {
            e.SetAlias("Order");
            auto id = e.GetAttributes().AddNew("id", "long");
            id->SetVisibility(Scope::Private);
            auto lines = e.GetAttributes().AddNew("lineas", "Linea");
            lines->SetLowerBound("0");
            lines->SetUpperBound("*");
            lines->SetStatic(true);
            auto total = e.GetMethods().AddNew("total", "double");
            total->SetParameters(parse_parameters("moneda: string = \"EUR\", out redondeo: int").parameters);
            total->SetAbstract(true);
        }, "Definir Pedido");
        assert(cmd && cls->GetAttributes().size() == 0 && "the factory leaves the element untouched");
        cmd->execute();
        assert(cls->GetAttributes().size() == 2 && cls->GetMethods().size() == 1 && observer.changed == 1);
        assert(!make_edit_element_command(ctx, *cls, [](Element&) {}, "nada") && "no change → no command");

        const auto id = std::to_string(classId);
        assert(value(repo, "SELECT count(*) AS n FROM t_attribute WHERE Object_ID=" + id, "n") == "2");
        assert(value(repo, "SELECT UpperBound FROM t_attribute WHERE Name='lineas'", "UpperBound") == "*");
        assert(value(repo, "SELECT Scope FROM t_attribute WHERE Name='id'", "Scope") == "Private");
        assert(value(repo, "SELECT count(*) AS n FROM t_operationparams p JOIN t_operation o ON o.OperationID=p.OperationID "
                           "WHERE o.Object_ID=" + id, "n") == "2");
        assert(value(repo, "SELECT Alias FROM t_object WHERE Object_ID=" + id, "Alias") == "Order");
        // Feature ids were assigned and kept on the live objects (stable across later saves)
        const int attrId = cls->GetAttributes().GetAt(0)->GetAttributeID();
        assert(attrId > 0);

        // 3. Rename + reorder + delete: rows are updated in place, removed ones deleted
        auto edit = make_edit_element_command(ctx, *cls, [](Element& e) {
            e.GetAttributes().GetAt(0)->SetName("codigo");
            e.GetMethods().Delete(0);
        }, "Editar");
        edit->execute();
        assert(value(repo, "SELECT Name FROM t_attribute WHERE ID=" + std::to_string(attrId), "Name") == "codigo");
        assert(value(repo, "SELECT count(*) AS n FROM t_operation WHERE Object_ID=" + id, "n") == "0");
        assert(value(repo, "SELECT count(*) AS n FROM t_operationparams", "n") == "0" && "no orphan parameters");
        edit->undo(); // the operation comes back with its parameters
        assert(value(repo, "SELECT count(*) AS n FROM t_operation WHERE Object_ID=" + id, "n") == "1");
        assert(value(repo, "SELECT count(*) AS n FROM t_operationparams", "n") == "2");
        assert(value(repo, "SELECT Name FROM t_attribute WHERE ID=" + std::to_string(attrId), "Name") == "id");
        repo.CloseFile();
    }
    {
        // 4. Everything reads back after reopening
        Repository repo;
        assert(repo.OpenFile(db.string()));
        auto cls = repo.GetElementByID(classId);
        assert(cls && cls->GetAlias() == "Order");
        assert(cls->GetAttributes().size() == 2);
        auto lines = cls->GetAttributes().GetAt(1);
        assert(lines->GetName() == "lineas" && lines->IsStatic() && lines->GetLowerBound() == "0" && lines->GetUpperBound() == "*");
        auto total = cls->GetMethods().GetAt(0);
        assert(total->IsAbstract() && total->GetReturnType() == "double");
        assert(format_parameters(total->GetParameters()) == "moneda: string = \"EUR\", out redondeo: int");
        repo.CloseFile();
    }
    fs::remove(db);
    std::cout << "  ✓ Attributes, operations, parameters and alias persist (insert, update in place, delete, undo)\n";

    // 5. Optional external model check (argv[1] or ONLYARCHITECT_SAMPLE_QEA)
    const char* envSample = std::getenv("ONLYARCHITECT_SAMPLE_QEA");
    fs::path sample = argc > 1 ? fs::path(argv[1]) : (envSample && *envSample ? fs::path(envSample) : fs::path{});
    if (!sample.empty() && fs::exists(sample)) {
        fs::path copy = fs::temp_directory_path() / "onlyarchitect_test_features_ea.qea";
        fs::copy_file(sample, copy, fs::copy_options::overwrite_existing);
        Repository repo;
        assert(repo.OpenFile(copy.string()));
        const auto attrId = value(repo, "SELECT ID FROM t_attribute WHERE Stereotype IS NOT NULL AND Stereotype<>'' "
                                        "AND Object_ID IN (SELECT Object_ID FROM t_object WHERE ParentID=0) LIMIT 1", "ID");
        assert(!attrId.empty());
        const auto ownerId = std::stoi(value(repo, "SELECT Object_ID FROM t_attribute WHERE ID=" + attrId, "Object_ID"));
        const auto stereotype = value(repo, "SELECT Stereotype FROM t_attribute WHERE ID=" + attrId, "Stereotype");
        const auto attrCount = value(repo, "SELECT count(*) AS n FROM t_attribute WHERE Object_ID=" + std::to_string(ownerId), "n");
        auto owner = repo.GetElementByID(ownerId);
        assert(owner && std::to_string(owner->GetAttributes().size()) == attrCount);
        owner->SetNotes("edited by onlyArchitect");
        assert(repo.SaveElement(*owner));
        assert(value(repo, "SELECT Stereotype FROM t_attribute WHERE ID=" + attrId, "Stereotype") == stereotype);
        assert(value(repo, "SELECT count(*) AS n FROM t_attribute WHERE Object_ID=" + std::to_string(ownerId), "n") == attrCount);
        repo.CloseFile();
        fs::remove(copy);
        std::cout << "  ✓ Saving an EA element preserves unmodelled attribute columns and ids\n";
    }

    std::cout << "==========================================\n";
    std::cout << " ALL FEATURE EDITING TESTS PASSED!\n";
    std::cout << "==========================================\n";
    return 0;
}
