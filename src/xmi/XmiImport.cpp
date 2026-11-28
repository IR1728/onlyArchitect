#include "ea/xmi/Xmi.hpp"
#include <fstream>
#include <memory>
#include <sstream>
#include <unordered_map>
#include "ea/editor/FeatureText.hpp"
#include "ea/xml/Xml.hpp"

namespace ea::xmi {

namespace {

using xml::Node;

constexpr int TargetPackage = -1; // PackagePlan index meaning "the package imported into"

std::string type_of(const Node& n) {
    return n.attr("xmi:type");
}

std::string id_of(const Node& n) {
    return n.attr("xmi:id");
}

// First id of an attribute that may hold a list ("a b c") or a child <client xmi:idref=".."/>
std::string ref(const Node& n, std::string_view name) {
    if (const auto* v = n.attribute(name)) {
        std::istringstream in(*v);
        std::string first;
        in >> first;
        return first;
    }
    if (const auto* c = n.child(name)) return c->attr("xmi:idref", c->attr("href"));
    return "";
}

// Value of a LiteralX / OpaqueExpression child: value attribute, body attribute or <body> text
std::string literal(const Node* n, std::string fallback) {
    if (!n) return fallback;
    if (const auto* v = n->attribute("value")) return *v;
    if (const auto* b = n->attribute("body")) return *b;
    if (const auto* b = n->child("body")) return b->text;
    // UML: a LiteralInteger without value is 0, an unlimited natural without value is 0 too
    return type_of(*n) == "uml:LiteralInteger" || type_of(*n) == "uml:LiteralUnlimitedNatural" ? "0" : fallback;
}

Scope scope_of(const Node& n) {
    return editor::parse_visibility(n.attr("visibility", "public")).value_or(Scope::Public);
}

bool is_true(const std::string& v) {
    return v == "true" || v == "1";
}

struct PackagePlan {
    int parent{TargetPackage};
    std::string name, notes, xmiId;
    int createdId{0};
    int elementId{0}; // its t_object row (relationships to the package point here)
};

struct ElementPlan {
    int package{TargetPackage};
    std::string xmiId;
    std::unique_ptr<Element> proto = std::make_unique<Element>(); // Element is neither copyable nor movable
    int createdId{0};
};

struct RelationshipPlan {
    std::string xmiId, client, supplier;
    Connector proto;
};

struct DiagramPlan {
    const Node* node{nullptr};
};

class Importer {
public:
    Importer(Repository& repo, int target) : m_repo(repo), m_target(target) {}

    ImportResult run(std::string_view document) {
        if (!m_repo.GetPackageByID(m_target)) return fail("El paquete de destino no existe.");
        auto parsed = xml::parse(document);
        if (!parsed.ok()) return fail("XML no válido (línea " + std::to_string(parsed.line) + "): " + parsed.error);
        const Node& root = *parsed.root;
        const auto rootLocal = root.localName();
        if (rootLocal != "XMI" && rootLocal != "Model" && rootLocal != "Package") {
            return fail("El archivo XML no es un documento XMI/UML válido (elemento raíz inesperado: <" + root.name + ">).");
        }

        index(root);
        const Node* model = findModel(root);
        if (!model) return fail("No hay ningún uml:Model ni uml:Package en el documento.");
        readStereotypeApplications(root);
        readExtension(root);

        if (model->localName() == "Model" || &root == model) walkChildren(*model, TargetPackage);
        else walkNamespace(*model, TargetPackage);
        applyExtensionConnectors();
        if (m_packages.empty() && m_elements.empty()) return fail("El documento no contiene paquetes ni elementos UML.");

        if (!write()) return std::move(m_result);
        m_result.ok = true;
        m_result.packages = m_packages.size();
        m_result.elements = m_elements.size();
        return std::move(m_result);
    }

private:
    ImportResult fail(std::string why) {
        m_result.ok = false;
        m_result.error = std::move(why);
        return std::move(m_result);
    }

    void warn(std::string w) {
        if (m_result.warnings.size() < 50) m_result.warnings.push_back(std::move(w));
    }

    // ---------------------------------------------------------------- reading
    void index(const Node& n) {
        std::vector<const Node*> stack{&n};
        while (!stack.empty()) {
            const Node* cur = stack.back();
            stack.pop_back();
            if (const auto* id = cur->attribute("xmi:id")) m_byId.emplace(*id, cur);
            for (const auto& c : cur->children) stack.push_back(c.get());
        }
    }

    static const Node* findModel(const Node& root) {
        const std::string t = type_of(root);
        if (root.localName() == "Model" || root.localName() == "Package" || t == "uml:Model" || t == "uml:Package") return &root;
        for (const auto& c : root.children) {
            if (c->localName() == "Model" || c->localName() == "Package" || type_of(*c) == "uml:Model") return c.get();
            // XMI 1.1 wrapper: <XMI><XMI.content><UML:Model>
            if (c->localName() == "XMI.content") {
                for (const auto& inner : c->children) {
                    if (inner->localName() == "Model" || inner->localName() == "Package") return inner.get();
                }
            }
        }
        return nullptr;
    }

    void readStereotypeApplications(const Node& root) {
        for (const auto& c : root.children) {
            const auto prefix = c->name.substr(0, c->name.find(':'));
            if (prefix == "xmi" || prefix == "uml" || c->name.find(':') == std::string::npos) continue;
            for (const auto& [k, v] : c->attributes) {
                const auto local = k.substr(k.find(':') == std::string::npos ? 0 : k.find(':') + 1);
                if (local.rfind("base_", 0) == 0 && !m_stereotypes.count(v)) m_stereotypes[v] = std::string(c->localName());
            }
        }
    }

    void readExtension(const Node& root) {
        for (const auto* ext : root.childrenNamed("xmi:Extension")) {
            if (const auto* els = ext->child("elements")) {
                for (const auto* e : els->childrenNamed("element")) m_extElements[e->attr("xmi:idref")] = e;
            }
            if (const auto* cons = ext->child("connectors")) {
                for (const auto* c : cons->childrenNamed("connector")) m_extConnectors.push_back(c);
            }
            if (const auto* ds = ext->child("diagrams")) {
                for (const auto* d : ds->childrenNamed("diagram")) m_diagrams.push_back({d});
            }
        }
    }

    void walkChildren(const Node& owner, int package) {
        for (const auto& c : owner.children) {
            const auto local = c->localName();
            if (local == "packagedElement" || local == "nestedClassifier" || local == "ownedMember" || local == "ownedType") {
                walkNamespace(*c, package);
            } else if (local == "ownedComment" && c->attribute("xmi:id") && package != TargetPackage) {
                // a comment owned by a package is a note on it (unless it only documents the package)
                const std::string annotated = c->attr("annotatedElement");
                if (annotated == m_packages[static_cast<size_t>(package)].xmiId) continue;
                addNote(*c, package);
            } else if (local == "ownedComment" && c->attribute("xmi:id")) {
                addNote(*c, package);
            }
        }
    }

    void walkNamespace(const Node& n, int package) {
        const std::string t = type_of(n);
        const std::string id = id_of(n);
        if (t == "uml:Package" || t == "uml:Model" || t == "uml:Profile") {
            PackagePlan p;
            p.parent = package;
            p.name = n.attr("name", "Package");
            p.xmiId = id;
            for (const auto* c : n.childrenNamed("ownedComment")) {
                if (c->attr("annotatedElement", id) == id) p.notes = c->attr("body", c->child("body") ? c->child("body")->text : "");
            }
            m_packages.push_back(std::move(p));
            const int index = static_cast<int>(m_packages.size()) - 1;
            if (!id.empty()) m_packageIndex[id] = index;
            walkChildren(n, index);
            return;
        }
        if (t == "uml:Association" || t == "uml:AssociationClass") {
            addAssociation(n);
            if (t == "uml:Association") return;
        }
        if (t == "uml:Dependency" || t == "uml:Usage" || t == "uml:Abstraction" || t == "uml:Realization" ||
            t == "uml:Substitution" || t == "uml:InformationFlow") {
            addDependency(n, t);
            return;
        }
        if (t == "uml:Comment") {
            addNote(n, package);
            return;
        }
        const ObjectType kind = object_type_for(t);
        if (kind == ObjectType::Unknown) {
            if (!t.empty()) warn("Se omitió " + t + " \"" + n.attr("name") + "\" (tipo no soportado).");
            return;
        }
        addElement(n, kind, package);
    }

    std::string typeName(const Node& typed) const {
        if (const auto* t = typed.attribute("type")) return nameOf(*t);
        if (const auto* t = typed.child("type")) {
            const std::string idref = t->attr("xmi:idref");
            if (!idref.empty()) return nameOf(idref);
            const std::string href = t->attr("href");
            const auto hash = href.rfind('#');
            return hash == std::string::npos ? href : href.substr(hash + 1);
        }
        return "";
    }

    // Name of a referenced classifier; "EAJava_int"-style references (no such id) give "int"
    std::string nameOf(const std::string& idref) const {
        auto it = m_byId.find(idref);
        if (it != m_byId.end()) return it->second->attr("name");
        if (idref.rfind("EA", 0) == 0 && idref.find('_') != std::string::npos) return idref.substr(idref.find('_') + 1);
        return idref;
    }

    void addElement(const Node& n, ObjectType kind, int package) {
        ElementPlan plan;
        plan.package = package;
        plan.xmiId = id_of(n);
        Element& e = *plan.proto;
        e.SetType(kind);
        e.SetName(n.attr("name"));
        e.SetAbstract(is_true(n.attr("isAbstract")));
        for (const auto* c : n.childrenNamed("ownedComment")) {
            if (c->attr("annotatedElement", plan.xmiId) == plan.xmiId) e.SetNotes(c->attr("body", c->child("body") ? c->child("body")->text : ""));
        }
        if (auto st = m_stereotypes.find(plan.xmiId); st != m_stereotypes.end()) e.SetStereotype(st->second);

        const Node* ext = nullptr;
        if (auto it = m_extElements.find(plan.xmiId); it != m_extElements.end()) ext = it->second;
        std::unordered_map<std::string, const Node*> extAttributes, extOperations;
        if (ext) {
            if (const auto* props = ext->child("properties")) {
                if (const auto* sType = props->attribute("sType")) {
                    const int ntype = std::atoi(props->attr("nType", props->attr("ntype", "0")).c_str());
                    const ObjectType eaKind = object_type_from_ea(*sType, ntype);
                    if (eaKind != ObjectType::Unknown) {
                        e.SetType(eaKind);
                        e.SetStoredTypeName(*sType);
                    }
                    e.SetNType(ntype);
                }
                if (const auto* v = props->attribute("stereotype")) e.SetStereotype(*v);
                if (const auto* v = props->attribute("alias")) e.SetAlias(*v);
                if (const auto* v = props->attribute("documentation")) e.SetNotes(*v);
                if (const auto* v = props->attribute("status")) e.SetStatus(*v);
                if (const auto* v = props->attribute("author")) e.SetAuthor(*v);
                if (const auto* v = props->attribute("version")) e.SetVersion(*v);
                if (const auto* v = props->attribute("isAbstract")) e.SetAbstract(is_true(*v));
            }
            if (const auto* as = ext->child("attributes")) for (const auto* a : as->childrenNamed("attribute")) extAttributes[a->attr("xmi:idref")] = a;
            if (const auto* os = ext->child("operations")) for (const auto* o : os->childrenNamed("operation")) extOperations[o->attr("xmi:idref")] = o;
        }

        for (const auto* lit : n.childrenNamed("ownedLiteral")) {
            auto a = std::make_shared<Attribute>();
            a->SetName(lit->attr("name"));
            a->SetType("");
            e.GetAttributes().AddExisting(a);
        }
        for (const auto* p : n.childrenNamed("ownedAttribute")) {
            if (p->attribute("association")) continue; // an association end owned by the class: the association brings it
            auto a = std::make_shared<Attribute>();
            a->SetName(p->attr("name"));
            a->SetVisibility(scope_of(*p));
            a->SetStatic(is_true(p->attr("isStatic")));
            a->SetConst(is_true(p->attr("isReadOnly")));
            a->SetType(typeName(*p));
            const Node* lowerNode = p->child("lowerValue");
            const Node* upperNode = p->child("upperValue");
            a->SetLowerBound(literal(lowerNode, "1"));
            a->SetUpperBound(literal(upperNode, "1"));
            a->SetDefault(literal(p->child("defaultValue"), ""));
            if (auto it = extAttributes.find(id_of(*p)); it != extAttributes.end()) {
                const Node& x = *it->second;
                a->SetType(x.attr("type", a->GetType()));
                a->SetLowerBound(x.attr("lowerBound", a->GetLowerBound()));
                a->SetUpperBound(x.attr("upperBound", a->GetUpperBound()));
                a->SetDefault(x.attr("default", a->GetDefault()));
                a->SetNotes(x.attr("documentation"));
            }
            e.GetAttributes().AddExisting(a);
        }
        for (const auto* o : n.childrenNamed("ownedOperation")) {
            auto m = std::make_shared<Method>();
            m->SetName(o->attr("name"));
            m->SetVisibility(scope_of(*o));
            m->SetStatic(is_true(o->attr("isStatic")));
            m->SetAbstract(is_true(o->attr("isAbstract")));
            m->SetConst(is_true(o->attr("isQuery")));
            m->SetReturnType("void");
            std::vector<Parameter> params;
            for (const auto* p : o->childrenNamed("ownedParameter")) {
                const std::string direction = p->attr("direction", "in");
                if (direction == "return") {
                    m->SetReturnType(typeName(*p));
                    continue;
                }
                Parameter param;
                param.name = p->attr("name");
                param.type = typeName(*p);
                param.kind = direction;
                param.default_value = literal(p->child("defaultValue"), "");
                param.pos = static_cast<int>(params.size());
                params.push_back(std::move(param));
            }
            if (auto it = extOperations.find(id_of(*o)); it != extOperations.end()) {
                const Node& x = *it->second;
                m->SetReturnType(x.attr("type", m->GetReturnType()));
                m->SetNotes(x.attr("documentation"));
                const auto xs = x.childrenNamed("parameter");
                if (xs.size() == params.size()) {
                    for (size_t i = 0; i < xs.size(); ++i) params[i].type = xs[i]->attr("type", params[i].type);
                }
            }
            if (m->GetReturnType().empty()) m->SetReturnType("void");
            m->SetParameters(std::move(params));
            e.GetMethods().AddExisting(m);
        }

        // Relationships owned by the classifier
        for (const auto* g : n.childrenNamed("generalization")) addOwned(*g, plan.xmiId, ref(*g, "general"), ConnectorType::Generalization, "");
        for (const auto* r : n.childrenNamed("interfaceRealization")) {
            std::string contract = ref(*r, "contract");
            if (contract.empty()) contract = ref(*r, "supplier");
            addOwned(*r, plan.xmiId, contract, ConnectorType::Realization, "");
        }
        for (const auto* i : n.childrenNamed("include")) addOwned(*i, plan.xmiId, ref(*i, "addition"), ConnectorType::UseCaseLink, "include");
        for (const auto* x : n.childrenNamed("extend")) addOwned(*x, plan.xmiId, ref(*x, "extendedCase"), ConnectorType::UseCaseLink, "extend");

        // Nested classifiers, activity nodes/groups/edges, state regions/subvertices/transitions, interaction lifelines
        for (const auto& c : n.children) {
            const auto local = c->localName();
            if (local == "nestedClassifier" || local == "packagedElement" || local == "ownedBehavior" ||
                local == "node" || local == "group" || local == "lifeline" || local == "subvertex") {
                walkNamespace(*c, package);
            } else if (local == "region") {
                for (const auto& rc : c->children) {
                    const auto rlocal = rc->localName();
                    if (rlocal == "subvertex") walkNamespace(*rc, package);
                    else if (rlocal == "transition") {
                        const std::string src = ref(*rc, "source");
                        const std::string dst = ref(*rc, "target");
                        if (!src.empty() && !dst.empty()) addOwned(*rc, src, dst, ConnectorType::Transition, "");
                    }
                }
            } else if (local == "edge") {
                const std::string src = ref(*c, "source");
                const std::string dst = ref(*c, "target");
                if (!src.empty() && !dst.empty()) {
                    const std::string et = type_of(*c);
                    const ConnectorType ct = (et == "uml:ObjectFlow") ? ConnectorType::ObjectFlow : ConnectorType::ControlFlow;
                    addOwned(*c, src, dst, ct, "");
                }
            } else if (local == "fragment" && (type_of(*c) == "uml:CombinedFragment" || type_of(*c) == "uml:InteractionUse")) {
                walkNamespace(*c, package);
            }
        }

        m_elements.push_back(std::move(plan));
        if (!m_elements.back().xmiId.empty()) m_elementIndex[m_elements.back().xmiId] = static_cast<int>(m_elements.size()) - 1;
    }

    void addNote(const Node& n, int package) {
        ElementPlan plan;
        plan.package = package;
        plan.xmiId = id_of(n);
        plan.proto->SetType(ObjectType::Note);
        plan.proto->SetNotes(n.attr("body", n.child("body") ? n.child("body")->text : ""));
        if (auto it = m_extElements.find(plan.xmiId); it != m_extElements.end()) {
            if (const auto* props = it->second->child("properties")) {
                if (props->attr("sType") == "Text") plan.proto->SetType(ObjectType::Text);
            }
            plan.proto->SetName(it->second->attr("name"));
        }
        std::istringstream annotated(n.attr("annotatedElement"));
        for (std::string target; annotated >> target;) {
            RelationshipPlan r;
            r.client = plan.xmiId;
            r.supplier = target;
            r.proto.SetType(ConnectorType::NoteLink);
            m_noteLinks.push_back(std::move(r));
        }
        m_elements.push_back(std::move(plan));
        if (!m_elements.back().xmiId.empty()) m_elementIndex[m_elements.back().xmiId] = static_cast<int>(m_elements.size()) - 1;
    }

    void addOwned(const Node& n, const std::string& client, const std::string& supplier, ConnectorType type, const char* stereotype) {
        RelationshipPlan r;
        r.xmiId = id_of(n);
        r.client = client;
        r.supplier = supplier;
        r.proto.SetType(type);
        r.proto.SetStereotype(stereotype);
        addRelationship(std::move(r));
    }

    void addDependency(const Node& n, const std::string& t) {
        RelationshipPlan r;
        r.xmiId = id_of(n);
        r.proto.SetName(n.attr("name"));
        if (t == "uml:InformationFlow") {
            r.client = ref(n, "informationSource");
            r.supplier = ref(n, "informationTarget");
            r.proto.SetType(ConnectorType::InformationFlow);
        } else {
            r.client = ref(n, "client");
            r.supplier = ref(n, "supplier");
            r.proto.SetType(t == "uml:Usage"         ? ConnectorType::Usage
                            : t == "uml:Abstraction" ? ConnectorType::Abstraction
                            : t == "uml:Realization" ? ConnectorType::Realization
                                                     : ConnectorType::Dependency);
        }
        if (auto st = m_stereotypes.find(r.xmiId); st != m_stereotypes.end()) r.proto.SetStereotype(st->second);
        addRelationship(std::move(r));
    }

    // An association end: owned by the association, or by a classifier (memberEnd → ownedAttribute)
    const Node* associationEnd(const Node& assoc, const std::string& endId) const {
        for (const auto* e : assoc.childrenNamed("ownedEnd")) if (id_of(*e) == endId) return e;
        auto it = m_byId.find(endId);
        return it != m_byId.end() ? it->second : nullptr;
    }

    void addAssociation(const Node& n) {
        std::vector<std::string> endIds;
        std::istringstream in(n.attr("memberEnd"));
        for (std::string id; in >> id;) endIds.push_back(id);
        if (endIds.empty()) {
            for (const auto* me : n.childrenNamed("memberEnd")) endIds.push_back(me->attr("xmi:idref"));
        }
        if (endIds.size() != 2) {
            warn("Asociación \"" + n.attr("name") + "\" sin dos extremos: omitida.");
            return;
        }
        const Node* first = associationEnd(n, endIds[0]);
        const Node* second = associationEnd(n, endIds[1]);
        if (!first || !second) {
            warn("Asociación \"" + n.attr("name") + "\" con extremos no resueltos: omitida.");
            return;
        }
        // The end with aggregation is typed by the part, which is the source in EA (the target is the whole)
        const std::string agg1 = first->attr("aggregation", "none"), agg2 = second->attr("aggregation", "none");
        const Node* part = agg2 != "none" && agg1 == "none" ? second : first;
        const Node* whole = part == first ? second : first;
        const std::string aggregation = part->attr("aggregation", "none");

        RelationshipPlan r;
        r.xmiId = id_of(n);
        r.client = ref(*part, "type");
        r.supplier = ref(*whole, "type");
        r.proto.SetName(n.attr("name"));
        r.proto.SetType(aggregation == "composite" ? ConnectorType::Composition
                        : aggregation == "shared"  ? ConnectorType::Aggregation
                                                   : ConnectorType::Association);
        r.proto.SetClientRole(part->attr("name"));
        r.proto.SetSupplierRole(whole->attr("name"));
        auto card = [](const Node& end) {
            const Node* lo = end.child("lowerValue");
            const Node* up = end.child("upperValue");
            if (!lo && !up) return std::string();
            return editor::format_multiplicity({literal(lo, "1"), literal(up, "1")});
        };
        r.proto.SetClientMultiplicity(card(*part));
        r.proto.SetSupplierMultiplicity(card(*whole));
        addRelationship(std::move(r));
    }

    void addRelationship(RelationshipPlan r) {
        if (!r.xmiId.empty()) {
            auto it = m_relationshipIndex.find(r.xmiId);
            if (it != m_relationshipIndex.end()) return; // already read
            m_relationshipIndex[r.xmiId] = m_relationships.size();
        }
        m_relationships.push_back(std::move(r));
    }

    // The extension knows every relationship with its exact kind (and the ones UML gives no place)
    void applyExtensionConnectors() {
        for (const auto* c : m_extConnectors) {
            const std::string id = c->attr("xmi:idref", c->attr("xmi:id"));
            const Node* src = c->child("source");
            const Node* dst = c->child("target");
            const Node* props = c->child("properties");
            if (!src || !dst || !props) continue;
            RelationshipPlan r;
            r.xmiId = id;
            r.client = src->attr("xmi:idref");
            r.supplier = dst->attr("xmi:idref");
            Connector& conn = r.proto;
            std::string cname = c->attr("name");
            if (cname.empty()) cname = props->attr("name");
            if (cname.empty()) {
                if (const auto* lbl = c->child("labels")) cname = lbl->attr("mt");
            }
            conn.SetName(cname);
            conn.SetType(connector_type_from_string(props->attr("ea_type", "Association")));
            conn.SetSubtype(props->attr("subtype"));
            conn.SetStereotype(props->attr("stereotype"));
            conn.SetDirection(props->attr("direction", "Source -> Destination"));
            conn.SetNotes(props->attr("documentation"));
            if (const auto* role = src->child("role")) conn.SetClientRole(role->attr("name"));
            if (const auto* role = dst->child("role")) conn.SetSupplierRole(role->attr("name"));
            if (const auto* t = src->child("type")) conn.SetClientMultiplicity(t->attr("multiplicity"));
            if (const auto* t = dst->child("type")) conn.SetSupplierMultiplicity(t->attr("multiplicity"));
            if (const auto* s = c->child("sequence")) {
                conn.SetSequenceNo(std::atoi(s->attr("seqno", "0").c_str()));
                conn.SetStartY(std::atoi(s->attr("startY", "0").c_str()));
                conn.SetEndY(std::atoi(s->attr("endY", "0").c_str()));
                for (int slot = 1; slot <= Connector::PDataSlots; ++slot) conn.SetPData(slot, s->attr("pdata" + std::to_string(slot)));
            }
            auto it = m_relationshipIndex.find(id);
            if (it != m_relationshipIndex.end()) m_relationships[it->second] = std::move(r); // exact data wins
            else addRelationship(std::move(r));
        }
        // Note links from comments, unless the extension already listed them
        for (auto& n : m_noteLinks) {
            bool listed = false;
            for (const auto& r : m_relationships) {
                listed = listed || (r.proto.GetType() == ConnectorType::NoteLink &&
                                    ((r.client == n.client && r.supplier == n.supplier) || (r.client == n.supplier && r.supplier == n.client)));
            }
            if (!listed) m_relationships.push_back(std::move(n));
        }
    }

    // ---------------------------------------------------------------- writing
    int packageIdOf(int plan) const {
        return plan == TargetPackage ? m_target : m_packages[static_cast<size_t>(plan)].createdId;
    }

    // Created element for an xmi id: an element, or the t_object row of a package
    int elementIdOf(const std::string& xmiId) const {
        if (auto it = m_elementIndex.find(xmiId); it != m_elementIndex.end()) return m_elements[static_cast<size_t>(it->second)].createdId;
        if (auto it = m_packageIndex.find(xmiId); it != m_packageIndex.end()) return m_packages[static_cast<size_t>(it->second)].elementId;
        return 0;
    }

    bool write() {
        if (!m_repo.Execute("BEGIN;")) {
            fail("No se pudo iniciar la transacción.");
            return false;
        }
        const bool ok = writePackages() && writeElements() && writeRelationships() && writeDiagrams();
        if (!ok) {
            m_repo.Execute("ROLLBACK;");
            m_result.repositoryReloadNeeded = true; // the in-memory model saw objects that no longer exist
            if (m_result.error.empty()) m_result.error = "Error al escribir en el repositorio: " + m_repo.GetLastError();
            return false;
        }
        if (!m_repo.Execute("COMMIT;")) {
            m_repo.Execute("ROLLBACK;");
            m_result.repositoryReloadNeeded = true;
            m_result.error = "No se pudo confirmar la transacción.";
            return false;
        }
        return true;
    }

    bool writePackages() {
        for (auto& p : m_packages) {
            auto pkg = m_repo.CreatePackage(packageIdOf(p.parent), p.name, p.notes);
            if (!pkg) {
                m_result.error = "No se pudo crear el paquete \"" + p.name + "\".";
                return false;
            }
            p.createdId = pkg->GetPackageID();
            if (auto row = m_repo.GetElementByGuid(pkg->GetPackageGUID())) p.elementId = row->GetElementID();
            if (p.parent == TargetPackage) m_result.topPackageIds.push_back(p.createdId);
        }
        return true;
    }

    bool writeElements() {
        for (auto& e : m_elements) {
            e.proto->SetPackageID(packageIdOf(e.package));
            e.proto->SetElementGUID(utils::generate_guid()); // importing twice must not clash
            auto live = m_repo.AddElement(*e.proto);
            if (!live) {
                m_result.error = "No se pudo crear el elemento \"" + e.proto->GetName() + "\".";
                return false;
            }
            e.createdId = live->GetElementID();
        }
        return true;
    }

    bool writeRelationships() {
        for (auto& r : m_relationships) {
            const int client = elementIdOf(r.client), supplier = elementIdOf(r.supplier);
            if (!client || !supplier) {
                warn("Relación " + to_string(r.proto.GetType()) + " \"" + r.proto.GetName() + "\" con un extremo fuera del documento: omitida.");
                continue;
            }
            r.proto.SetClientID(client);
            r.proto.SetSupplierID(supplier);
            r.proto.SetConnectorGUID(utils::generate_guid());
            auto live = m_repo.AddConnector(r.proto);
            if (!live) {
                m_result.error = "No se pudo crear una relación " + to_string(r.proto.GetType()) + ".";
                return false;
            }
            m_connectorIds[r.xmiId] = live->GetConnectorID();
            ++m_result.relationships;
        }
        return true;
    }

    struct Rect { int left = 0, top = 0, right = 0, bottom = 0; };

    static Rect parse_geometry(const std::string& g) {
        Rect r;
        std::istringstream in(g);
        for (std::string kv; std::getline(in, kv, ';');) {
            const auto eq = kv.find('=');
            if (eq == std::string::npos) continue;
            const auto key = kv.substr(0, eq);
            const int v = std::atoi(kv.substr(eq + 1).c_str());
            if (key == "Left") r.left = v;
            else if (key == "Top") r.top = v;
            else if (key == "Right") r.right = v;
            else if (key == "Bottom") r.bottom = v;
        }
        // XMI written by EA uses screen coordinates (y down, positive); the repository stores y negated
        if (r.top >= 0 && r.bottom > r.top) {
            r.top = -r.top;
            r.bottom = -r.bottom;
        }
        return r;
    }

    bool writeDiagrams() {
        for (const auto& d : m_diagrams) {
            const Node& n = *d.node;
            const Node* props = n.child("properties");
            const Node* model = n.child("model");
            if (!props) continue;
            int package = m_target;
            if (model) {
                if (auto it = m_packageIndex.find(model->attr("package")); it != m_packageIndex.end()) {
                    package = m_packages[static_cast<size_t>(it->second)].createdId;
                }
            }
            auto diagram = m_repo.CreateDiagram(package, props->attr("name", "Diagram"),
                                                diagram_type_from_string(props->attr("type", "Logical")), props->attr("stereotype"));
            if (!diagram) {
                m_result.error = "No se pudo crear el diagrama \"" + props->attr("name") + "\".";
                return false;
            }
            int seq = 1;
            std::vector<const Node*> links;
            if (const auto* els = n.child("elements")) {
                for (const auto* e : els->childrenNamed("element")) {
                    const std::string subject = e->attr("subject");
                    if (m_connectorIds.count(subject)) { links.push_back(e); continue; } // EA lists links here too
                    const int elementId = elementIdOf(subject);
                    if (!elementId) continue;
                    const Rect g = parse_geometry(e->attr("geometry"));
                    auto obj = std::make_shared<DiagramObject>();
                    obj->SetDiagramID(diagram->GetDiagramID());
                    obj->SetElementID(elementId);
                    obj->SetLeft(g.left);
                    obj->SetTop(g.top);
                    obj->SetRight(g.right);
                    obj->SetBottom(g.bottom);
                    obj->SetSequence(std::atoi(e->attr("seqno", std::to_string(seq)).c_str()));
                    obj->SetStyle(e->attr("style"));
                    ++seq;
                    if (!m_repo.AddElementToDiagram(*diagram, obj)) return false;
                }
            }
            if (const auto* ls = n.child("links")) for (const auto* l : ls->childrenNamed("link")) links.push_back(l);
            for (const auto* l : links) {
                auto it = m_connectorIds.find(l->attr("subject"));
                if (it == m_connectorIds.end()) continue;
                auto link = std::make_shared<DiagramLink>();
                link->SetDiagramID(diagram->GetDiagramID());
                link->SetConnectorID(it->second);
                link->SetGeometry(l->attr("geometry"));
                link->SetPath(l->attr("path"));
                link->SetStyle(l->attr("style"));
                link->SetHidden(is_true(l->attr("hidden")));
                if (!m_repo.AddConnectorToDiagram(*diagram, link)) return false;
            }
            ++m_result.diagrams;
        }
        return true;
    }

    Repository& m_repo;
    int m_target;
    ImportResult m_result;
    std::unordered_map<std::string, const Node*> m_byId;
    std::unordered_map<std::string, std::string> m_stereotypes;
    std::unordered_map<std::string, const Node*> m_extElements;
    std::vector<const Node*> m_extConnectors;
    std::vector<DiagramPlan> m_diagrams;
    std::vector<PackagePlan> m_packages;
    std::unordered_map<std::string, int> m_packageIndex;
    std::vector<ElementPlan> m_elements;
    std::unordered_map<std::string, int> m_elementIndex;
    std::vector<RelationshipPlan> m_relationships;
    std::unordered_map<std::string, size_t> m_relationshipIndex;
    std::vector<RelationshipPlan> m_noteLinks;
    std::unordered_map<std::string, int> m_connectorIds;
};

} // namespace

ImportResult import_into(Repository& repo, int targetPackageId, std::string_view document) {
    return Importer(repo, targetPackageId).run(document);
}

ImportResult import_file(Repository& repo, int targetPackageId, const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        ImportResult r;
        r.error = "No se pudo abrir " + path;
        return r;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return import_into(repo, targetPackageId, buffer.str());
}

} // namespace ea::xmi
