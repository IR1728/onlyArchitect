#include "ea/repository/SqliteStorageEngine.hpp"
#include "ea/model/Package.hpp"
#include "ea/model/Element.hpp"
#include "ea/model/Diagram.hpp"
#include "ea/model/Connector.hpp"
#include "ea/core/Utils.hpp"
#include <iostream>
#include <sstream>
#include <unordered_set>

namespace ea {

SqliteStorageEngine::SqliteStorageEngine() = default;

SqliteStorageEngine::~SqliteStorageEngine() {
    Close();
}

bool SqliteStorageEngine::Open(const std::string& path) {
    Close();
    m_path = path;
    int rc = sqlite3_open(path.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        if (m_db) {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
        return false;
    }
    return true;
}

void SqliteStorageEngine::Close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
    m_path.clear();
    m_package_cache.clear();
    m_element_cache.clear();
    m_diagram_cache.clear();
    m_connector_cache.clear();
}

std::vector<std::shared_ptr<Package>> SqliteStorageEngine::LoadRootPackages() {
    std::vector<std::shared_ptr<Package>> roots;
    if (!m_db) return roots;

    const char* sql = "SELECT Package_ID, Parent_ID, Name, Notes, ea_guid, Version, UMLVersion "
                      "FROM t_package "
                      "WHERE Parent_ID = 0 OR Parent_ID NOT IN (SELECT Package_ID FROM t_package) "
                      "ORDER BY Package_ID ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return roots;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        int parent_id = sqlite3_column_int(stmt, 1);
        const char* name_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        const char* notes_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        const char* guid_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        const char* ver_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        const char* uml_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));

        auto pkg = std::make_shared<Package>();
        pkg->SetPackageID(id);
        pkg->SetParentID(parent_id);
        pkg->SetName(name_txt ? name_txt : "");
        pkg->SetNotes(notes_txt ? notes_txt : "");
        pkg->SetPackageGUID(guid_txt ? guid_txt : utils::generate_guid());
        pkg->SetVersion(ver_txt ? ver_txt : "1.0");
        pkg->SetUMLVersion(uml_txt ? uml_txt : "2.5");
        pkg->SetIsModel(true);

        m_package_cache[id] = pkg;
        roots.push_back(pkg);
    }
    sqlite3_finalize(stmt);

    for (auto& root : roots) {
        LoadPackageChildren(*root);
    }

    return roots;
}

void SqliteStorageEngine::LoadPackageChildren(Package& pkg) {
    if (!m_db) return;
    int pkg_id = pkg.GetPackageID();

    // 1. Sub-packages
    const char* pkg_sql = "SELECT Package_ID, Parent_ID, Name, Notes, ea_guid, Version, UMLVersion "
                          "FROM t_package WHERE Parent_ID = ? ORDER BY Name ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, pkg_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, pkg_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            int parent_id = sqlite3_column_int(stmt, 1);
            const char* name_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            const char* notes_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            const char* guid_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            const char* ver_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            const char* uml_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));

            auto sub_pkg = std::make_shared<Package>();
            sub_pkg->SetPackageID(id);
            sub_pkg->SetParentID(parent_id);
            sub_pkg->SetName(name_txt ? name_txt : "");
            sub_pkg->SetNotes(notes_txt ? notes_txt : "");
            sub_pkg->SetPackageGUID(guid_txt ? guid_txt : utils::generate_guid());
            sub_pkg->SetVersion(ver_txt ? ver_txt : "1.0");
            sub_pkg->SetUMLVersion(uml_txt ? uml_txt : "2.5");

            m_package_cache[id] = sub_pkg;
            pkg.GetPackages().AddExisting(sub_pkg);
            LoadPackageChildren(*sub_pkg);
        }
        sqlite3_finalize(stmt);
    }

    // 2. Elements in this package (top-level elements in package)
    const char* elem_sql = "SELECT Object_ID, Object_Type, Name, Stereotype, Note, "
                           "Status, Author, Version, Complexity, GenType, GenFile, Classifier, "
                           "ea_guid, IsLeaf, Abstract, ParentID "
                           "FROM t_object WHERE Package_ID = ? AND (ParentID = 0 OR ParentID IS NULL) "
                           "ORDER BY Name ASC;";
    if (sqlite3_prepare_v2(m_db, elem_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, pkg_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            const char* type_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const char* name_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            const char* st_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            const char* notes_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            const char* status_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            const char* auth_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            const char* ver_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            const char* comp_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
            const char* gt_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
            const char* gf_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
            int classifier = sqlite3_column_int(stmt, 11);
            const char* guid_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
            const char* abs_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));
            int parent_id = sqlite3_column_int(stmt, 15);

            auto elem = std::make_shared<Element>();
            elem->SetElementID(id);
            elem->SetPackageID(pkg_id);
            elem->SetParentID(parent_id);
            elem->SetType(object_type_from_string(type_txt ? type_txt : "Class"));
            elem->SetName(name_txt ? name_txt : "");
            elem->SetStereotype(st_txt ? st_txt : "");
            elem->SetNotes(notes_txt ? notes_txt : "");
            elem->SetStatus(status_txt ? status_txt : "Proposed");
            elem->SetAuthor(auth_txt ? auth_txt : "");
            elem->SetVersion(ver_txt ? ver_txt : "1.0");
            elem->SetComplexity(comp_txt ? comp_txt : "1");
            elem->SetGenType(gt_txt ? gt_txt : "<none>");
            elem->SetGenFile(gf_txt ? gf_txt : "");
            elem->SetClassifierID(classifier);
            elem->SetElementGUID(guid_txt ? guid_txt : utils::generate_guid());
            elem->SetAbstract(abs_txt && (std::string(abs_txt) == "1" || std::string(abs_txt) == "true"));

            m_element_cache[id] = elem;
            LoadElementDetails(*elem);
            pkg.GetElements().AddExisting(elem);
        }
        sqlite3_finalize(stmt);
    }

    // 3. Diagrams in this package
    const char* diag_sql = "SELECT Diagram_ID, Package_ID, ParentID, Diagram_Type, Name, "
                           "Version, Author, Notes, Stereotype, cx, cy, Scale, ea_guid "
                           "FROM t_diagram WHERE Package_ID = ? ORDER BY Name ASC;";
    if (sqlite3_prepare_v2(m_db, diag_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, pkg_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            int parent_id = sqlite3_column_int(stmt, 2);
            const char* type_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            const char* name_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            const char* ver_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            const char* auth_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            const char* notes_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            const char* st_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
            int cx = sqlite3_column_int(stmt, 9);
            int cy = sqlite3_column_int(stmt, 10);
            int scale = sqlite3_column_int(stmt, 11);
            const char* guid_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));

            auto diag = std::make_shared<Diagram>();
            diag->SetDiagramID(id);
            diag->SetPackageID(pkg_id);
            diag->SetParentID(parent_id);
            diag->SetType(diagram_type_from_string(type_txt ? type_txt : "Logical"));
            diag->SetName(name_txt ? name_txt : "");
            diag->SetVersion(ver_txt ? ver_txt : "1.0");
            diag->SetAuthor(auth_txt ? auth_txt : "");
            diag->SetNotes(notes_txt ? notes_txt : "");
            diag->SetStereotype(st_txt ? st_txt : "");
            diag->SetCx(cx > 0 ? cx : 1000);
            diag->SetCy(cy > 0 ? cy : 800);
            diag->SetScale(scale > 0 ? scale : 100);
            diag->SetDiagramGUID(guid_txt ? guid_txt : utils::generate_guid());

            m_diagram_cache[id] = diag;
            LoadDiagramDetails(*diag);
            pkg.GetDiagrams().AddExisting(diag);
        }
        sqlite3_finalize(stmt);
    }
}

void SqliteStorageEngine::LoadElementDetails(Element& elem) {
    if (!m_db) return;
    int elem_id = elem.GetElementID();

    // Attributes
    const char* attr_sql = "SELECT ID, Object_ID, Name, Scope, Type, [Default], Notes, Pos, ea_guid "
                           "FROM t_attribute WHERE Object_ID = ? ORDER BY Pos ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, attr_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, elem_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            auto attr = std::make_shared<Attribute>();
            attr->SetAttributeID(sqlite3_column_int(stmt, 0));
            attr->SetObjectID(elem_id);
            const char* n = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            const char* s = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            const char* t = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            const char* d = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            const char* no = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            int p = sqlite3_column_int(stmt, 7);
            const char* g = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));

            attr->SetName(n ? n : "");
            attr->SetVisibility(scope_from_string(s ? s : "Public"));
            attr->SetType(t ? t : "int");
            attr->SetDefault(d ? d : "");
            attr->SetNotes(no ? no : "");
            attr->SetPos(p);
            attr->SetAttributeGUID(g ? g : utils::generate_guid());
            elem.GetAttributes().AddExisting(attr);
        }
        sqlite3_finalize(stmt);
    }

    // Operations / Methods
    const char* op_sql = "SELECT OperationID, Object_ID, Name, Scope, Type, Notes, Pos, ea_guid "
                         "FROM t_operation WHERE Object_ID = ? ORDER BY Pos ASC;";
    if (sqlite3_prepare_v2(m_db, op_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, elem_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            auto meth = std::make_shared<Method>();
            int op_id = sqlite3_column_int(stmt, 0);
            meth->SetMethodID(op_id);
            meth->SetObjectID(elem_id);
            const char* n = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            const char* s = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            const char* t = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            const char* no = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            int p = sqlite3_column_int(stmt, 6);
            const char* g = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));

            meth->SetName(n ? n : "");
            meth->SetVisibility(scope_from_string(s ? s : "Public"));
            meth->SetReturnType(t ? t : "void");
            meth->SetNotes(no ? no : "");
            meth->SetPos(p);
            meth->SetMethodGUID(g ? g : utils::generate_guid());

            // Load parameters for operation
            const char* param_sql = "SELECT Name, Type, DefaultValue, Kind, Pos "
                                   "FROM t_operationparams WHERE OperationID = ? ORDER BY Pos ASC;";
            sqlite3_stmt* param_stmt = nullptr;
            if (sqlite3_prepare_v2(m_db, param_sql, -1, &param_stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_int(param_stmt, 1, op_id);
                while (sqlite3_step(param_stmt) == SQLITE_ROW) {
                    Parameter param;
                    const char* pn = reinterpret_cast<const char*>(sqlite3_column_text(param_stmt, 0));
                    const char* pt = reinterpret_cast<const char*>(sqlite3_column_text(param_stmt, 1));
                    const char* pd = reinterpret_cast<const char*>(sqlite3_column_text(param_stmt, 2));
                    const char* pk = reinterpret_cast<const char*>(sqlite3_column_text(param_stmt, 3));
                    param.name = pn ? pn : "";
                    param.type = pt ? pt : "";
                    param.default_value = pd ? pd : "";
                    param.kind = pk ? pk : "in";
                    param.pos = sqlite3_column_int(param_stmt, 4);
                    meth->AddParameter(std::move(param));
                }
                sqlite3_finalize(param_stmt);
            }

            elem.GetMethods().AddExisting(meth);
        }
        sqlite3_finalize(stmt);
    }

    // Connectors connected to this element
    const char* conn_sql = "SELECT Connector_ID, Name, Connector_Type, Subtype, Stereotype, "
                           "Start_Object_ID, End_Object_ID, SourceRole, DestRole, SourceCard, "
                           "DestCard, Direction, Notes, ea_guid "
                           "FROM t_connector WHERE Start_Object_ID = ? OR End_Object_ID = ?;";
    if (sqlite3_prepare_v2(m_db, conn_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, elem_id);
        sqlite3_bind_int(stmt, 2, elem_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int c_id = sqlite3_column_int(stmt, 0);
            auto it = m_connector_cache.find(c_id);
            if (it != m_connector_cache.end()) {
                elem.GetConnectors().AddExisting(it->second);
                continue;
            }

            auto conn = std::make_shared<Connector>();
            conn->SetConnectorID(c_id);
            const char* n = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const char* t = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            const char* sub = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            const char* st = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            int start_id = sqlite3_column_int(stmt, 5);
            int end_id = sqlite3_column_int(stmt, 6);
            const char* s_role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            const char* d_role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
            const char* s_card = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
            const char* d_card = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
            const char* dir = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
            const char* no = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
            const char* g = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13));

            conn->SetName(n ? n : "");
            conn->SetType(connector_type_from_string(t ? t : "Association"));
            conn->SetSubtype(sub ? sub : "");
            conn->SetStereotype(st ? st : "");
            conn->SetClientID(start_id);
            conn->SetSupplierID(end_id);
            conn->SetClientRole(s_role ? s_role : "");
            conn->SetSupplierRole(d_role ? d_role : "");
            conn->SetClientMultiplicity(s_card ? s_card : "");
            conn->SetSupplierMultiplicity(d_card ? d_card : "");
            conn->SetDirection(dir ? dir : "Source -> Destination");
            conn->SetNotes(no ? no : "");
            conn->SetConnectorGUID(g ? g : utils::generate_guid());

            m_connector_cache[c_id] = conn;
            elem.GetConnectors().AddExisting(conn);
        }
        sqlite3_finalize(stmt);
    }
}

void SqliteStorageEngine::LoadDiagramDetails(Diagram& diag) {
    if (!m_db) return;
    int diag_id = diag.GetDiagramID();

    // Diagram objects
    const char* obj_sql = "SELECT Diagram_ID, Object_ID, RectLeft, RectTop, RectRight, RectBottom, "
                          "Sequence, ObjectStyle, Instance_ID "
                          "FROM t_diagramobjects WHERE Diagram_ID = ? ORDER BY Sequence ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, obj_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, diag_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int d_id = sqlite3_column_int(stmt, 0);
            int o_id = sqlite3_column_int(stmt, 1);
            int left = sqlite3_column_int(stmt, 2);
            int top = sqlite3_column_int(stmt, 3);
            int right = sqlite3_column_int(stmt, 4);
            int bottom = sqlite3_column_int(stmt, 5);
            int seq = sqlite3_column_int(stmt, 6);
            const char* style = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            int inst_id = sqlite3_column_int(stmt, 8);

            auto dobj = std::make_shared<DiagramObject>(d_id, o_id, left, top, right, bottom, seq);
            dobj->SetStyle(style ? style : "");
            dobj->SetInstanceID(inst_id);
            diag.GetDiagramObjects().AddExisting(dobj);
        }
        sqlite3_finalize(stmt);
    }

    // Diagram links
    std::unordered_set<int> known_connector_ids;
    const char* link_sql = "SELECT DiagramID, ConnectorID, Geometry, Style, Hidden, Path, Instance_ID "
                           "FROM t_diagramlinks WHERE DiagramID = ?;";
    if (sqlite3_prepare_v2(m_db, link_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, diag_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int d_id = sqlite3_column_int(stmt, 0);
            int c_id = sqlite3_column_int(stmt, 1);
            const char* geom = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            const char* style = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            int hidden = sqlite3_column_int(stmt, 4);
            const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            int inst_id = sqlite3_column_int(stmt, 6);

            auto dlink = std::make_shared<DiagramLink>(d_id, c_id);
            dlink->SetGeometry(geom ? geom : "");
            dlink->SetStyle(style ? style : "");
            dlink->SetHidden(hidden != 0);
            dlink->SetPath(path ? path : "");
            dlink->SetInstanceID(inst_id);
            diag.GetDiagramLinks().AddExisting(dlink);
            known_connector_ids.insert(c_id);
        }
        sqlite3_finalize(stmt);
    }

    // Discover implicit connectors between diagram objects (EA convention: connectors between visible objects belong to diagram unless explicitly hidden)
    std::string obj_ids_str;
    for (size_t i = 0; i < diag.GetDiagramObjects().size(); ++i) {
        auto obj = diag.GetDiagramObjects().GetAt(static_cast<short>(i));
        if (obj) {
            if (!obj_ids_str.empty()) obj_ids_str += ",";
            obj_ids_str += std::to_string(obj->GetElementID());
        }
    }

    if (!obj_ids_str.empty()) {
        std::string implicit_sql = "SELECT Connector_ID FROM t_connector "
                                   "WHERE Start_Object_ID IN (" + obj_ids_str + ") "
                                   "AND End_Object_ID IN (" + obj_ids_str + ");";
        sqlite3_stmt* imp_stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, implicit_sql.c_str(), -1, &imp_stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(imp_stmt) == SQLITE_ROW) {
                int c_id = sqlite3_column_int(imp_stmt, 0);
                if (known_connector_ids.find(c_id) == known_connector_ids.end()) {
                    auto dlink = std::make_shared<DiagramLink>(diag_id, c_id);
                    dlink->SetHidden(false);
                    diag.GetDiagramLinks().AddExisting(dlink);
                    known_connector_ids.insert(c_id);
                }
            }
            sqlite3_finalize(imp_stmt);
        }
    }
}

std::shared_ptr<Package> SqliteStorageEngine::LoadPackage(int package_id) {
    auto it = m_package_cache.find(package_id);
    if (it != m_package_cache.end()) return it->second;

    const char* sql = "SELECT Package_ID, Parent_ID, Name, Notes, ea_guid, Version, UMLVersion "
                      "FROM t_package WHERE Package_ID = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return nullptr;
    sqlite3_bind_int(stmt, 1, package_id);

    std::shared_ptr<Package> pkg = nullptr;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        pkg = std::make_shared<Package>();
        pkg->SetPackageID(sqlite3_column_int(stmt, 0));
        pkg->SetParentID(sqlite3_column_int(stmt, 1));
        const char* n = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        const char* no = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        const char* g = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        const char* v = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        const char* u = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));

        pkg->SetName(n ? n : "");
        pkg->SetNotes(no ? no : "");
        pkg->SetPackageGUID(g ? g : utils::generate_guid());
        pkg->SetVersion(v ? v : "1.0");
        pkg->SetUMLVersion(u ? u : "2.5");
        m_package_cache[package_id] = pkg;
    }
    sqlite3_finalize(stmt);

    if (pkg) {
        LoadPackageChildren(*pkg);
    }
    return pkg;
}

std::shared_ptr<Package> SqliteStorageEngine::LoadPackageByGuid(const Guid& guid) {
    const char* sql = "SELECT Package_ID FROM t_package WHERE ea_guid = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return nullptr;
    sqlite3_bind_text(stmt, 1, guid.c_str(), -1, SQLITE_STATIC);
    int id = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id ? LoadPackage(id) : nullptr;
}

std::shared_ptr<Element> SqliteStorageEngine::LoadElement(int element_id) {
    auto it = m_element_cache.find(element_id);
    if (it != m_element_cache.end()) return it->second;

    const char* sql = "SELECT Object_ID, Object_Type, Name, Stereotype, Note, "
                      "Status, Author, Version, Complexity, GenType, GenFile, Classifier, "
                      "ea_guid, IsLeaf, Abstract, ParentID, Package_ID "
                      "FROM t_object WHERE Object_ID = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return nullptr;
    sqlite3_bind_int(stmt, 1, element_id);

    std::shared_ptr<Element> elem = nullptr;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        elem = std::make_shared<Element>();
        elem->SetElementID(sqlite3_column_int(stmt, 0));
        const char* type_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* name_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        const char* st_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        const char* notes_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        const char* status_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        const char* auth_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        const char* ver_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        const char* comp_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        const char* gt_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        const char* gf_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        int classifier = sqlite3_column_int(stmt, 11);
        const char* guid_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
        const char* abs_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));
        int parent_id = sqlite3_column_int(stmt, 15);
        int pkg_id = sqlite3_column_int(stmt, 16);

        elem->SetPackageID(pkg_id);
        elem->SetParentID(parent_id);
        elem->SetType(object_type_from_string(type_txt ? type_txt : "Class"));
        elem->SetName(name_txt ? name_txt : "");
        elem->SetStereotype(st_txt ? st_txt : "");
        elem->SetNotes(notes_txt ? notes_txt : "");
        elem->SetStatus(status_txt ? status_txt : "Proposed");
        elem->SetAuthor(auth_txt ? auth_txt : "");
        elem->SetVersion(ver_txt ? ver_txt : "1.0");
        elem->SetComplexity(comp_txt ? comp_txt : "1");
        elem->SetGenType(gt_txt ? gt_txt : "<none>");
        elem->SetGenFile(gf_txt ? gf_txt : "");
        elem->SetClassifierID(classifier);
        elem->SetElementGUID(guid_txt ? guid_txt : utils::generate_guid());
        elem->SetAbstract(abs_txt && (std::string(abs_txt) == "1" || std::string(abs_txt) == "true"));

        m_element_cache[element_id] = elem;
    }
    sqlite3_finalize(stmt);

    if (elem) {
        LoadElementDetails(*elem);
    }
    return elem;
}

std::shared_ptr<Element> SqliteStorageEngine::LoadElementByGuid(const Guid& guid) {
    const char* sql = "SELECT Object_ID FROM t_object WHERE ea_guid = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return nullptr;
    sqlite3_bind_text(stmt, 1, guid.c_str(), -1, SQLITE_STATIC);
    int id = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id ? LoadElement(id) : nullptr;
}

std::shared_ptr<Diagram> SqliteStorageEngine::LoadDiagram(int diagram_id) {
    auto it = m_diagram_cache.find(diagram_id);
    if (it != m_diagram_cache.end()) return it->second;

    const char* sql = "SELECT Diagram_ID, Package_ID, ParentID, Diagram_Type, Name, "
                      "Version, Author, Notes, Stereotype, cx, cy, Scale, ea_guid "
                      "FROM t_diagram WHERE Diagram_ID = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return nullptr;
    sqlite3_bind_int(stmt, 1, diagram_id);

    std::shared_ptr<Diagram> diag = nullptr;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        diag = std::make_shared<Diagram>();
        diag->SetDiagramID(sqlite3_column_int(stmt, 0));
        diag->SetPackageID(sqlite3_column_int(stmt, 1));
        diag->SetParentID(sqlite3_column_int(stmt, 2));
        const char* type_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        const char* name_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        const char* ver_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        const char* auth_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        const char* notes_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        const char* st_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        int cx = sqlite3_column_int(stmt, 9);
        int cy = sqlite3_column_int(stmt, 10);
        int scale = sqlite3_column_int(stmt, 11);
        const char* guid_txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));

        diag->SetType(diagram_type_from_string(type_txt ? type_txt : "Logical"));
        diag->SetName(name_txt ? name_txt : "");
        diag->SetVersion(ver_txt ? ver_txt : "1.0");
        diag->SetAuthor(auth_txt ? auth_txt : "");
        diag->SetNotes(notes_txt ? notes_txt : "");
        diag->SetStereotype(st_txt ? st_txt : "");
        diag->SetCx(cx > 0 ? cx : 1000);
        diag->SetCy(cy > 0 ? cy : 800);
        diag->SetScale(scale > 0 ? scale : 100);
        diag->SetDiagramGUID(guid_txt ? guid_txt : utils::generate_guid());

        m_diagram_cache[diagram_id] = diag;
    }
    sqlite3_finalize(stmt);

    if (diag) {
        LoadDiagramDetails(*diag);
    }
    return diag;
}

std::shared_ptr<Diagram> SqliteStorageEngine::LoadDiagramByGuid(const Guid& guid) {
    const char* sql = "SELECT Diagram_ID FROM t_diagram WHERE ea_guid = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return nullptr;
    sqlite3_bind_text(stmt, 1, guid.c_str(), -1, SQLITE_STATIC);
    int id = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id ? LoadDiagram(id) : nullptr;
}

std::shared_ptr<Connector> SqliteStorageEngine::LoadConnector(int connector_id) {
    auto it = m_connector_cache.find(connector_id);
    if (it != m_connector_cache.end()) return it->second;

    const char* sql = "SELECT Connector_ID, Name, Connector_Type, Subtype, Stereotype, "
                      "Start_Object_ID, End_Object_ID, SourceRole, DestRole, SourceCard, "
                      "DestCard, Direction, Notes, ea_guid "
                      "FROM t_connector WHERE Connector_ID = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return nullptr;
    sqlite3_bind_int(stmt, 1, connector_id);

    std::shared_ptr<Connector> conn = nullptr;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        conn = std::make_shared<Connector>();
        conn->SetConnectorID(sqlite3_column_int(stmt, 0));
        const char* n = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* t = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        const char* sub = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        const char* st = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        int start_id = sqlite3_column_int(stmt, 5);
        int end_id = sqlite3_column_int(stmt, 6);
        const char* s_role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        const char* d_role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        const char* s_card = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        const char* d_card = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        const char* dir = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
        const char* no = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
        const char* g = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13));

        conn->SetName(n ? n : "");
        conn->SetType(connector_type_from_string(t ? t : "Association"));
        conn->SetSubtype(sub ? sub : "");
        conn->SetStereotype(st ? st : "");
        conn->SetClientID(start_id);
        conn->SetSupplierID(end_id);
        conn->SetClientRole(s_role ? s_role : "");
        conn->SetSupplierRole(d_role ? d_role : "");
        conn->SetClientMultiplicity(s_card ? s_card : "");
        conn->SetSupplierMultiplicity(d_card ? d_card : "");
        conn->SetDirection(dir ? dir : "Source -> Destination");
        conn->SetNotes(no ? no : "");
        conn->SetConnectorGUID(g ? g : utils::generate_guid());

        m_connector_cache[connector_id] = conn;
    }
    sqlite3_finalize(stmt);
    return conn;
}

std::shared_ptr<Connector> SqliteStorageEngine::LoadConnectorByGuid(const Guid& guid) {
    const char* sql = "SELECT Connector_ID FROM t_connector WHERE ea_guid = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return nullptr;
    sqlite3_bind_text(stmt, 1, guid.c_str(), -1, SQLITE_STATIC);
    int id = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id ? LoadConnector(id) : nullptr;
}

bool SqliteStorageEngine::SavePackage(Package& pkg) {
    if (!m_db) return false;
    const char* sql = "INSERT INTO t_package (Package_ID, Parent_ID, Name, Notes, ea_guid, Version, UMLVersion) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?) "
                      "ON CONFLICT(Package_ID) DO UPDATE SET "
                      "Parent_ID=excluded.Parent_ID, Name=excluded.Name, Notes=excluded.Notes, "
                      "Version=excluded.Version, UMLVersion=excluded.UMLVersion;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    if (pkg.GetPackageID() == 0) {
        sqlite3_stmt* id_stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, "SELECT COALESCE(MAX(Package_ID), 0) + 1 FROM t_package;", -1, &id_stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(id_stmt) == SQLITE_ROW) {
                pkg.SetPackageID(sqlite3_column_int(id_stmt, 0));
            }
            sqlite3_finalize(id_stmt);
        }
    }

    sqlite3_bind_int(stmt, 1, pkg.GetPackageID());
    sqlite3_bind_int(stmt, 2, pkg.GetParentID());
    sqlite3_bind_text(stmt, 3, pkg.GetName().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, pkg.GetNotes().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, pkg.GetPackageGUID().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, pkg.GetVersion().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, pkg.GetUMLVersion().c_str(), -1, SQLITE_STATIC);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool SqliteStorageEngine::SaveElement(Element& elem) {
    if (!m_db) return false;
    if (elem.GetElementID() == 0) {
        sqlite3_stmt* id_stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, "SELECT COALESCE(MAX(Object_ID), 0) + 1 FROM t_object;", -1, &id_stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(id_stmt) == SQLITE_ROW) {
                elem.SetElementID(sqlite3_column_int(id_stmt, 0));
            }
            sqlite3_finalize(id_stmt);
        }
    }

    const char* sql = "INSERT INTO t_object (Object_ID, Package_ID, ParentID, Object_Type, Name, "
                      "Stereotype, Note, Status, Author, Version, Complexity, GenType, GenFile, "
                      "Classifier, ea_guid, Abstract) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
                      "ON CONFLICT(Object_ID) DO UPDATE SET "
                      "Package_ID=excluded.Package_ID, ParentID=excluded.ParentID, Object_Type=excluded.Object_Type, "
                      "Name=excluded.Name, Stereotype=excluded.Stereotype, Note=excluded.Note, "
                      "Status=excluded.Status, Author=excluded.Author, Version=excluded.Version, "
                      "Complexity=excluded.Complexity, GenType=excluded.GenType, GenFile=excluded.GenFile, "
                      "Classifier=excluded.Classifier, Abstract=excluded.Abstract;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, elem.GetElementID());
    sqlite3_bind_int(stmt, 2, elem.GetPackageID());
    sqlite3_bind_int(stmt, 3, elem.GetParentID());
    sqlite3_bind_text(stmt, 4, to_string(elem.GetType()).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, elem.GetName().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, elem.GetStereotype().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, elem.GetNotes().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, elem.GetStatus().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, elem.GetAuthor().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 10, elem.GetVersion().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 11, elem.GetComplexity().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 12, elem.GetGenType().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 13, elem.GetGenFile().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 14, elem.GetClassifierID());
    sqlite3_bind_text(stmt, 15, elem.GetElementGUID().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 16, elem.IsAbstract() ? "1" : "0", -1, SQLITE_STATIC);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool SqliteStorageEngine::SaveConnector(Connector& conn) {
    if (!m_db) return false;
    if (conn.GetConnectorID() == 0) {
        sqlite3_stmt* id_stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, "SELECT COALESCE(MAX(Connector_ID), 0) + 1 FROM t_connector;", -1, &id_stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(id_stmt) == SQLITE_ROW) {
                conn.SetConnectorID(sqlite3_column_int(id_stmt, 0));
            }
            sqlite3_finalize(id_stmt);
        }
    }
    if (conn.GetConnectorGUID().empty()) {
        conn.SetConnectorGUID(utils::generate_guid());
    }

    const char* sql = "INSERT INTO t_connector (Connector_ID, Name, Connector_Type, Subtype, Stereotype, "
                      "Start_Object_ID, End_Object_ID, SourceRole, DestRole, SourceCard, DestCard, "
                      "Direction, Notes, ea_guid) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
                      "ON CONFLICT(Connector_ID) DO UPDATE SET "
                      "Name=excluded.Name, Connector_Type=excluded.Connector_Type, Subtype=excluded.Subtype, "
                      "Stereotype=excluded.Stereotype, Start_Object_ID=excluded.Start_Object_ID, "
                      "End_Object_ID=excluded.End_Object_ID, SourceRole=excluded.SourceRole, "
                      "DestRole=excluded.DestRole, SourceCard=excluded.SourceCard, DestCard=excluded.DestCard, "
                      "Direction=excluded.Direction, Notes=excluded.Notes;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, conn.GetConnectorID());
    sqlite3_bind_text(stmt, 2, conn.GetName().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, to_string(conn.GetType()).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, conn.GetSubtype().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, conn.GetStereotype().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 6, conn.GetClientID());
    sqlite3_bind_int(stmt, 7, conn.GetSupplierID());
    sqlite3_bind_text(stmt, 8, conn.GetClientRole().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, conn.GetSupplierRole().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 10, conn.GetClientMultiplicity().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 11, conn.GetSupplierMultiplicity().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 12, conn.GetDirection().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 13, conn.GetNotes().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 14, conn.GetConnectorGUID().c_str(), -1, SQLITE_STATIC);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool SqliteStorageEngine::SaveDiagram(Diagram& diag) {
    if (!m_db) return false;
    if (diag.GetDiagramID() == 0) {
        sqlite3_stmt* id_stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, "SELECT COALESCE(MAX(Diagram_ID), 0) + 1 FROM t_diagram;", -1, &id_stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(id_stmt) == SQLITE_ROW) {
                diag.SetDiagramID(sqlite3_column_int(id_stmt, 0));
            }
            sqlite3_finalize(id_stmt);
        }
    }

    const char* sql = "INSERT INTO t_diagram (Diagram_ID, Package_ID, ParentID, Diagram_Type, Name, "
                      "Version, Author, Notes, Stereotype, cx, cy, Scale, ea_guid) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
                      "ON CONFLICT(Diagram_ID) DO UPDATE SET "
                      "Package_ID=excluded.Package_ID, ParentID=excluded.ParentID, Diagram_Type=excluded.Diagram_Type, "
                      "Name=excluded.Name, Version=excluded.Version, Author=excluded.Author, "
                      "Notes=excluded.Notes, Stereotype=excluded.Stereotype, cx=excluded.cx, cy=excluded.cy, "
                      "Scale=excluded.Scale;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, diag.GetDiagramID());
    sqlite3_bind_int(stmt, 2, diag.GetPackageID());
    sqlite3_bind_int(stmt, 3, diag.GetParentID());
    sqlite3_bind_text(stmt, 4, to_string(diag.GetType()).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, diag.GetName().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, diag.GetVersion().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, diag.GetAuthor().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, diag.GetNotes().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, diag.GetStereotype().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 10, diag.GetCx());
    sqlite3_bind_int(stmt, 11, diag.GetCy());
    sqlite3_bind_int(stmt, 12, diag.GetScale());
    sqlite3_bind_text(stmt, 13, diag.GetDiagramGUID().c_str(), -1, SQLITE_STATIC);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    if (ok) {
        for (size_t i = 0; i < diag.GetDiagramObjects().size(); ++i) {
            auto obj = diag.GetDiagramObjects().GetAt(static_cast<short>(i));
            if (obj) {
                SaveDiagramObject(diag.GetDiagramID(), *obj);
            }
        }
    }

    return ok;
}

bool SqliteStorageEngine::SaveDiagramObject(int diagramId, const DiagramObject& obj) {
    if (!m_db) return false;

    const char* check_sql = "SELECT count(*) FROM t_diagramobjects WHERE Diagram_ID = ? AND Object_ID = ?;";
    sqlite3_stmt* check_stmt = nullptr;
    int count = 0;
    if (sqlite3_prepare_v2(m_db, check_sql, -1, &check_stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(check_stmt, 1, diagramId);
        sqlite3_bind_int(check_stmt, 2, obj.GetElementID());
        if (sqlite3_step(check_stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(check_stmt, 0);
        }
        sqlite3_finalize(check_stmt);
    }

    sqlite3_stmt* stmt = nullptr;
    if (count > 0) {
        const char* update_sql = "UPDATE t_diagramobjects SET "
                                 "RectLeft = ?, RectTop = ?, RectRight = ?, RectBottom = ?, Sequence = ? "
                                 "WHERE Diagram_ID = ? AND Object_ID = ?;";
        if (sqlite3_prepare_v2(m_db, update_sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, obj.GetLeft());
        sqlite3_bind_int(stmt, 2, obj.GetTop());
        sqlite3_bind_int(stmt, 3, obj.GetRight());
        sqlite3_bind_int(stmt, 4, obj.GetBottom());
        sqlite3_bind_int(stmt, 5, obj.GetSequence());
        sqlite3_bind_int(stmt, 6, diagramId);
        sqlite3_bind_int(stmt, 7, obj.GetElementID());
    } else {
        const char* insert_sql = "INSERT INTO t_diagramobjects (Diagram_ID, Object_ID, RectLeft, RectTop, RectRight, RectBottom, Sequence) "
                                 "VALUES (?, ?, ?, ?, ?, ?, ?);";
        if (sqlite3_prepare_v2(m_db, insert_sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, diagramId);
        sqlite3_bind_int(stmt, 2, obj.GetElementID());
        sqlite3_bind_int(stmt, 3, obj.GetLeft());
        sqlite3_bind_int(stmt, 4, obj.GetTop());
        sqlite3_bind_int(stmt, 5, obj.GetRight());
        sqlite3_bind_int(stmt, 6, obj.GetBottom());
        sqlite3_bind_int(stmt, 7, obj.GetSequence());
    }

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::string SqliteStorageEngine::SQLQuery(const std::string& sql) {
    if (!m_db) return "<EADATA></EADATA>";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return "<EADATA><Error>" + utils::escape_xml(sqlite3_errmsg(m_db)) + "</Error></EADATA>";
    }

    std::ostringstream xml;
    xml << "<?xml version=\"1.0\"?>\n<EADATA version=\"1.0\" exporter=\"OpenEA\">\n  <Dataset_0>\n    <Data>\n";

    int cols = sqlite3_column_count(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        xml << "      <Row>\n";
        for (int i = 0; i < cols; ++i) {
            const char* col_name = sqlite3_column_name(stmt, i);
            const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            std::string c_name = col_name ? col_name : ("col_" + std::to_string(i));
            xml << "        <" << c_name << ">" << utils::escape_xml(val ? val : "") << "</" << c_name << ">\n";
        }
        xml << "      </Row>\n";
    }
    xml << "    </Data>\n  </Dataset_0>\n</EADATA>\n";

    sqlite3_finalize(stmt);
    return xml.str();
}

bool SqliteStorageEngine::Execute(const std::string& sql) {
    if (!m_db) return false;
    char* err_msg = nullptr;
    int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) {
            std::cerr << "OpenEA SQLite Execute Error: " << err_msg << std::endl;
            sqlite3_free(err_msg);
        }
        return false;
    }
    return true;
}

bool SqliteStorageEngine::CreateNewRepository(const std::string& path) {
    if (!Open(path)) return false;

    const char* ddl = R"SQL(
        CREATE TABLE IF NOT EXISTS t_package (
            Package_ID INTEGER PRIMARY KEY AUTOINCREMENT,
            Name TEXT,
            Parent_ID INTEGER DEFAULT 0,
            CreatedDate TEXT,
            ModifiedDate TEXT,
            Notes TEXT,
            ea_guid TEXT UNIQUE,
            XMLPath TEXT,
            IsControlled INTEGER DEFAULT 0,
            Version TEXT DEFAULT '1.0',
            UMLVersion TEXT DEFAULT '2.5'
        );

        CREATE TABLE IF NOT EXISTS t_object (
            Object_ID INTEGER PRIMARY KEY AUTOINCREMENT,
            Object_Type TEXT,
            Diagram_ID INTEGER DEFAULT 0,
            Name TEXT,
            Alias TEXT,
            Author TEXT,
            Version TEXT DEFAULT '1.0',
            Note TEXT,
            Package_ID INTEGER DEFAULT 0,
            Stereotype TEXT,
            Status TEXT DEFAULT 'Proposed',
            Complexity TEXT DEFAULT '1',
            ea_guid TEXT UNIQUE,
            ParentID INTEGER DEFAULT 0,
            Classifier INTEGER DEFAULT 0,
            GenType TEXT DEFAULT '<none>',
            GenFile TEXT,
            IsLeaf INTEGER DEFAULT 0,
            Abstract TEXT DEFAULT '0'
        );

        CREATE TABLE IF NOT EXISTS t_attribute (
            ID INTEGER PRIMARY KEY AUTOINCREMENT,
            Object_ID INTEGER,
            Name TEXT,
            Scope TEXT DEFAULT 'Public',
            Type TEXT DEFAULT 'int',
            [Default] TEXT,
            Notes TEXT,
            Pos INTEGER DEFAULT 0,
            ea_guid TEXT UNIQUE
        );

        CREATE TABLE IF NOT EXISTS t_operation (
            OperationID INTEGER PRIMARY KEY AUTOINCREMENT,
            Object_ID INTEGER,
            Name TEXT,
            Scope TEXT DEFAULT 'Public',
            Type TEXT DEFAULT 'void',
            Notes TEXT,
            Pos INTEGER DEFAULT 0,
            ea_guid TEXT UNIQUE
        );

        CREATE TABLE IF NOT EXISTS t_operationparams (
            OperationID INTEGER,
            Name TEXT,
            Type TEXT,
            DefaultValue TEXT,
            Kind TEXT DEFAULT 'in',
            Pos INTEGER DEFAULT 0,
            ea_guid TEXT UNIQUE,
            PRIMARY KEY(OperationID, Name)
        );

        CREATE TABLE IF NOT EXISTS t_connector (
            Connector_ID INTEGER PRIMARY KEY AUTOINCREMENT,
            Name TEXT,
            Direction TEXT DEFAULT 'Source -> Destination',
            Notes TEXT,
            Connector_Type TEXT DEFAULT 'Association',
            SubType TEXT,
            SourceCard TEXT,
            SourceRole TEXT,
            DestCard TEXT,
            DestRole TEXT,
            Start_Object_ID INTEGER,
            End_Object_ID INTEGER,
            Stereotype TEXT,
            ea_guid TEXT UNIQUE
        );

        CREATE TABLE IF NOT EXISTS t_diagram (
            Diagram_ID INTEGER PRIMARY KEY AUTOINCREMENT,
            Package_ID INTEGER,
            ParentID INTEGER DEFAULT 0,
            Diagram_Type TEXT DEFAULT 'Logical',
            Name TEXT,
            Version TEXT DEFAULT '1.0',
            Author TEXT,
            Notes TEXT,
            Stereotype TEXT,
            cx INTEGER DEFAULT 1000,
            cy INTEGER DEFAULT 800,
            Scale INTEGER DEFAULT 100,
            CreatedDate TEXT,
            ModifiedDate TEXT,
            ea_guid TEXT UNIQUE
        );

        CREATE TABLE IF NOT EXISTS t_diagramobjects (
            Diagram_ID INTEGER,
            Object_ID INTEGER,
            RectTop INTEGER,
            RectLeft INTEGER,
            RectRight INTEGER,
            RectBottom INTEGER,
            Sequence INTEGER DEFAULT 0,
            ObjectStyle TEXT,
            Instance_ID INTEGER PRIMARY KEY AUTOINCREMENT
        );

        CREATE TABLE IF NOT EXISTS t_diagramlinks (
            DiagramID INTEGER,
            ConnectorID INTEGER,
            Geometry TEXT,
            Style TEXT,
            Hidden INTEGER DEFAULT 0,
            Path TEXT,
            Instance_ID INTEGER PRIMARY KEY AUTOINCREMENT
        );

        CREATE TABLE IF NOT EXISTS t_objectproperties (
            PropertyID INTEGER PRIMARY KEY AUTOINCREMENT,
            Object_ID INTEGER,
            Property TEXT,
            Value TEXT,
            Notes TEXT,
            ea_guid TEXT UNIQUE
        );

        CREATE TABLE IF NOT EXISTS usys_system (
            Property TEXT PRIMARY KEY,
            Value TEXT
        );

        INSERT OR IGNORE INTO usys_system (Property, Value) VALUES ('EA_VERSION', '17.2');
        INSERT OR IGNORE INTO usys_system (Property, Value) VALUES ('SCHEMA_VERSION', '1721');
    )SQL";

    if (!Execute(ddl)) return false;

    // Create root Model Package
    Package root_model;
    root_model.SetName("Model");
    root_model.SetParentID(0);
    root_model.SetPackageGUID(utils::generate_guid());
    root_model.SetIsModel(true);
    return SavePackage(root_model);
}

} // namespace ea
