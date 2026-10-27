#pragma once

#include <string>
#include <memory>
#include "ea/model/Package.hpp"
#include "ea/model/Diagram.hpp"
#include "ea/repository/Repository.hpp"

namespace ea {

class PlantUMLExporter {
public:
    static std::string ExportDiagram(const Diagram& diagram, Repository& repo);
    static std::string ExportPackage(const Package& package, Repository& repo);
    static bool ExportDiagramToFile(const Diagram& diagram, Repository& repo, const std::string& path);
};

class JSONExporter {
public:
    static std::string ExportDiagram(const Diagram& diagram, Repository& repo);
    static std::string ExportPackage(const Package& package, Repository& repo, bool recursive = true);
    static std::string ExportRepository(Repository& repo);
    static bool ExportRepositoryToFile(Repository& repo, const std::string& path);
};

class XMIExporter {
public:
    // Exports package or model to standard OMG UML 2.5 / XMI 2.1
    static std::string ExportPackageXMI(const Package& package, Repository& repo);
    static bool ExportPackageXMIToFile(const Package& package, Repository& repo, const std::string& path);
};

} // namespace ea
