#include "ea/xmi/Xmi.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include "ea/editor/FeatureText.hpp"
#include "ea/xml/Xml.hpp"

namespace ea::xmi {

namespace {

constexpr const char* kXmiNs = "http://www.omg.org/spec/XMI/20131001";
constexpr const char* kUmlNs = "http://www.omg.org/spec/UML/20161101";
constexpr const char* kProfileNs = "http://onlyarchitect.dev/profile/1.0";
constexpr const char* kProfilePrefix = "oaprofile";
constexpr const char* kPrimitiveTypes = "http://www.omg.org/spec/UML/20161101/PrimitiveTypes.xmi#";

// "{1A2B-...}" → "EAID_1A2B_..." (an XML NCName, stable across exports of the same model)
std::string id_from_guid(const std::string& prefix, const std::string& guid) {
    std::string out = prefix;
    for (char c : guid) {
        if (c == '{' || c == '}') continue;
        out += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
    }
    return out;
}

std::string visibility(Scope s) {
    switch (s) {
        case Scope::Private: return "private";
        case Scope::Protected: return "protected";
        case Scope::Package: return "package";
        default: return "public";
    }
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Standard UML primitive for common type names (int → Integer, ...); empty when not a primitive
std::string uml_primitive(const std::string& typeName) {
    static const std::unordered_map<std::string, std::string> primitives = {
        {"int", "Integer"},     {"integer", "Integer"}, {"long", "Integer"},   {"short", "Integer"},
        {"bool", "Boolean"},    {"boolean", "Boolean"}, {"string", "String"},  {"std::string", "String"},
        {"double", "Real"},     {"float", "Real"},      {"real", "Real"},      {"unlimitednatural", "UnlimitedNatural"},
    };
    auto it = primitives.find(lower(typeName));
    return it != primitives.end() ? it->second : "";
}

bool is_xml_name(const std::string& s) {
    if (s.empty() || !(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_')) return false;
    return std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isalnum(c) || c == '_' || c == '-' || c == '.'; });
}

bool is_composite(const Connector& c) {
    return c.GetType() == ConnectorType::Composition || c.GetSubtype() == "Strong";
}

class Exporter {
public:
    explicit Exporter(Repository& repo) : m_repo(repo) {}

    ExportResult run(const Package& root) {
        collectPackages(root);
        for (const auto* pkg : m_packages) collectElements(*pkg);
        collectConnectors();
        indexClassifierNames();

        m_w.open("xmi:XMI").attr("xmlns:xmi", kXmiNs).attr("xmlns:uml", kUmlNs).attr(std::string("xmlns:") + kProfilePrefix, kProfileNs);
        m_w.open("xmi:Documentation").attr("exporter", "onlyArchitect").attr("exporterVersion", "1.0").close();
        m_w.open("uml:Model").attr("xmi:type", "uml:Model").attr("xmi:id", "onlyArchitect_Model").attr("name", "onlyArchitect Model");
        writePackage(root);
        m_w.close();
        writeExtension();
        writeStereotypeApplications();
        m_w.close();

        m_result.xml = m_w.str();
        // package rows without a loaded package (EA keeps some) are written as empty uml:Package
        const auto orphanPackages = static_cast<size_t>(std::count_if(m_elements.begin(), m_elements.end(),
            [](const auto& e) { return e->GetType() == ObjectType::Package; }));
        m_result.packages = m_packages.size() + orphanPackages;
        m_result.elements = m_elements.size() - orphanPackages;
        m_result.relationships = m_connectors.size();
        return std::move(m_result);
    }

private:
    // ---------------------------------------------------------------- collection
    void collectPackages(const Package& pkg) {
        m_packages.push_back(&pkg);
        m_packageXmi[pkg.GetPackageID()] = id_from_guid("EAPK_", pkg.GetPackageGUID());
        m_packageByGuid[pkg.GetPackageGUID()] = &pkg;
        for (const auto& child : pkg.GetPackages()) if (child) collectPackages(*child);
    }

    void collectElementRecursive(const std::shared_ptr<Element>& elem, int packageId) {
        if (!elem || m_elementById.count(elem->GetElementID())) return;
        if (elem->GetType() == ObjectType::Package) {
            // the t_object row of a package: relationships to it point at the package itself
            auto it = m_packageByGuid.find(elem->GetElementGUID());
            if (it != m_packageByGuid.end()) {
                m_elementXmi[elem->GetElementID()] = m_packageXmi[it->second->GetPackageID()];
                m_elementById[elem->GetElementID()] = elem.get();
                for (const auto& child : elem->GetElements()) collectElementRecursive(child, packageId);
                return;
            }
        }
        m_elements.push_back(elem);
        m_elementById[elem->GetElementID()] = elem.get();
        m_elementXmi[elem->GetElementID()] = id_from_guid("EAID_", elem->GetElementGUID());
        m_elementsOf[packageId].push_back(elem.get());
        for (const auto& child : elem->GetElements()) collectElementRecursive(child, packageId);
    }

    void collectElements(const Package& pkg) {
        for (const auto& elem : pkg.GetElements()) {
            collectElementRecursive(elem, pkg.GetPackageID());
        }
        // Also collect nested elements in this package (ParentID != 0, e.g., Robustness objects
        // owned by UseCases or Actions/Partitions owned by Activities) that EA stores in t_object.
        const std::string xml = m_repo.SQLQuery(
            "SELECT Object_ID FROM t_object WHERE Package_ID = " + std::to_string(pkg.GetPackageID()) +
            " AND ParentID > 0 ORDER BY Object_ID ASC;");
        if (!xml.empty()) {
            auto parsed = xml::parse(xml);
            if (parsed.ok() && parsed.root) {
                std::vector<const xml::Node*> stack{parsed.root.get()};
                while (!stack.empty()) {
                    const xml::Node* cur = stack.back();
                    stack.pop_back();
                    if (cur->localName() == "Object_ID" && !cur->text.empty()) {
                        try {
                            const int oid = std::stoi(cur->text);
                            if (oid > 0 && !m_elementById.count(oid)) {
                                if (auto nested = m_repo.GetElementByID(oid)) {
                                    collectElementRecursive(nested, pkg.GetPackageID());
                                }
                            }
                        } catch (...) {}
                    }
                    for (const auto& ch : cur->children) stack.push_back(ch.get());
                }
            }
        }
    }

    void collectConnectors() {
        std::unordered_set<int> seen;
        size_t outside = 0;
        for (const auto& elem : m_elements) {
            for (const auto& conn : elem->GetConnectors()) {
                if (!conn || !seen.insert(conn->GetConnectorID()).second) continue;
                if (!m_elementXmi.count(conn->GetClientID()) || !m_elementXmi.count(conn->GetSupplierID())) {
                    ++outside;
                    continue;
                }
                m_connectors.push_back(conn);
                m_connectorXmi[conn->GetConnectorID()] = id_from_guid("EAID_", conn->GetConnectorGUID());
                classify(*conn);
            }
        }
        if (outside > 0) {
            m_result.warnings.push_back(std::to_string(outside) +
                                        " relaciones con elementos fuera del paquete exportado no se incluyeron.");
        }
    }

    void classify(const Connector& c) {
        const auto* client = m_elementById[c.GetClientID()];
        const auto* supplier = m_elementById[c.GetSupplierID()];
        switch (c.GetType()) {
            case ConnectorType::Generalization:
            case ConnectorType::UseCaseLink:
                m_ownedBy[c.GetClientID()].push_back(&c);
                m_inUml.insert(c.GetConnectorID());
                return;
            case ConnectorType::Realization:
                if (supplier && supplier->GetType() == ObjectType::Interface) m_ownedBy[c.GetClientID()].push_back(&c);
                else m_packagedIn[client ? client->GetPackageID() : 0].push_back(&c);
                m_inUml.insert(c.GetConnectorID());
                return;
            case ConnectorType::Association:
            case ConnectorType::Aggregation:
            case ConnectorType::Composition:
            case ConnectorType::Dependency:
            case ConnectorType::Usage:
            case ConnectorType::Abstraction:
            case ConnectorType::InformationFlow:
            case ConnectorType::PackageImport:
            case ConnectorType::PackageMerge:
                m_packagedIn[client ? client->GetPackageID() : 0].push_back(&c);
                m_inUml.insert(c.GetConnectorID());
                return;
            case ConnectorType::NoteLink: {
                const bool clientIsNote = client && (client->GetType() == ObjectType::Note || client->GetType() == ObjectType::Text);
                m_annotations[clientIsNote ? c.GetClientID() : c.GetSupplierID()].push_back(
                    clientIsNote ? c.GetSupplierID() : c.GetClientID());
                return;
            }
            default:
                return; // behaviour-owned (messages, transitions, flows, ...): extension only
        }
    }

    void indexClassifierNames() {
        std::unordered_map<std::string, int> count;
        for (const auto& e : m_elements) ++count[e->GetName()];
        for (const auto& e : m_elements) {
            if (count[e->GetName()] == 1 && !e->GetName().empty()) m_classifierByName[e->GetName()] = m_elementXmi[e->GetElementID()];
        }
    }

    // ---------------------------------------------------------------- UML part
    void writeComment(const std::string& body, const std::string& annotated, const std::string& id) {
        m_w.open("ownedComment").attr("xmi:type", "uml:Comment").attr("xmi:id", id).attr("body", body);
        m_w.attrIfNotEmpty("annotatedElement", annotated);
        m_w.close();
    }

    void writePackage(const Package& pkg) {
        const auto& id = m_packageXmi[pkg.GetPackageID()];
        m_w.open("packagedElement").attr("xmi:type", "uml:Package").attr("xmi:id", id).attr("name", pkg.GetName());
        if (!pkg.GetNotes().empty()) writeComment(pkg.GetNotes(), id, id + "_note");

        for (const auto* e : m_elementsOf[pkg.GetPackageID()]) {
            if (e->GetType() == ObjectType::Note || e->GetType() == ObjectType::Text) writeNote(*e);
        }
        for (const auto* e : m_elementsOf[pkg.GetPackageID()]) {
            if (e->GetType() != ObjectType::Note && e->GetType() != ObjectType::Text) writeElement(*e);
        }
        for (const auto* c : m_packagedIn[pkg.GetPackageID()]) writePackagedRelationship(*c);
        for (const auto& child : pkg.GetPackages()) if (child) writePackage(*child);
        m_w.close();
    }

    void writeNote(const Element& note) {
        std::string annotated;
        for (int target : m_annotations[note.GetElementID()]) {
            if (!annotated.empty()) annotated += ' ';
            annotated += m_elementXmi[target];
        }
        writeComment(note.GetNotes().empty() ? note.GetName() : note.GetNotes(), annotated, m_elementXmi[note.GetElementID()]);
    }

    void writeTypeOf(const std::string& typeName) {
        const std::string primitive = uml_primitive(typeName);
        if (primitive.empty()) return;
        m_w.open("type").attr("xmi:type", "uml:PrimitiveType").attr("href", std::string(kPrimitiveTypes) + primitive).close();
    }

    // `type` attribute (must be written while the start tag is open): a classifier of the export
    void writeTypeRef(const std::string& typeName) {
        auto it = m_classifierByName.find(typeName);
        if (it != m_classifierByName.end()) m_w.attr("type", it->second);
    }

    void writeMultiplicity(const std::string& id, const std::string& lowerBound, const std::string& upperBound) {
        if (lowerBound.empty() && upperBound.empty()) return;
        m_w.open("lowerValue").attr("xmi:type", "uml:LiteralInteger").attr("xmi:id", id + "_lower")
            .attr("value", lowerBound.empty() ? "1" : lowerBound).close();
        m_w.open("upperValue").attr("xmi:type", "uml:LiteralUnlimitedNatural").attr("xmi:id", id + "_upper")
            .attr("value", upperBound.empty() ? "1" : upperBound).close();
    }

    void writeElement(const Element& e) {
        const auto& id = m_elementXmi[e.GetElementID()];
        const std::string metaclass = uml_metaclass(e.GetType());
        m_w.open("packagedElement").attr("xmi:type", metaclass).attr("xmi:id", id).attr("name", e.GetName())
            .attr("visibility", "public");
        if (e.IsAbstract()) m_w.attr("isAbstract", "true");
        if (!e.GetNotes().empty()) writeComment(e.GetNotes(), id, id + "_note");

        for (const auto& a : e.GetAttributes()) {
            if (!a) continue;
            const auto aid = id_from_guid("EAID_", a->GetAttributeGUID());
            if (e.GetType() == ObjectType::Enumeration) {
                m_w.open("ownedLiteral").attr("xmi:type", "uml:EnumerationLiteral").attr("xmi:id", aid).attr("name", a->GetName()).close();
                continue;
            }
            m_w.open("ownedAttribute").attr("xmi:type", "uml:Property").attr("xmi:id", aid).attr("name", a->GetName())
                .attr("visibility", visibility(a->GetVisibility()));
            if (a->IsStatic()) m_w.attr("isStatic", "true");
            if (a->IsConst()) m_w.attr("isReadOnly", "true");
            writeTypeRef(a->GetType());
            writeTypeOf(m_classifierByName.count(a->GetType()) ? "" : a->GetType());
            writeMultiplicity(aid, a->GetLowerBound(), a->GetUpperBound());
            if (!a->GetDefault().empty()) {
                m_w.open("defaultValue").attr("xmi:type", "uml:OpaqueExpression").attr("xmi:id", aid + "_default")
                    .attr("body", a->GetDefault()).close();
            }
            m_w.close();
        }
        for (const auto& m : e.GetMethods()) {
            if (!m) continue;
            const auto mid = id_from_guid("EAID_", m->GetMethodGUID());
            m_w.open("ownedOperation").attr("xmi:type", "uml:Operation").attr("xmi:id", mid).attr("name", m->GetName())
                .attr("visibility", visibility(m->GetVisibility()));
            if (m->IsStatic()) m_w.attr("isStatic", "true");
            if (m->IsAbstract()) m_w.attr("isAbstract", "true");
            if (m->IsConst()) m_w.attr("isQuery", "true");
            int n = 0;
            for (const auto& p : m->GetParameters()) {
                m_w.open("ownedParameter").attr("xmi:type", "uml:Parameter").attr("xmi:id", mid + "_p" + std::to_string(n++))
                    .attr("name", p.name).attr("direction", p.kind.empty() ? "in" : p.kind);
                writeTypeRef(p.type);
                writeTypeOf(m_classifierByName.count(p.type) ? "" : p.type);
                if (!p.default_value.empty()) {
                    m_w.open("defaultValue").attr("xmi:type", "uml:OpaqueExpression").attr("xmi:id", mid + "_d" + std::to_string(n))
                        .attr("body", p.default_value).close();
                }
                m_w.close();
            }
            if (!m->GetReturnType().empty() && m->GetReturnType() != "void") {
                m_w.open("ownedParameter").attr("xmi:type", "uml:Parameter").attr("xmi:id", mid + "_return").attr("direction", "return");
                writeTypeRef(m->GetReturnType());
                writeTypeOf(m_classifierByName.count(m->GetReturnType()) ? "" : m->GetReturnType());
                m_w.close();
            }
            m_w.close();
        }
        for (const auto* c : m_ownedBy[e.GetElementID()]) writeOwnedRelationship(*c);
        m_w.close();
    }

    void writeOwnedRelationship(const Connector& c) {
        const auto& id = m_connectorXmi[c.GetConnectorID()];
        const auto& target = m_elementXmi[c.GetSupplierID()];
        switch (c.GetType()) {
            case ConnectorType::Generalization:
                m_w.open("generalization").attr("xmi:type", "uml:Generalization").attr("xmi:id", id).attr("general", target).close();
                return;
            case ConnectorType::Realization:
                m_w.open("interfaceRealization").attr("xmi:type", "uml:InterfaceRealization").attr("xmi:id", id)
                    .attr("client", m_elementXmi[c.GetClientID()]).attr("supplier", target).attr("contract", target).close();
                return;
            case ConnectorType::UseCaseLink:
                if (lower(c.GetStereotype()) == "extend") {
                    m_w.open("extend").attr("xmi:type", "uml:Extend").attr("xmi:id", id).attr("extendedCase", target).close();
                } else {
                    m_w.open("include").attr("xmi:type", "uml:Include").attr("xmi:id", id).attr("addition", target).close();
                }
                return;
            default: return;
        }
    }

    void writeAssociationEnd(const std::string& assocId, const std::string& endId, int typeId, const std::string& role,
                             const std::string& card, const std::string& aggregation) {
        m_w.open("ownedEnd").attr("xmi:type", "uml:Property").attr("xmi:id", endId).attr("name", role)
            .attr("type", m_elementXmi[typeId]).attr("association", assocId);
        if (!aggregation.empty()) m_w.attr("aggregation", aggregation);
        if (!card.empty()) {
            if (auto m = editor::parse_multiplicity(card)) writeMultiplicity(endId, m->lower, m->upper);
        }
        m_w.close();
    }

    void writePackagedRelationship(const Connector& c) {
        const auto& id = m_connectorXmi[c.GetConnectorID()];
        const auto& client = m_elementXmi[c.GetClientID()];
        const auto& supplier = m_elementXmi[c.GetSupplierID()];
        switch (c.GetType()) {
            case ConnectorType::Association:
            case ConnectorType::Aggregation:
            case ConnectorType::Composition: {
                // EA: the target (supplier) is the whole; the end typed by the part carries the aggregation
                const std::string partAggregation = c.GetType() == ConnectorType::Association ? ""
                                                    : is_composite(c)                         ? "composite"
                                                                                              : "shared";
                const auto src = id + "_src", dst = id + "_dst";
                m_w.open("packagedElement").attr("xmi:type", "uml:Association").attr("xmi:id", id).attr("name", c.GetName())
                    .attr("memberEnd", src + " " + dst);
                writeAssociationEnd(id, src, c.GetClientID(), c.GetClientRole(), c.GetClientMultiplicity(), partAggregation);
                writeAssociationEnd(id, dst, c.GetSupplierID(), c.GetSupplierRole(), c.GetSupplierMultiplicity(), "");
                m_w.close();
                return;
            }
            case ConnectorType::InformationFlow:
                m_w.open("packagedElement").attr("xmi:type", "uml:InformationFlow").attr("xmi:id", id).attr("name", c.GetName())
                    .attr("informationSource", client).attr("informationTarget", supplier).close();
                return;
            default: {
                const char* metaclass = c.GetType() == ConnectorType::Usage         ? "uml:Usage"
                                        : c.GetType() == ConnectorType::Abstraction ? "uml:Abstraction"
                                        : c.GetType() == ConnectorType::Realization ? "uml:Realization"
                                                                                    : "uml:Dependency";
                m_w.open("packagedElement").attr("xmi:type", metaclass).attr("xmi:id", id).attr("name", c.GetName())
                    .attr("client", client).attr("supplier", supplier).close();
                return;
            }
        }
    }

    // ---------------------------------------------------------------- extension
    void writeExtension() {
        m_w.open("xmi:Extension").attr("extender", Extender).attr("extenderID", "1.0");
        m_w.open("elements");
        for (const auto& e : m_elements) writeElementExtension(*e);
        m_w.close();
        m_w.open("connectors");
        for (const auto& c : m_connectors) writeConnectorExtension(*c);
        m_w.close();
        m_w.open("diagrams");
        for (const auto* pkg : m_packages) {
            for (const auto& d : pkg->GetDiagrams()) if (d) writeDiagram(*d);
        }
        m_w.close();
        m_w.close();
    }

    void writeElementExtension(const Element& e) {
        m_w.open("element").attr("xmi:idref", m_elementXmi[e.GetElementID()]).attr("xmi:type", uml_metaclass(e.GetType()))
            .attr("name", e.GetName()).attr("scope", "public");
        m_w.open("properties")
            .attr("sType", e.GetStoredTypeName().empty() ? ea_object_type_name(e.GetType()) : e.GetStoredTypeName())
            .attr("nType", e.GetNType())
            .attrIfNotEmpty("stereotype", e.GetStereotype())
            .attrIfNotEmpty("alias", e.GetAlias())
            .attrIfNotEmpty("documentation", e.GetNotes())
            .attrIfNotEmpty("status", e.GetStatus())
            .attrIfNotEmpty("author", e.GetAuthor())
            .attrIfNotEmpty("version", e.GetVersion())
            .attr("isAbstract", e.IsAbstract() ? "true" : "false")
            .close();
        // Feature types by name: UML can only reference classifiers, the name keeps "std::string", "Linea[]", ...
        if (!e.GetAttributes().empty()) {
            m_w.open("attributes");
            for (const auto& a : e.GetAttributes()) {
                if (!a) continue;
                m_w.open("attribute").attr("xmi:idref", id_from_guid("EAID_", a->GetAttributeGUID())).attr("name", a->GetName())
                    .attr("type", a->GetType()).attr("lowerBound", a->GetLowerBound()).attr("upperBound", a->GetUpperBound())
                    .attr("visibility", visibility(a->GetVisibility()));
                m_w.attrIfNotEmpty("default", a->GetDefault()).attrIfNotEmpty("documentation", a->GetNotes());
                m_w.close();
            }
            m_w.close();
        }
        if (!e.GetMethods().empty()) {
            m_w.open("operations");
            for (const auto& m : e.GetMethods()) {
                if (!m) continue;
                m_w.open("operation").attr("xmi:idref", id_from_guid("EAID_", m->GetMethodGUID())).attr("name", m->GetName())
                    .attr("type", m->GetReturnType()).attrIfNotEmpty("documentation", m->GetNotes());
                for (const auto& p : m->GetParameters()) {
                    m_w.open("parameter").attr("name", p.name).attr("type", p.type).attr("kind", p.kind)
                        .attrIfNotEmpty("default", p.default_value).close();
                }
                m_w.close();
            }
            m_w.close();
        }
        m_w.close();
    }

    void writeConnectorEnd(const char* tag, int elementId, const std::string& role, const std::string& card) {
        const auto* e = m_elementById[elementId];
        m_w.open(tag).attr("xmi:idref", m_elementXmi[elementId]);
        m_w.open("model").attr("type", e ? ea_object_type_name(e->GetType()) : "").attr("name", e ? e->GetName() : "").close();
        m_w.open("role").attr("name", role).close();
        m_w.open("type").attr("multiplicity", card).close();
        m_w.close();
    }

    void writeConnectorExtension(const Connector& c) {
        // relationships with a UML form are referenced; the others (messages, flows, ...) live only here
        m_w.open("connector").attr(m_inUml.count(c.GetConnectorID()) ? "xmi:idref" : "xmi:id", m_connectorXmi[c.GetConnectorID()])
            .attr("name", c.GetName());
        writeConnectorEnd("source", c.GetClientID(), c.GetClientRole(), c.GetClientMultiplicity());
        writeConnectorEnd("target", c.GetSupplierID(), c.GetSupplierRole(), c.GetSupplierMultiplicity());
        m_w.open("properties").attr("ea_type", to_string(c.GetType())).attrIfNotEmpty("subtype", c.GetSubtype())
            .attrIfNotEmpty("stereotype", c.GetStereotype()).attr("direction", c.GetDirection())
            .attrIfNotEmpty("documentation", c.GetNotes()).close();
        bool hasPData = false;
        for (int s = 1; s <= Connector::PDataSlots; ++s) hasPData = hasPData || !c.GetPData(s).empty();
        if (c.GetSequenceNo() != 0 || c.GetStartY() != 0 || hasPData) {
            m_w.open("sequence").attr("seqno", c.GetSequenceNo()).attr("startY", c.GetStartY()).attr("endY", c.GetEndY());
            for (int s = 1; s <= Connector::PDataSlots; ++s) m_w.attrIfNotEmpty("pdata" + std::to_string(s), c.GetPData(s));
            m_w.close();
        }
        m_w.close();
    }

    void writeDiagram(const Diagram& d) {
        m_w.open("diagram").attr("xmi:id", id_from_guid("EAID_", d.GetDiagramGUID()));
        m_w.open("model").attr("package", m_packageXmi[d.GetPackageID()]).close();
        m_w.open("properties").attr("name", d.GetName()).attr("type", to_string(d.GetType()))
            .attrIfNotEmpty("stereotype", d.GetStereotype()).attrIfNotEmpty("documentation", d.GetNotes()).close();
        m_w.open("elements");
        for (const auto& o : d.GetDiagramObjects()) {
            if (!o || !m_elementXmi.count(o->GetElementID())) continue;
            // EA coordinates as stored (Top/Bottom negative downwards)
            m_w.open("element").attr("subject", m_elementXmi[o->GetElementID()]).attr("seqno", o->GetSequence())
                .attr("geometry", "Left=" + std::to_string(o->GetLeft()) + ";Top=" + std::to_string(o->GetTop()) +
                                      ";Right=" + std::to_string(o->GetRight()) + ";Bottom=" + std::to_string(o->GetBottom()) + ";")
                .attrIfNotEmpty("style", o->GetStyle()).close();
        }
        m_w.close();
        m_w.open("links");
        for (const auto& l : d.GetDiagramLinks()) {
            if (!l || !m_connectorXmi.count(l->GetConnectorID())) continue;
            m_w.open("link").attr("subject", m_connectorXmi[l->GetConnectorID()]).attrIfNotEmpty("geometry", l->GetGeometry())
                .attrIfNotEmpty("path", l->GetPath()).attrIfNotEmpty("style", l->GetStyle())
                .attr("hidden", l->IsHidden() ? "true" : "false").close();
        }
        m_w.close();
        m_w.close();
        ++m_result.diagrams;
    }

    void writeStereotypeApplications() {
        size_t skipped = 0;
        for (const auto& e : m_elements) {
            const auto& st = e->GetStereotype();
            if (st.empty() || e->GetType() == ObjectType::Note || e->GetType() == ObjectType::Text) continue;
            if (!is_xml_name(st)) { ++skipped; continue; } // kept in the extension anyway
            const std::string metaclass = uml_metaclass(e->GetType()).substr(4); // "uml:Class" → "Class"
            m_w.open(std::string(kProfilePrefix) + ":" + st).attr("xmi:id", m_elementXmi[e->GetElementID()] + "_st")
                .attr("base_" + metaclass, m_elementXmi[e->GetElementID()]).close();
        }
        if (skipped > 0) {
            m_result.warnings.push_back(std::to_string(skipped) + " estereotipos con caracteres no válidos en XML solo van en la extensión.");
        }
    }

    Repository& m_repo;
    xml::Writer m_w;
    ExportResult m_result;
    std::vector<const Package*> m_packages;
    std::unordered_map<std::string, const Package*> m_packageByGuid;
    std::unordered_map<int, std::string> m_packageXmi;
    std::vector<std::shared_ptr<Element>> m_elements;
    std::unordered_map<int, const Element*> m_elementById;
    std::unordered_map<int, std::string> m_elementXmi;
    std::unordered_map<int, std::vector<const Element*>> m_elementsOf;
    std::vector<std::shared_ptr<Connector>> m_connectors;
    std::unordered_map<int, std::string> m_connectorXmi;
    std::unordered_set<int> m_inUml; // connectors with a standard UML representation
    std::unordered_map<int, std::vector<const Connector*>> m_ownedBy;
    std::unordered_map<int, std::vector<const Connector*>> m_packagedIn;
    std::unordered_map<int, std::vector<int>> m_annotations;
    std::unordered_map<std::string, std::string> m_classifierByName;
};

} // namespace

ExportResult export_package(Repository& repo, const Package& package) {
    return Exporter(repo).run(package);
}

bool export_package_to_file(Repository& repo, const Package& package, const std::string& path, ExportResult* result) {
    auto r = export_package(repo, package);
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << r.xml;
    const bool ok = static_cast<bool>(out);
    if (result) *result = std::move(r);
    return ok;
}

} // namespace ea::xmi
