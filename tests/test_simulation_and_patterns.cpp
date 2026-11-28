#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <iostream>
#include "ea/editor/ModelPatternService.hpp"
#include "ea/export/ModelDocExporter.hpp"
#include "ea/repository/Repository.hpp"
#include "ea/simulation/SimulationEngine.hpp"

namespace fs = std::filesystem;

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Running SimulationEngine & ModelPatternService Tests  \n";
    std::cout << "=======================================================\n";

    const fs::path dbPath = fs::temp_directory_path() / "onlyarchitect_test_sim_patterns.qea";
    fs::remove(dbPath);

    ea::Repository repo;
    assert(repo.CreateRepository(dbPath.string()));

    // 1. Instantiate an Activity pattern and simulate its real nodes/edges
    const auto actRes = ea::editor::ModelPatternService::instantiatePattern(
        repo, 0, "Activity Diagrams", "Basic Activity Diagram with Decisions");
    assert(actRes.ok && actRes.packageId > 0 && actRes.diagramId > 0);
    assert(actRes.elementsCreated == 5 && actRes.connectorsCreated == 4);

    const auto actPlan = ea::simulation::SimulationEngine::buildFromDiagram(repo, actRes.diagramId);
    assert(!actPlan.empty());
    assert(actPlan.nodes.size() == 5);
    assert(actPlan.edges.size() == 4);
    assert(actPlan.steps.size() == 5);
    assert(actPlan.steps.front().nodeKind == "Initial");
    assert(actPlan.steps.back().nodeKind == "Final");
    std::cout << "  ✓ Activity pattern instantiated & simulated (" << actPlan.steps.size() << " steps)\n";

    // 2. Instantiate a Sequence pattern and verify message-order simulation
    const auto seqRes = ea::editor::ModelPatternService::instantiatePattern(
        repo, 0, "Sequence Diagrams", "Basic Sequence Diagram with Synchronous Message");
    assert(seqRes.ok && seqRes.elementsCreated == 3 && seqRes.connectorsCreated == 4);

    const auto seqPlan = ea::simulation::SimulationEngine::buildFromDiagram(repo, seqRes.diagramId);
    assert(seqPlan.nodes.size() == 3);
    assert(seqPlan.edges.size() == 4);
    assert(seqPlan.steps.size() == 4);
    assert(seqPlan.steps.front().logMessage.find("solicitar()") != std::string::npos);
    assert(seqPlan.steps.back().logMessage.find("respuesta") != std::string::npos);
    std::cout << "  ✓ Sequence pattern instantiated & simulated in SeqNo order (" << seqPlan.steps.size() << " steps)\n";

    // 3. Instantiate a Use Case pattern (Boundary is decorative and skipped by simulation nodes)
    const auto ucRes = ea::editor::ModelPatternService::instantiatePattern(
        repo, 0, "Use Case Diagrams", "Basic Use Case Model with Extend");
    assert(ucRes.ok && ucRes.elementsCreated == 4 && ucRes.connectorsCreated == 2);

    const auto ucPlan = ea::simulation::SimulationEngine::buildFromDiagram(repo, ucRes.diagramId);
    assert(ucPlan.nodes.size() == 3 && "Boundary must not be simulated as an execution node");
    assert(ucPlan.edges.size() == 2);
    std::cout << "  ✓ Use Case pattern instantiated (Boundary excluded from sim nodes)\n";

    // 4. Instantiate a Class pattern with attributes and operations
    const auto clsRes = ea::editor::ModelPatternService::instantiatePattern(
        repo, 0, "Class Diagrams", "Basic Class Diagram with Attributes and Operations");
    assert(clsRes.ok && clsRes.elementsCreated == 2 && clsRes.connectorsCreated == 1);
    std::cout << "  ✓ Class pattern instantiated with real attributes, operations and generalization\n";

    // 5. Export full model architectural documentation to Markdown
    const std::string md = ea::ModelDocExporter::exportMarkdown(repo);
    assert(md.find("# Documentación de Arquitectura") != std::string::npos);
    assert(md.find("EntidadBase") != std::string::npos);
    assert(md.find("validar()") != std::string::npos);
    assert(md.find("Basic Activity Diagram with Decisions") != std::string::npos);
    std::cout << "  ✓ ModelDocExporter generated complete Markdown documentation (" << md.size() << " bytes)\n";

    repo.CloseFile();
    fs::remove(dbPath);

    std::cout << "=======================================================\n";
    std::cout << " ALL SIMULATION & MODEL PATTERN TESTS PASSED!          \n";
    std::cout << "=======================================================\n";
    return 0;
}
