#pragma once

// Model interchange in OMG XMI (UML 2.5.1 metamodel, XMI 2.5.1 serialization).
//
// The standard part carries what UML defines: packages, classifiers by metaclass, properties with
// multiplicity, operations with parameters, comments, generalizations, interface realizations,
// dependencies, associations (ends, roles, aggregation), include/extend. What UML does not cover or
// that needs a behavioural owner we do not model (EA object kinds, stereotypes, messages,
// transitions, flows, diagrams) travels in an <xmi:Extension extender="onlyArchitect"> laid out
// like Enterprise Architect's (elements / connectors / diagrams), so a round trip is lossless.
//
// Import reads both parts: the extension when present, the standard UML otherwise (e.g. files
// written by other tools). Stereotype applications at the document root (<profile:st base_Class=..>)
// are also understood.

#include <string>
#include <string_view>
#include <vector>
#include "ea/repository/Repository.hpp"

namespace ea::xmi {

inline constexpr const char* Extender = "onlyArchitect";

struct ExportResult {
    std::string xml;
    size_t packages{0}, elements{0}, relationships{0}, diagrams{0};
    std::vector<std::string> warnings; // e.g. relationships to elements outside the exported package
};

// Exports `package` and everything below it
ExportResult export_package(Repository& repo, const Package& package);
bool export_package_to_file(Repository& repo, const Package& package, const std::string& path, ExportResult* result = nullptr);

struct ImportResult {
    bool ok{false};
    std::string error;            // why nothing was imported (parse error, write failure, ...)
    bool repositoryReloadNeeded{false}; // a write failed mid-way and was rolled back: reopen the file
    size_t packages{0}, elements{0}, relationships{0}, diagrams{0};
    std::vector<std::string> warnings;  // skipped parts (unresolved references, unknown kinds, ...)
    std::vector<int> topPackageIds;     // packages created directly under the target
};

// Imports a document under package `targetPackageId`. All or nothing: the document is parsed and
// resolved before anything is written, and the writes run in one transaction.
ImportResult import_into(Repository& repo, int targetPackageId, std::string_view document);
ImportResult import_file(Repository& repo, int targetPackageId, const std::string& path);

// --- Mapping between the model and UML metaclasses (shared by export and import) --------------
std::string uml_metaclass(ObjectType type);         // "uml:Class", ...
ObjectType object_type_for(std::string_view umlType); // inverse; Unknown when not a classifier we know

} // namespace ea::xmi
