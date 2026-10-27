#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include "ea/repository/Repository.hpp"
#include "ea/repository/SqliteStorageEngine.hpp"
#include "ea/diagram/DiagramRenderer.hpp"
#include "ea/diagram/DiagramLayout.hpp"
#include "ea/export/Exporters.hpp"

void PrintUsage() {
    std::cout << R"(
===================================================================
 OpenEA — Reverse-Engineered Enterprise Architect C++ Platform
 Compatible with Sparx Systems Enterprise Architect 17.2 (.qea)
===================================================================

Usage:
  openea <command> [options]

Commands:
  info <file.qea>
      Display repository statistics, counts, schema and root models.

  tree <file.qea> [max_depth]
      Print the model hierarchy tree (packages, elements, diagrams).

  render <file.qea> --diagram <id/name> --out <file.svg|png>
      Render a diagram to vector SVG or raster PNG using Cairo.

  layout <file.qea> --diagram <id/name> [--algo force|grid] [--save]
      Run automatic layout engine (Force-directed spring or Grid).

  export <file.qea> --diagram <id/name> --format <plantuml|json|xmi> --out <file>
      Export diagram or package to standard formats.

  query <file.qea> "<SQL>"
      Run arbitrary SQL query on repository and output formatted XML.

  create <new_file.qea>
      Create a new blank EA-compatible repository from scratch.

Examples:
  openea info EAExample.qea
  openea render EAExample.qea --diagram 11 --out diagram11.svg
  openea export EAExample.qea --diagram 11 --format plantuml --out diagram11.puml
)";
}

void PrintTree(const ea::Package& pkg, int depth = 0, int max_depth = 3) {
    if (depth > max_depth) return;
    std::string indent(depth * 2, ' ');
    std::cout << indent << "📁 [Package " << pkg.GetPackageID() << "] " << pkg.GetName() << "\n";

    for (size_t i = 0; i < pkg.GetElements().size(); ++i) {
        auto elem = pkg.GetElements().GetAt(static_cast<short>(i));
        if (!elem) continue;
        std::cout << indent << "   📦 [" << ea::to_string(elem->GetType()) << "] "
                  << elem->GetName()
                  << " (attrs: " << elem->GetAttributes().size()
                  << ", ops: " << elem->GetMethods().size() << ")\n";
    }

    for (size_t i = 0; i < pkg.GetDiagrams().size(); ++i) {
        auto diag = pkg.GetDiagrams().GetAt(static_cast<short>(i));
        if (!diag) continue;
        std::cout << indent << "   📊 [Diagram " << diag->GetDiagramID() << " - "
                  << ea::to_string(diag->GetType()) << "] " << diag->GetName()
                  << " (" << diag->GetDiagramObjects().size() << " elements, "
                  << diag->GetDiagramLinks().size() << " links)\n";
    }

    for (size_t i = 0; i < pkg.GetPackages().size(); ++i) {
        auto sub = pkg.GetPackages().GetAt(static_cast<short>(i));
        if (sub) PrintTree(*sub, depth + 1, max_depth);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    std::string cmd = argv[1];

    if (cmd == "--help" || cmd == "-h" || cmd == "help") {
        PrintUsage();
        return 0;
    }

    if (cmd == "create") {
        if (argc < 3) {
            std::cerr << "Error: missing repository path for create.\n";
            return 1;
        }
        std::string path = argv[2];
        ea::SqliteStorageEngine engine;
        if (engine.CreateNewRepository(path)) {
            std::cout << "Successfully created new Enterprise Architect repository at: " << path << "\n";
            return 0;
        } else {
            std::cerr << "Failed to create repository at: " << path << "\n";
            return 1;
        }
    }

    if (argc < 3) {
        PrintUsage();
        return 1;
    }

    std::string qea_path = argv[2];
    ea::Repository repo;
    if (!repo.OpenFile(qea_path)) {
        std::cerr << "Error: " << repo.GetLastError() << "\n";
        return 1;
    }

    if (cmd == "info") {
        auto counts = repo.GetCounts();
        std::cout << "\n=== Enterprise Architect Repository Information ===\n"
                  << "Database Path:  " << repo.GetConnectionString() << "\n"
                  << "Engine Version: " << repo.GetLibraryVersion() << "\n"
                  << "Root Models:    " << repo.GetModels().size() << "\n"
                  << "Total Packages: " << counts.packages << "\n"
                  << "Total Elements: " << counts.elements << "\n"
                  << "Total Diagrams: " << counts.diagrams << "\n"
                  << "Total Links:    " << counts.connectors << "\n"
                  << "Attributes:     " << counts.attributes << "\n"
                  << "Operations:     " << counts.operations << "\n"
                  << "====================================================\n";
        std::cout << "\nRoot Models in Repository:\n";
        for (size_t i = 0; i < repo.GetModels().size(); ++i) {
            auto m = repo.GetModels().GetAt(static_cast<short>(i));
            if (m) {
                std::cout << "  - [" << m->GetPackageID() << "] " << m->GetName()
                          << " (GUID: " << m->GetPackageGUID() << ")\n";
            }
        }
        return 0;
    }

    if (cmd == "tree") {
        int max_depth = 2;
        if (argc >= 4) {
            try { max_depth = std::stoi(argv[3]); } catch (...) {}
        }
        std::cout << "\n=== Repository Tree (Max Depth: " << max_depth << ") ===\n";
        for (size_t i = 0; i < repo.GetModels().size(); ++i) {
            auto m = repo.GetModels().GetAt(static_cast<short>(i));
            if (m) PrintTree(*m, 0, max_depth);
        }
        return 0;
    }

    if (cmd == "query") {
        if (argc < 4) {
            std::cerr << "Error: missing SQL query string.\n";
            return 1;
        }
        std::string sql = argv[3];
        std::cout << repo.SQLQuery(sql) << "\n";
        return 0;
    }

    if (cmd == "render") {
        std::string diag_id_str;
        std::string out_path = "diagram.svg";
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if ((arg == "--diagram" || arg == "-d") && i + 1 < argc) {
                diag_id_str = argv[++i];
            } else if ((arg == "--out" || arg == "-o") && i + 1 < argc) {
                out_path = argv[++i];
            }
        }

        if (diag_id_str.empty()) {
            std::cerr << "Error: --diagram <id> is required.\n";
            return 1;
        }

        int diag_id = 0;
        try { diag_id = std::stoi(diag_id_str); } catch (...) {}
        auto diag = repo.GetDiagramByID(diag_id);
        if (!diag) {
            std::cerr << "Error: Diagram ID " << diag_id << " not found.\n";
            return 1;
        }

        std::cout << "Rendering Diagram [" << diag->GetDiagramID() << "] \"" << diag->GetName()
                  << "\" (" << diag->GetDiagramObjects().size() << " objects, "
                  << diag->GetDiagramLinks().size() << " links)...\n";

        ea::DiagramRenderer renderer;
        bool ok = false;
        if (out_path.size() >= 4 && out_path.substr(out_path.size() - 4) == ".png") {
            ok = renderer.RenderToPNGFile(*diag, repo, out_path);
        } else {
            ok = renderer.RenderToSVGFile(*diag, repo, out_path);
        }

        if (ok) {
            std::cout << "Saved diagram render to: " << out_path << "\n";
            return 0;
        } else {
            std::cerr << "Failed to render diagram.\n";
            return 1;
        }
    }

    if (cmd == "export") {
        std::string diag_id_str;
        std::string format = "plantuml";
        std::string out_path;
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if ((arg == "--diagram" || arg == "-d") && i + 1 < argc) {
                diag_id_str = argv[++i];
            } else if ((arg == "--format" || arg == "-f") && i + 1 < argc) {
                format = argv[++i];
            } else if ((arg == "--out" || arg == "-o") && i + 1 < argc) {
                out_path = argv[++i];
            }
        }

        if (format == "json" && diag_id_str.empty()) {
            // Full repository JSON export
            std::string json = ea::JSONExporter::ExportRepository(repo);
            if (!out_path.empty()) {
                ea::JSONExporter::ExportRepositoryToFile(repo, out_path);
                std::cout << "Exported repository JSON to: " << out_path << "\n";
            } else {
                std::cout << json << "\n";
            }
            return 0;
        }

        if (diag_id_str.empty()) {
            std::cerr << "Error: --diagram <id> required.\n";
            return 1;
        }

        int diag_id = 0;
        try { diag_id = std::stoi(diag_id_str); } catch (...) {}
        auto diag = repo.GetDiagramByID(diag_id);
        if (!diag) {
            std::cerr << "Error: Diagram ID " << diag_id << " not found.\n";
            return 1;
        }

        if (format == "plantuml" || format == "puml") {
            std::string uml = ea::PlantUMLExporter::ExportDiagram(*diag, repo);
            if (!out_path.empty()) {
                ea::PlantUMLExporter::ExportDiagramToFile(*diag, repo, out_path);
                std::cout << "Exported diagram PlantUML to: " << out_path << "\n";
            } else {
                std::cout << uml << "\n";
            }
        } else if (format == "xmi") {
            auto pkg = repo.GetPackageByID(diag->GetPackageID());
            if (pkg) {
                std::string xmi = ea::XMIExporter::ExportPackageXMI(*pkg, repo);
                if (!out_path.empty()) {
                    ea::XMIExporter::ExportPackageXMIToFile(*pkg, repo, out_path);
                    std::cout << "Exported package XMI to: " << out_path << "\n";
                } else {
                    std::cout << xmi << "\n";
                }
            }
        }
        return 0;
    }

    if (cmd == "layout") {
        std::string diag_id_str;
        std::string algo_str = "force";
        bool save = false;
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if ((arg == "--diagram" || arg == "-d") && i + 1 < argc) {
                diag_id_str = argv[++i];
            } else if ((arg == "--algo" || arg == "-a") && i + 1 < argc) {
                algo_str = argv[++i];
            } else if (arg == "--save" || arg == "-s") {
                save = true;
            }
        }

        int diag_id = 0;
        try { diag_id = std::stoi(diag_id_str); } catch (...) {}
        auto diag = repo.GetDiagramByID(diag_id);
        if (!diag) {
            std::cerr << "Error: Diagram ID " << diag_id << " not found.\n";
            return 1;
        }

        std::cout << "Applying " << algo_str << " layout to diagram [" << diag_id << "]...\n";
        ea::LayoutAlgorithm algo = ea::LayoutAlgorithm::ForceDirected;
        if (algo_str == "grid") algo = ea::LayoutAlgorithm::Grid;
        else if (algo_str == "hierarchical") algo = ea::LayoutAlgorithm::Hierarchical;

        ea::DiagramLayout::Layout(*diag, repo, algo);

        if (save) {
            repo.GetStorageEngine()->SaveDiagram(*diag);
            std::cout << "Layout saved to database.\n";
        } else {
            std::cout << "Layout computed (dry-run, pass --save to commit to database).\n";
        }
        return 0;
    }

    std::cerr << "Unknown command: " << cmd << "\n";
    PrintUsage();
    return 1;
}
