#include <iostream>
#include <cassert>
#include <filesystem>
#include "ea/repository/Repository.hpp"
#include "ea/repository/SqliteStorageEngine.hpp"
#include "ea/diagram/DiagramRenderer.hpp"
#include "ea/export/Exporters.hpp"

namespace fs = std::filesystem;

void TestRealEADatabase(const std::string& path) {
    std::cout << "[Test 1] Opening test repository database: " << path << "...\n";
    ea::Repository repo;
    bool ok = repo.OpenFile(path);
    assert(ok && "Failed to open repository database");

    std::cout << "  - Library version: " << repo.GetLibraryVersion() << "\n";
    auto counts = repo.GetCounts();
    std::cout << "  - Packages: " << counts.packages << "\n";
    std::cout << "  - Elements: " << counts.elements << "\n";
    std::cout << "  - Diagrams: " << counts.diagrams << "\n";
    std::cout << "  - Connectors: " << counts.connectors << "\n";
    assert(counts.packages > 0);
    assert(counts.elements > 0);

    // Test Model loading
    assert(repo.GetModels().size() > 0);
    auto root_model = repo.GetModels().GetAt(0);
    assert(root_model != nullptr);
    std::cout << "  - Root model name: " << root_model->GetName() << "\n";

    // Test Diagram 11 ("Abstract Class Model")
    auto diag = repo.GetDiagramByID(11);
    if (diag) {
        std::cout << "  - Found Diagram 11: \"" << diag->GetName()
                  << "\" (" << ea::to_string(diag->GetType()) << ")\n";
        std::cout << "  - Diagram objects: " << diag->GetDiagramObjects().size() << "\n";
        std::cout << "  - Diagram links: " << diag->GetDiagramLinks().size() << "\n";
        assert(diag->GetDiagramObjects().size() > 0);

        // Test SVG rendering
        ea::DiagramRenderer renderer;
        std::string svg = renderer.RenderToSVG(*diag, repo);
        assert(!svg.empty());
        assert(svg.find("<svg") != std::string::npos);
        assert(svg.find("</svg>") != std::string::npos);
        std::cout << "  - Successfully rendered SVG (size: " << svg.size() << " bytes)\n";

        // Test PlantUML export
        std::string puml = ea::PlantUMLExporter::ExportDiagram(*diag, repo);
        assert(!puml.empty());
        assert(puml.find("@startuml") != std::string::npos);
        assert(puml.find("@enduml") != std::string::npos);
        std::cout << "  - Successfully exported PlantUML (size: " << puml.size() << " bytes)\n";
    }

    std::cout << "[Test 1 PASSED]\n\n";
}

void TestCreateAndSaveRepository(const std::string& new_path) {
    std::cout << "[Test 2] Creating new blank repository at: " << new_path << "...\n";
    if (fs::exists(new_path)) {
        fs::remove(new_path);
    }

    ea::SqliteStorageEngine engine;
    bool ok = engine.CreateNewRepository(new_path);
    assert(ok && "Failed to create new repository");

    ea::Repository repo;
    ok = repo.OpenFile(new_path);
    assert(ok && "Failed to open newly created repository");

    // Add a new Package
    auto root = repo.GetModels().GetAt(0);
    assert(root != nullptr);

    auto dev_pkg = root->GetPackages().AddNew("ArchitectureModel", "Package");
    assert(dev_pkg != nullptr);
    dev_pkg->SetPackageID(2);
    dev_pkg->SetParentID(root->GetPackageID());
    repo.GetStorageEngine()->SavePackage(*dev_pkg);

    // Add a Class Element
    auto cls = dev_pkg->GetElements().AddNew("OrderService", "Class");
    assert(cls != nullptr);
    cls->SetElementID(101);
    cls->SetPackageID(dev_pkg->GetPackageID());
    cls->SetStereotype("Service");

    // Add Attributes
    auto attr = cls->GetAttributes().AddNew("orderId", "int");
    attr->SetVisibility(ea::Scope::Private);

    // Add Methods
    auto meth = cls->GetMethods().AddNew("ProcessOrder", "bool");
    meth->SetVisibility(ea::Scope::Public);

    repo.GetStorageEngine()->SaveElement(*cls);

    // Re-open repository and verify persistence
    repo.CloseFile();
    bool reopened = repo.OpenFile(new_path);
    assert(reopened);

    auto loaded_elem = repo.GetElementByID(101);
    assert(loaded_elem != nullptr);
    assert(loaded_elem->GetName() == "OrderService");
    assert(loaded_elem->GetStereotype() == "Service");
    std::cout << "  - Verified loaded element: " << loaded_elem->GetName()
              << " [Stereotype: " << loaded_elem->GetStereotype() << "]\n";

    std::cout << "[Test 2 PASSED]\n\n";
}

void TestDiagramManipulationAndPersistence(const std::string& db_path) {
    std::cout << "[Test 3] Testing Diagram Manipulation and Database Persistence...\n";
    ea::Repository repo;
    bool ok = repo.OpenFile(db_path);
    assert(ok && "Failed to open repository");

    // Create a new Diagram
    auto root = repo.GetModels().GetAt(0);
    assert(root != nullptr);
    auto dev_pkg = repo.GetPackageByID(2);
    assert(dev_pkg != nullptr);

    auto diag = dev_pkg->GetDiagrams().AddNew("ClassOverviewDiagram", "Logical");
    assert(diag != nullptr);
    diag->SetDiagramID(10);
    diag->SetPackageID(dev_pkg->GetPackageID());

    // Add DiagramObject for OrderService (ID 101)
    auto dobj = diag->GetDiagramObjects().AddNew("OrderService", "Class");
    assert(dobj != nullptr);
    dobj->SetDiagramID(diag->GetDiagramID());
    dobj->SetElementID(101);
    dobj->SetLeft(120);
    dobj->SetTop(-150);
    dobj->SetRight(300);
    dobj->SetBottom(-250);

    // Save diagram to database
    bool saved = repo.GetStorageEngine()->SaveDiagram(*diag);
    assert(saved && "Failed to save initial diagram");

    // Simulate user manipulation in GUI: drag element to (450, -320) and resize
    dobj->SetLeft(450);
    dobj->SetRight(650);
    dobj->SetTop(-320);
    dobj->SetBottom(-440);

    // Save modified coordinates
    saved = repo.GetStorageEngine()->SaveDiagramObject(diag->GetDiagramID(), *dobj);
    assert(saved && "Failed to update manipulated diagram object coordinates");

    // Close and Re-open repository to test cold persistence
    repo.CloseFile();
    bool reopened = repo.OpenFile(db_path);
    assert(reopened);

    auto loaded_diag = repo.GetDiagramByID(10);
    assert(loaded_diag != nullptr);
    assert(loaded_diag->GetDiagramObjects().size() == 1);

    auto loaded_obj = loaded_diag->GetDiagramObjects().GetAt(0);
    assert(loaded_obj != nullptr);
    assert(loaded_obj->GetElementID() == 101);
    assert(loaded_obj->GetLeft() == 450);
    assert(loaded_obj->GetRight() == 650);
    assert(loaded_obj->GetTop() == -320);
    assert(loaded_obj->GetBottom() == -440);

    std::cout << "  - Diagram Object persisted coordinates verified:\n";
    std::cout << "    Left=" << loaded_obj->GetLeft()
              << " Right=" << loaded_obj->GetRight()
              << " Top=" << loaded_obj->GetTop()
              << " Bottom=" << loaded_obj->GetBottom()
              << " (W=" << loaded_obj->GetWidth() << ", H=" << loaded_obj->GetHeight() << ")\n";

    std::cout << "[Test 3 PASSED]\n\n";
}

int main(int argc, char* argv[]) {
    std::cout << "==========================================\n";
    std::cout << " Running onlyArchitect Test Suite         \n";
    std::cout << "==========================================\n\n";

    std::string sample_db = "my_model.qea";
    if (argc > 1) {
        sample_db = argv[1];
    }

    if (fs::exists(sample_db)) {
        TestRealEADatabase(sample_db);
    } else {
        std::cout << "Warning: sample database " << sample_db << " not found, skipping Test 1.\n";
    }

    TestCreateAndSaveRepository("/tmp/test_onlyarchitect.qea");
    TestDiagramManipulationAndPersistence("/tmp/test_onlyarchitect.qea");

    std::string eapx_file = "/tmp/DiagramaCasosUsoExamen.eapx";
    if (fs::exists(eapx_file)) {
        std::cout << "[Test 4] Testing Direct Opening of MS Access Repository (.eapx)...\n";
        ea::Repository repo_eapx;
        bool eapx_ok = repo_eapx.OpenFile(eapx_file);
        assert(eapx_ok && "Should open .eapx repository seamlessly");
        auto counts = repo_eapx.GetCounts();
        std::cout << "  - EAPX Packages: " << counts.packages
                  << ", Elements: " << counts.elements
                  << ", Diagrams: " << counts.diagrams
                  << ", Connectors: " << counts.connectors << "\n";
        assert(counts.elements > 0);
        assert(counts.diagrams == 2);
        std::cout << "[Test 4 PASSED]\n\n";
    }

    std::cout << "==========================================\n";
    std::cout << " ALL TESTS COMPLETED SUCCESSFULLY!        \n";
    std::cout << "==========================================\n";
    return 0;
}
