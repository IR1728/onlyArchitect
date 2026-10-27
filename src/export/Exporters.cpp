#include "ea/export/Exporters.hpp"
#include "ea/core/Utils.hpp"
#include <sstream>
#include <fstream>
#include <unordered_set>

namespace ea {

// ============================================================================
// PlantUML Exporter
// ============================================================================

std::string PlantUMLExporter::ExportDiagram(const Diagram& diagram, Repository& repo) {
    std::ostringstream uml;
    uml << "@startuml\n";
    uml << "title " << diagram.GetName() << "\n\n";
    uml << "skinparam classAttributeIconSize 0\n";
    uml << "skinparam monochrome false\n\n";

    std::unordered_set<int> exported_element_ids;

    // Export Elements in this diagram
    for (size_t i = 0; i < diagram.GetDiagramObjects().size(); ++i) {
        auto dobj = diagram.GetDiagramObjects().GetAt(static_cast<short>(i));
        if (!dobj) continue;
        auto elem = repo.GetElementByID(dobj->GetElementID());
        if (!elem) continue;

        exported_element_ids.insert(elem->GetElementID());

        std::string type_kw = "class";
        if (elem->GetType() == ObjectType::Interface) type_kw = "interface";
        else if (elem->GetType() == ObjectType::Enumeration) type_kw = "enum";
        else if (elem->GetType() == ObjectType::Package) type_kw = "package";
        else if (elem->GetType() == ObjectType::Component) type_kw = "component";
        else if (elem->GetType() == ObjectType::UseCase) type_kw = "usecase";
        else if (elem->GetType() == ObjectType::Actor) type_kw = "actor";
        else if (elem->GetType() == ObjectType::Note) type_kw = "note";

        if (elem->GetType() == ObjectType::Note) {
            uml << "note \"" << utils::replace_all(elem->GetNotes(), "\n", "\\n") << "\" as Note_" << elem->GetElementID() << "\n\n";
            continue;
        }

        uml << type_kw << " \"" << elem->GetName() << "\" as E" << elem->GetElementID();
        if (!elem->GetStereotype().empty()) {
            uml << " <<" << elem->GetStereotype() << ">>";
        }
        uml << " {\n";

        // Attributes
        for (size_t a = 0; a < elem->GetAttributes().size(); ++a) {
            auto attr = elem->GetAttributes().GetAt(static_cast<short>(a));
            if (!attr) continue;
            char vis = '+';
            if (attr->GetVisibility() == Scope::Private) vis = '-';
            else if (attr->GetVisibility() == Scope::Protected) vis = '#';
            else if (attr->GetVisibility() == Scope::Package) vis = '~';
            uml << "  " << vis << " " << attr->GetName() << " : " << attr->GetType() << "\n";
        }

        // Methods
        for (size_t m = 0; m < elem->GetMethods().size(); ++m) {
            auto meth = elem->GetMethods().GetAt(static_cast<short>(m));
            if (!meth) continue;
            char vis = '+';
            if (meth->GetVisibility() == Scope::Private) vis = '-';
            else if (meth->GetVisibility() == Scope::Protected) vis = '#';
            else if (meth->GetVisibility() == Scope::Package) vis = '~';
            uml << "  " << vis << " " << meth->GetName() << "() : " << meth->GetReturnType() << "\n";
        }

        uml << "}\n\n";
    }

    // Export Connectors
    for (size_t i = 0; i < diagram.GetDiagramLinks().size(); ++i) {
        auto dlink = diagram.GetDiagramLinks().GetAt(static_cast<short>(i));
        if (!dlink) continue;
        auto conn = repo.GetConnectorByID(dlink->GetConnectorID());
        if (!conn) continue;

        if (exported_element_ids.find(conn->GetClientID()) == exported_element_ids.end() ||
            exported_element_ids.find(conn->GetSupplierID()) == exported_element_ids.end()) {
            continue;
        }

        std::string arrow = "-->";
        switch (conn->GetType()) {
            case ConnectorType::Generalization: arrow = "--|>"; break;
            case ConnectorType::Realization: arrow = "..|>"; break;
            case ConnectorType::Dependency: arrow = "..>"; break;
            case ConnectorType::Composition: arrow = "*--"; break;
            case ConnectorType::Aggregation: arrow = "o--"; break;
            case ConnectorType::NoteLink: arrow = ".."; break;
            default: arrow = "-->"; break;
        }

        uml << "E" << conn->GetClientID() << " " << arrow << " E" << conn->GetSupplierID();
        if (!conn->GetName().empty()) {
            uml << " : " << conn->GetName();
        }
        uml << "\n";
    }

    uml << "@enduml\n";
    return uml.str();
}

std::string PlantUMLExporter::ExportPackage(const Package& package, [[maybe_unused]] Repository& repo) {
    std::ostringstream uml;
    uml << "@startuml\n";
    uml << "title Package: " << package.GetName() << "\n\n";

    for (size_t i = 0; i < package.GetElements().size(); ++i) {
        auto elem = package.GetElements().GetAt(static_cast<short>(i));
        if (!elem) continue;
        uml << "class \"" << elem->GetName() << "\" as E" << elem->GetElementID() << " {\n";
        for (size_t a = 0; a < elem->GetAttributes().size(); ++a) {
            auto attr = elem->GetAttributes().GetAt(static_cast<short>(a));
            if (attr) uml << "  + " << attr->GetName() << " : " << attr->GetType() << "\n";
        }
        for (size_t m = 0; m < elem->GetMethods().size(); ++m) {
            auto meth = elem->GetMethods().GetAt(static_cast<short>(m));
            if (meth) uml << "  + " << meth->GetName() << "() : " << meth->GetReturnType() << "\n";
        }
        uml << "}\n";
    }

    uml << "@enduml\n";
    return uml.str();
}

bool PlantUMLExporter::ExportDiagramToFile(const Diagram& diagram, Repository& repo, const std::string& path) {
    std::ofstream out(path);
    if (!out.is_open()) return false;
    out << ExportDiagram(diagram, repo);
    return true;
}

// ============================================================================
// JSON Exporter
// ============================================================================

static void SerializePackageJSON(std::ostringstream& ss, const Package& pkg, Repository& repo, int indent = 2);

static void SerializeElementJSON(std::ostringstream& ss, const Element& elem, int indent = 4) {
    std::string ind(indent, ' ');
    ss << ind << "{\n";
    ss << ind << "  \"id\": " << elem.GetElementID() << ",\n";
    ss << ind << "  \"guid\": \"" << elem.GetElementGUID() << "\",\n";
    ss << ind << "  \"name\": \"" << utils::escape_json(elem.GetName()) << "\",\n";
    ss << ind << "  \"type\": \"" << to_string(elem.GetType()) << "\",\n";
    ss << ind << "  \"stereotype\": \"" << utils::escape_json(elem.GetStereotype()) << "\",\n";
    ss << ind << "  \"status\": \"" << utils::escape_json(elem.GetStatus()) << "\",\n";
    ss << ind << "  \"notes\": \"" << utils::escape_json(elem.GetNotes()) << "\",\n";

    // Attributes
    ss << ind << "  \"attributes\": [\n";
    for (size_t i = 0; i < elem.GetAttributes().size(); ++i) {
        auto a = elem.GetAttributes().GetAt(static_cast<short>(i));
        if (!a) continue;
        ss << ind << "    {\"id\": " << a->GetAttributeID()
           << ", \"name\": \"" << utils::escape_json(a->GetName()) << "\""
           << ", \"type\": \"" << utils::escape_json(a->GetType()) << "\""
           << ", \"scope\": \"" << to_string(a->GetVisibility()) << "\"}"
           << (i + 1 < elem.GetAttributes().size() ? "," : "") << "\n";
    }
    ss << ind << "  ],\n";

    // Methods
    ss << ind << "  \"methods\": [\n";
    for (size_t i = 0; i < elem.GetMethods().size(); ++i) {
        auto m = elem.GetMethods().GetAt(static_cast<short>(i));
        if (!m) continue;
        ss << ind << "    {\"id\": " << m->GetMethodID()
           << ", \"name\": \"" << utils::escape_json(m->GetName()) << "\""
           << ", \"return_type\": \"" << utils::escape_json(m->GetReturnType()) << "\""
           << ", \"scope\": \"" << to_string(m->GetVisibility()) << "\"}"
           << (i + 1 < elem.GetMethods().size() ? "," : "") << "\n";
    }
    ss << ind << "  ]\n";
    ss << ind << "}";
}

static void SerializePackageJSON(std::ostringstream& ss, const Package& pkg, Repository& repo, int indent) {
    std::string ind(indent, ' ');
    ss << ind << "{\n";
    ss << ind << "  \"id\": " << pkg.GetPackageID() << ",\n";
    ss << ind << "  \"guid\": \"" << pkg.GetPackageGUID() << "\",\n";
    ss << ind << "  \"name\": \"" << utils::escape_json(pkg.GetName()) << "\",\n";

    // Elements
    ss << ind << "  \"elements\": [\n";
    for (size_t i = 0; i < pkg.GetElements().size(); ++i) {
        auto elem = pkg.GetElements().GetAt(static_cast<short>(i));
        if (!elem) continue;
        SerializeElementJSON(ss, *elem, indent + 4);
        if (i + 1 < pkg.GetElements().size()) ss << ",";
        ss << "\n";
    }
    ss << ind << "  ],\n";

    // Sub-packages
    ss << ind << "  \"packages\": [\n";
    for (size_t i = 0; i < pkg.GetPackages().size(); ++i) {
        auto sub = pkg.GetPackages().GetAt(static_cast<short>(i));
        if (!sub) continue;
        SerializePackageJSON(ss, *sub, repo, indent + 4);
        if (i + 1 < pkg.GetPackages().size()) ss << ",";
        ss << "\n";
    }
    ss << ind << "  ]\n";
    ss << ind << "}";
}

std::string JSONExporter::ExportRepository(Repository& repo) {
    std::ostringstream ss;
    ss << "{\n  \"repository\": {\n    \"version\": \"" << repo.GetLibraryVersion() << "\",\n";
    ss << "    \"models\": [\n";
    for (size_t i = 0; i < repo.GetModels().size(); ++i) {
        auto m = repo.GetModels().GetAt(static_cast<short>(i));
        if (!m) continue;
        SerializePackageJSON(ss, *m, repo, 6);
        if (i + 1 < repo.GetModels().size()) ss << ",";
        ss << "\n";
    }
    ss << "    ]\n  }\n}\n";
    return ss.str();
}

bool JSONExporter::ExportRepositoryToFile(Repository& repo, const std::string& path) {
    std::ofstream out(path);
    if (!out.is_open()) return false;
    out << ExportRepository(repo);
    return true;
}

// ============================================================================
// XMI Exporter (UML 2.5 / XMI 2.1)
// ============================================================================

std::string XMIExporter::ExportPackageXMI(const Package& package, [[maybe_unused]] Repository& repo) {
    std::ostringstream xmi;
    xmi << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xmi << "<xmi:XMI xmi:version=\"2.1\" xmlns:uml=\"http://schema.omg.org/spec/UML/2.1\" xmlns:xmi=\"http://schema.omg.org/spec/XMI/2.1\">\n";
    xmi << "  <uml:Model xmi:type=\"uml:Model\" xmi:id=\"EA_Model_" << package.GetPackageID()
        << "\" name=\"" << utils::escape_xml(package.GetName()) << "\">\n";

    for (size_t i = 0; i < package.GetElements().size(); ++i) {
        auto elem = package.GetElements().GetAt(static_cast<short>(i));
        if (!elem) continue;
        xmi << "    <packagedElement xmi:type=\"uml:Class\" xmi:id=\"EA_Elem_" << elem->GetElementID()
            << "\" name=\"" << utils::escape_xml(elem->GetName()) << "\">\n";

        for (size_t a = 0; a < elem->GetAttributes().size(); ++a) {
            auto attr = elem->GetAttributes().GetAt(static_cast<short>(a));
            if (!attr) continue;
            xmi << "      <ownedAttribute xmi:type=\"uml:Property\" xmi:id=\"EA_Attr_" << attr->GetAttributeID()
                << "\" name=\"" << utils::escape_xml(attr->GetName()) << "\" visibility=\""
                << to_string(attr->GetVisibility()) << "\" />\n";
        }

        for (size_t m = 0; m < elem->GetMethods().size(); ++m) {
            auto meth = elem->GetMethods().GetAt(static_cast<short>(m));
            if (!meth) continue;
            xmi << "      <ownedOperation xmi:type=\"uml:Operation\" xmi:id=\"EA_Meth_" << meth->GetMethodID()
                << "\" name=\"" << utils::escape_xml(meth->GetName()) << "\" visibility=\""
                << to_string(meth->GetVisibility()) << "\" />\n";
        }

        xmi << "    </packagedElement>\n";
    }

    xmi << "  </uml:Model>\n";
    xmi << "</xmi:XMI>\n";
    return xmi.str();
}

bool XMIExporter::ExportPackageXMIToFile(const Package& package, Repository& repo, const std::string& path) {
    std::ofstream out(path);
    if (!out.is_open()) return false;
    out << ExportPackageXMI(package, repo);
    return true;
}

} // namespace ea
