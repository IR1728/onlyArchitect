#include "ea/export/ModelDocExporter.hpp"
#include "ea/editor/FeatureText.hpp"
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace ea {

namespace {

void writePackageMarkdown(const Package& pkg, int depth, std::ostringstream& out) {
    const std::string heading(static_cast<size_t>(std::min(depth + 2, 6)), '#');
    out << heading << " Paquete: " << pkg.GetName() << "\n\n";
    if (!pkg.GetNotes().empty()) {
        out << pkg.GetNotes() << "\n\n";
    }

    if (!pkg.GetDiagrams().empty()) {
        out << "**Diagramas:**\n\n";
        for (size_t i = 0; i < pkg.GetDiagrams().size(); ++i) {
            if (auto d = pkg.GetDiagrams().GetAt(static_cast<short>(i))) {
                out << "- **" << d->GetName() << "** (`" << to_string(d->GetType()) << "`, "
                    << d->GetDiagramObjects().size() << " elementos, "
                    << d->GetDiagramLinks().size() << " enlaces)\n";
            }
        }
        out << "\n";
    }

    std::unordered_set<std::string> childPkgGuids;
    for (const auto& sub : pkg.GetPackages()) {
        if (sub) childPkgGuids.insert(sub->GetPackageGUID());
    }

    for (size_t i = 0; i < pkg.GetElements().size(); ++i) {
        auto elem = pkg.GetElements().GetAt(static_cast<short>(i));
        if (!elem) continue;
        if (elem->GetType() == ObjectType::Package && childPkgGuids.count(elem->GetElementGUID())) continue;

        out << "- **" << (elem->GetName().empty() ? "(sin nombre)" : elem->GetName()) << "**"
            << " — `" << to_string(elem->GetType()) << "`";
        if (!elem->GetStereotype().empty()) {
            out << " `<<" << elem->GetStereotype() << ">>`";
        }
        out << "\n";

        if (!elem->GetNotes().empty()) {
            out << "  > " << elem->GetNotes() << "\n";
        }

        if (!elem->GetAttributes().empty()) {
            out << "  - *Atributos:* ";
            for (size_t a = 0; a < elem->GetAttributes().size(); ++a) {
                if (auto attr = elem->GetAttributes().GetAt(static_cast<short>(a))) {
                    if (a > 0) out << ", ";
                    out << "`" << editor::visibility_symbol(attr->GetVisibility()) << attr->GetName();
                    if (!attr->GetType().empty()) out << ": " << attr->GetType();
                    out << "`";
                }
            }
            out << "\n";
        }

        if (!elem->GetMethods().empty()) {
            out << "  - *Operaciones:* ";
            for (size_t m = 0; m < elem->GetMethods().size(); ++m) {
                if (auto meth = elem->GetMethods().GetAt(static_cast<short>(m))) {
                    if (m > 0) out << ", ";
                    out << "`" << editor::visibility_symbol(meth->GetVisibility()) << meth->GetName()
                        << "(" << editor::format_parameters(meth->GetParameters()) << ")";
                    if (!meth->GetReturnType().empty()) out << ": " << meth->GetReturnType();
                    out << "`";
                }
            }
            out << "\n";
        }
    }

    if (!pkg.GetElements().empty()) {
        out << "\n";
    }

    for (size_t i = 0; i < pkg.GetPackages().size(); ++i) {
        if (auto sub = pkg.GetPackages().GetAt(static_cast<short>(i))) {
            writePackageMarkdown(*sub, depth + 1, out);
        }
    }
}

} // namespace

std::string ModelDocExporter::exportMarkdown(Repository& repo) {
    std::ostringstream out;
    out << "# Documentación de Arquitectura — onlyArchitect\n\n";
    if (!repo.IsOpen()) {
        out << "*No hay un repositorio abierto.*\n";
        return out.str();
    }

    const auto counts = repo.GetCounts();
    out << "| Métrica | Cantidad |\n";
    out << "|---|---:|\n";
    out << "| Paquetes | " << counts.packages << " |\n";
    out << "| Elementos | " << counts.elements << " |\n";
    out << "| Diagramas | " << counts.diagrams << " |\n";
    out << "| Relaciones | " << counts.connectors << " |\n";
    out << "| Atributos | " << counts.attributes << " |\n";
    out << "| Operaciones | " << counts.operations << " |\n\n";
    out << "---\n\n";

    for (size_t i = 0; i < repo.GetModels().size(); ++i) {
        if (auto m = repo.GetModels().GetAt(static_cast<short>(i))) {
            writePackageMarkdown(*m, 0, out);
        }
    }
    return out.str();
}

bool ModelDocExporter::exportMarkdownToFile(Repository& repo, const std::string& filePath) {
    std::ofstream f(filePath, std::ios::binary);
    if (!f.is_open()) return false;
    f << exportMarkdown(repo);
    return f.good();
}

} // namespace ea
