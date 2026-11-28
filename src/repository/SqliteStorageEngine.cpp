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

namespace {

// Column list shared by every query that builds an Element (order matters: see element_from_row)
constexpr const char* kElementColumns =
    "Object_ID, Object_Type, Name, Stereotype, Note, Status, Author, Version, Complexity, GenType, GenFile, "
    "Classifier, ea_guid, IsLeaf, Abstract, ParentID, Package_ID, NType, Alias";

std::string column_text(sqlite3_stmt* stmt, int col, const char* fallback = "") {
    const auto* txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
    return txt ? txt : fallback;
}

std::shared_ptr<Element> element_from_row(sqlite3_stmt* stmt) {
    auto elem = std::make_shared<Element>();
    const std::string typeName = column_text(stmt, 1, "Class");
    const int ntype = sqlite3_column_int(stmt, 17);
    elem->SetElementID(sqlite3_column_int(stmt, 0));
    elem->SetStoredTypeName(typeName);
    elem->SetNType(ntype);
    elem->SetType(object_type_from_ea(typeName, ntype));
    elem->SetName(column_text(stmt, 2));
    elem->SetStereotype(column_text(stmt, 3));
    elem->SetNotes(column_text(stmt, 4));
    elem->SetStatus(column_text(stmt, 5, "Proposed"));
    elem->SetAuthor(column_text(stmt, 6));
    elem->SetVersion(column_text(stmt, 7, "1.0"));
    elem->SetComplexity(column_text(stmt, 8, "1"));
    elem->SetGenType(column_text(stmt, 9, "<none>"));
    elem->SetGenFile(column_text(stmt, 10));
    elem->SetClassifierID(sqlite3_column_int(stmt, 11));
    const std::string guid = column_text(stmt, 12);
    elem->SetElementGUID(guid.empty() ? utils::generate_guid() : guid);
    const std::string abstract = column_text(stmt, 14);
    elem->SetAbstract(abstract == "1" || abstract == "true");
    elem->SetParentID(sqlite3_column_int(stmt, 15));
    elem->SetPackageID(sqlite3_column_int(stmt, 16));
    elem->SetAlias(column_text(stmt, 18));
    return elem;
}

// Column list shared by every query that builds a Connector (order matters: see connector_from_row)
constexpr const char* kConnectorColumns =
    "Connector_ID, Name, Connector_Type, SubType, Stereotype, Start_Object_ID, End_Object_ID, SourceRole, DestRole, "
    "SourceCard, DestCard, Direction, Notes, ea_guid, SeqNo, PtStartY, PtEndY, DiagramID, "
    "PDATA1, PDATA2, PDATA3, PDATA4, PDATA5, SourceIsNavigable, DestIsNavigable";

std::shared_ptr<Connector> connector_from_row(sqlite3_stmt* stmt) {
    auto conn = std::make_shared<Connector>();
    conn->SetConnectorID(sqlite3_column_int(stmt, 0));
    conn->SetName(column_text(stmt, 1));
    conn->SetType(connector_type_from_string(column_text(stmt, 2, "Association")));
    conn->SetSubtype(column_text(stmt, 3));
    conn->SetStereotype(column_text(stmt, 4));
    conn->SetClientID(sqlite3_column_int(stmt, 5));
    conn->SetSupplierID(sqlite3_column_int(stmt, 6));
    conn->SetClientRole(column_text(stmt, 7));
    conn->SetSupplierRole(column_text(stmt, 8));
    conn->SetClientMultiplicity(column_text(stmt, 9));
    conn->SetSupplierMultiplicity(column_text(stmt, 10));
    conn->SetDirection(column_text(stmt, 11, "Source -> Destination"));
    conn->SetNotes(column_text(stmt, 12));
    const std::string guid = column_text(stmt, 13);
    conn->SetConnectorGUID(guid.empty() ? utils::generate_guid() : guid);
    conn->SetSequenceNo(sqlite3_column_int(stmt, 14));
    conn->SetStartY(sqlite3_column_int(stmt, 15));
    conn->SetEndY(sqlite3_column_int(stmt, 16));
    conn->SetDiagramID(sqlite3_column_int(stmt, 17));
    for (int slot = 1; slot <= Connector::PDataSlots; ++slot) conn->SetPData(slot, column_text(stmt, 17 + slot));
    conn->SetSourceNavigable(sqlite3_column_int(stmt, 23) != 0);
    conn->SetDestNavigable(sqlite3_column_int(stmt, 24) != 0);
    return conn;
}

// Object_Type to write: EA's original name when the kind is unchanged, else the name for the kind
std::string stored_type_name(const Element& elem) {
    const auto& original = elem.GetStoredTypeName();
    if (!original.empty() && object_type_from_ea(original, elem.GetNType()) == elem.GetType()) return original;
    return ea_object_type_name(elem.GetType());
}

} // namespace

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
    const std::string elem_sql = std::string("SELECT ") + kElementColumns +
                                 " FROM t_object WHERE Package_ID = ? AND (ParentID = 0 OR ParentID IS NULL) "
                                 "ORDER BY Name ASC;";
    if (sqlite3_prepare_v2(m_db, elem_sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, pkg_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            auto elem = element_from_row(stmt);
            m_element_cache[elem->GetElementID()] = elem;
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
    const char* attr_sql = "SELECT ID, Object_ID, Name, Scope, Type, [Default], Notes, Pos, ea_guid, "
                           "IsStatic, Const, LowerBound, UpperBound "
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
            attr->SetType(t ? t : "");
            attr->SetDefault(d ? d : "");
            attr->SetNotes(no ? no : "");
            attr->SetPos(p);
            attr->SetAttributeGUID(g ? g : utils::generate_guid());
            attr->SetStatic(sqlite3_column_int(stmt, 9) != 0);
            attr->SetConst(sqlite3_column_int(stmt, 10) != 0);
            attr->SetLowerBound(column_text(stmt, 11, "1"));
            attr->SetUpperBound(column_text(stmt, 12, "1"));
            elem.GetAttributes().AddExisting(attr);
        }
        sqlite3_finalize(stmt);
    }

    // Operations / Methods
    const char* op_sql = "SELECT OperationID, Object_ID, Name, Scope, Type, Notes, Pos, ea_guid, "
                         "IsStatic, Abstract, Const "
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
            meth->SetStatic(sqlite3_column_int(stmt, 8) != 0);
            const std::string abstract = column_text(stmt, 9);
            meth->SetAbstract(abstract == "1" || abstract == "true");
            meth->SetConst(sqlite3_column_int(stmt, 10) != 0);

            // Load parameters for operation
            const char* param_sql = "SELECT Name, Type, [Default], Kind, Pos "
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
    const std::string conn_sql = std::string("SELECT ") + kConnectorColumns +
                                 " FROM t_connector WHERE Start_Object_ID = ? OR End_Object_ID = ?;";
    if (sqlite3_prepare_v2(m_db, conn_sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, elem_id);
        sqlite3_bind_int(stmt, 2, elem_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const int c_id = sqlite3_column_int(stmt, 0);
            auto it = m_connector_cache.find(c_id);
            if (it != m_connector_cache.end()) {
                elem.GetConnectors().AddExisting(it->second);
                continue;
            }
            auto conn = connector_from_row(stmt);
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

    const std::string sql = std::string("SELECT ") + kElementColumns + " FROM t_object WHERE Object_ID = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return nullptr;
    sqlite3_bind_int(stmt, 1, element_id);

    std::shared_ptr<Element> elem = nullptr;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        elem = element_from_row(stmt);
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

    const std::string sql = std::string("SELECT ") + kConnectorColumns + " FROM t_connector WHERE Connector_ID = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return nullptr;
    sqlite3_bind_int(stmt, 1, connector_id);

    std::shared_ptr<Connector> conn = nullptr;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        conn = connector_from_row(stmt);
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
    const char* sql = "INSERT INTO t_package (Package_ID, Parent_ID, Name, Notes, ea_guid, Version, UMLVersion, "
                      "IsControlled, Protected, UseDTD, LogXML) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, 0, 0, 0, 0) "
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
                      "Classifier, ea_guid, Abstract, NType, Alias, IsRoot, IsLeaf, IsSpec, IsActive) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, 0, 0, 0) "
                      "ON CONFLICT(Object_ID) DO UPDATE SET "
                      "Package_ID=excluded.Package_ID, ParentID=excluded.ParentID, Object_Type=excluded.Object_Type, "
                      "NType=excluded.NType, Alias=excluded.Alias, "
                      "Name=excluded.Name, Stereotype=excluded.Stereotype, Note=excluded.Note, "
                      "Status=excluded.Status, Author=excluded.Author, Version=excluded.Version, "
                      "Complexity=excluded.Complexity, GenType=excluded.GenType, GenFile=excluded.GenFile, "
                      "Classifier=excluded.Classifier, Abstract=excluded.Abstract;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, elem.GetElementID());
    sqlite3_bind_int(stmt, 2, elem.GetPackageID());
    sqlite3_bind_int(stmt, 3, elem.GetParentID());
    const std::string typeName = stored_type_name(elem);
    sqlite3_bind_text(stmt, 4, typeName.c_str(), -1, SQLITE_TRANSIENT);
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
    sqlite3_bind_int(stmt, 17, elem.GetNType());
    sqlite3_bind_text(stmt, 18, elem.GetAlias().c_str(), -1, SQLITE_STATIC);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok && SaveFeatures(elem);
}

// Attributes, operations and parameters of `elem` become exactly what the model holds. Rows are
// updated in place (EA columns we do not model, such as IsCollection or Style, are preserved),
// new ones inserted and removed ones deleted, all-or-nothing.
bool SqliteStorageEngine::SaveFeatures(Element& elem) {
    if (!Execute("SAVEPOINT save_features;")) return false;
    const bool ok = SaveAttributes(elem) && SaveOperations(elem);
    if (!ok) Execute("ROLLBACK TO save_features;");
    return Execute("RELEASE save_features;") && ok;
}

namespace {

// Ids stored for `ownerId` in `table` that are not in `kept`
std::vector<int> stale_ids(sqlite3* db, const char* table, const char* idColumn, int ownerId,
                           const std::unordered_set<int>& kept) {
    std::vector<int> out;
    const std::string sql = std::string("SELECT ") + idColumn + " FROM " + table + " WHERE Object_ID = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return out;
    sqlite3_bind_int(stmt, 1, ownerId);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const int id = sqlite3_column_int(stmt, 0);
        if (!kept.count(id)) out.push_back(id);
    }
    sqlite3_finalize(stmt);
    return out;
}

bool run_with_id(sqlite3* db, const char* sql, int id) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, id);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

} // namespace

bool SqliteStorageEngine::SaveAttributes(Element& elem) {
    const char* sql = "INSERT INTO t_attribute (ID, Object_ID, Name, Scope, Type, [Default], Notes, Pos, ea_guid, "
                      "IsStatic, Const, LowerBound, UpperBound) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
                      "ON CONFLICT(ID) DO UPDATE SET Object_ID=excluded.Object_ID, Name=excluded.Name, "
                      "Scope=excluded.Scope, Type=excluded.Type, [Default]=excluded.[Default], Notes=excluded.Notes, "
                      "Pos=excluded.Pos, IsStatic=excluded.IsStatic, Const=excluded.Const, "
                      "LowerBound=excluded.LowerBound, UpperBound=excluded.UpperBound;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    std::unordered_set<int> kept;
    kept.reserve(elem.GetAttributes().size());
    int pos = 0;
    bool ok = true;
    for (const auto& attr : elem.GetAttributes()) {
        if (!attr) continue;
        if (attr->GetAttributeGUID().empty()) attr->SetAttributeGUID(utils::generate_guid());
        attr->SetObjectID(elem.GetElementID());
        attr->SetPos(pos++);
        sqlite3_reset(stmt);
        if (attr->GetAttributeID() > 0) sqlite3_bind_int(stmt, 1, attr->GetAttributeID());
        else sqlite3_bind_null(stmt, 1); // AUTOINCREMENT
        sqlite3_bind_int(stmt, 2, elem.GetElementID());
        sqlite3_bind_text(stmt, 3, attr->GetName().c_str(), -1, SQLITE_STATIC);
        const std::string scope = to_string(attr->GetVisibility());
        sqlite3_bind_text(stmt, 4, scope.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, attr->GetType().c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, attr->GetDefault().c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, attr->GetNotes().c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 8, attr->GetPos());
        sqlite3_bind_text(stmt, 9, attr->GetAttributeGUID().c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 10, attr->IsStatic() ? 1 : 0);
        sqlite3_bind_int(stmt, 11, attr->IsConst() ? 1 : 0);
        sqlite3_bind_text(stmt, 12, attr->GetLowerBound().c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 13, attr->GetUpperBound().c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) != SQLITE_DONE) { ok = false; break; }
        if (attr->GetAttributeID() <= 0) attr->SetAttributeID(static_cast<int>(sqlite3_last_insert_rowid(m_db)));
        kept.insert(attr->GetAttributeID());
    }
    sqlite3_finalize(stmt);
    if (!ok) return false;
    for (int id : stale_ids(m_db, "t_attribute", "ID", elem.GetElementID(), kept)) {
        if (!run_with_id(m_db, "DELETE FROM t_attributetag WHERE ElementID = ?;", id) ||
            !run_with_id(m_db, "DELETE FROM t_attributeconstraints WHERE Object_ID = (SELECT Object_ID FROM "
                               "t_attribute WHERE ID = ?1) AND AttName = (SELECT Name FROM t_attribute WHERE ID = ?1);", id) ||
            !run_with_id(m_db, "DELETE FROM t_attribute WHERE ID = ?;", id)) return false;
    }
    return true;
}

bool SqliteStorageEngine::SaveOperations(Element& elem) {
    const char* sql = "INSERT INTO t_operation (OperationID, Object_ID, Name, Scope, Type, Notes, Pos, ea_guid, "
                      "IsStatic, Abstract, Const, Pure, IsRoot, IsLeaf, IsQuery) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, 0, 0, 0) "
                      "ON CONFLICT(OperationID) DO UPDATE SET Object_ID=excluded.Object_ID, Name=excluded.Name, "
                      "Scope=excluded.Scope, Type=excluded.Type, Notes=excluded.Notes, Pos=excluded.Pos, "
                      "IsStatic=excluded.IsStatic, Abstract=excluded.Abstract, Const=excluded.Const;";
    const char* param_sql = "INSERT INTO t_operationparams (OperationID, Name, Type, [Default], Kind, Pos, ea_guid) "
                            "VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_stmt* param = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    if (sqlite3_prepare_v2(m_db, param_sql, -1, &param, nullptr) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return false;
    }
    std::unordered_set<int> kept;
    kept.reserve(elem.GetMethods().size());
    int pos = 0;
    bool ok = true;
    for (const auto& meth : elem.GetMethods()) {
        if (!meth) continue;
        if (meth->GetMethodGUID().empty()) meth->SetMethodGUID(utils::generate_guid());
        meth->SetObjectID(elem.GetElementID());
        meth->SetPos(pos++);
        sqlite3_reset(stmt);
        if (meth->GetMethodID() > 0) sqlite3_bind_int(stmt, 1, meth->GetMethodID());
        else sqlite3_bind_null(stmt, 1);
        sqlite3_bind_int(stmt, 2, elem.GetElementID());
        sqlite3_bind_text(stmt, 3, meth->GetName().c_str(), -1, SQLITE_STATIC);
        const std::string scope = to_string(meth->GetVisibility());
        sqlite3_bind_text(stmt, 4, scope.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, meth->GetReturnType().c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, meth->GetNotes().c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 7, meth->GetPos());
        sqlite3_bind_text(stmt, 8, meth->GetMethodGUID().c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 9, meth->IsStatic() ? 1 : 0);
        sqlite3_bind_text(stmt, 10, meth->IsAbstract() ? "1" : "0", -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 11, meth->IsConst() ? 1 : 0);
        if (sqlite3_step(stmt) != SQLITE_DONE) { ok = false; break; }
        if (meth->GetMethodID() <= 0) meth->SetMethodID(static_cast<int>(sqlite3_last_insert_rowid(m_db)));
        kept.insert(meth->GetMethodID());

        // Parameters: the list is small and ordered, rewrite it
        if (!run_with_id(m_db, "DELETE FROM t_operationparams WHERE OperationID = ?;", meth->GetMethodID())) {
            ok = false;
            break;
        }
        int ppos = 0;
        for (const auto& p : meth->GetParameters()) {
            sqlite3_reset(param);
            sqlite3_bind_int(param, 1, meth->GetMethodID());
            sqlite3_bind_text(param, 2, p.name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(param, 3, p.type.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(param, 4, p.default_value.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(param, 5, p.kind.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(param, 6, ppos++);
            const std::string guid = utils::generate_guid();
            sqlite3_bind_text(param, 7, guid.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(param) != SQLITE_DONE) { ok = false; break; }
        }
        if (!ok) break;
    }
    sqlite3_finalize(stmt);
    sqlite3_finalize(param);
    if (!ok) return false;
    for (int id : stale_ids(m_db, "t_operation", "OperationID", elem.GetElementID(), kept)) {
        for (const char* del : {"DELETE FROM t_operationparams WHERE OperationID = ?;",
                                "DELETE FROM t_operationtag WHERE ElementID = ?;",
                                "DELETE FROM t_operationpres WHERE OperationID = ?;",
                                "DELETE FROM t_operationposts WHERE OperationID = ?;",
                                "DELETE FROM t_operation WHERE OperationID = ?;"}) {
            if (!run_with_id(m_db, del, id)) return false;
        }
    }
    return true;
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
                      "Direction, Notes, ea_guid, SeqNo, PtStartY, PtEndY, DiagramID, PDATA1, PDATA2, PDATA3, PDATA4, PDATA5, "
                      "SourceIsNavigable, DestIsNavigable, IsRoot, IsLeaf, IsSpec, IsSignal, IsStimulus) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, 0, 0, 0, 0, 0, 0) "
                      "ON CONFLICT(Connector_ID) DO UPDATE SET "
                      "Name=excluded.Name, Connector_Type=excluded.Connector_Type, Subtype=excluded.Subtype, "
                      "Stereotype=excluded.Stereotype, Start_Object_ID=excluded.Start_Object_ID, "
                      "End_Object_ID=excluded.End_Object_ID, SourceRole=excluded.SourceRole, "
                      "DestRole=excluded.DestRole, SourceCard=excluded.SourceCard, DestCard=excluded.DestCard, "
                      "Direction=excluded.Direction, Notes=excluded.Notes, SeqNo=excluded.SeqNo, "
                      "PtStartY=excluded.PtStartY, PtEndY=excluded.PtEndY, DiagramID=excluded.DiagramID, "
                      "PDATA1=excluded.PDATA1, PDATA2=excluded.PDATA2, PDATA3=excluded.PDATA3, "
                      "PDATA4=excluded.PDATA4, PDATA5=excluded.PDATA5;";

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
    sqlite3_bind_int(stmt, 15, conn.GetSequenceNo());
    sqlite3_bind_int(stmt, 16, conn.GetStartY());
    sqlite3_bind_int(stmt, 17, conn.GetEndY());
    sqlite3_bind_int(stmt, 18, conn.GetDiagramID());
    for (int slot = 1; slot <= Connector::PDataSlots; ++slot) {
        const auto& v = conn.GetPData(slot);
        if (v.empty()) sqlite3_bind_null(stmt, 18 + slot); // EA leaves unused slots NULL
        else sqlite3_bind_text(stmt, 18 + slot, v.c_str(), -1, SQLITE_STATIC);
    }

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
                      "Version, Author, Notes, Stereotype, cx, cy, Scale, ea_guid, "
                      "AttPub, AttPri, AttPro, ShowForeign, ShowBorder, ShowPackageContents, Locked) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 1, 1, 1, 1, 1, 1, 0) "
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

bool SqliteStorageEngine::SaveDiagramLink(const DiagramLink& link) {
    if (!m_db) return false;

    const char* update_sql = "UPDATE t_diagramlinks SET Geometry = ?, Style = ?, Hidden = ?, Path = ? "
                             "WHERE DiagramID = ? AND ConnectorID = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, update_sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, link.GetGeometry().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, link.GetStyle().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, link.IsHidden() ? 1 : 0);
    sqlite3_bind_text(stmt, 4, link.GetPath().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, link.GetDiagramID());
    sqlite3_bind_int(stmt, 6, link.GetConnectorID());
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    if (!ok) return false;
    if (sqlite3_changes(m_db) > 0) return true;

    const char* insert_sql = "INSERT INTO t_diagramlinks (DiagramID, ConnectorID, Geometry, Style, Hidden, Path) "
                             "VALUES (?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(m_db, insert_sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, link.GetDiagramID());
    sqlite3_bind_int(stmt, 2, link.GetConnectorID());
    sqlite3_bind_text(stmt, 3, link.GetGeometry().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, link.GetStyle().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, link.IsHidden() ? 1 : 0);
    sqlite3_bind_text(stmt, 6, link.GetPath().c_str(), -1, SQLITE_TRANSIENT);
    ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool SqliteStorageEngine::DeleteDiagramObject(int diagramId, int elementId) {
    if (!m_db) return false;
    const char* sql = "DELETE FROM t_diagramobjects WHERE Diagram_ID = ? AND Object_ID = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, diagramId);
    sqlite3_bind_int(stmt, 2, elementId);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool SqliteStorageEngine::DeleteDiagramLink(int diagramId, int connectorId) {
    if (!m_db) return false;
    const char* sql = "DELETE FROM t_diagramlinks WHERE DiagramID = ? AND ConnectorID = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, diagramId);
    sqlite3_bind_int(stmt, 2, connectorId);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

namespace {
// Runs each statement with `id` bound to its single parameter; stops at the first failure.
bool delete_rows_by_id(sqlite3* db, std::initializer_list<const char*> statements, int id) {
    for (const char* sql : statements) {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(stmt, 1, id);
        bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        if (!ok) return false;
    }
    return true;
}
} // namespace

bool SqliteStorageEngine::DeleteElement(int elementId) {
    if (!m_db) return false;
    m_element_cache.erase(elementId);
    return delete_rows_by_id(m_db, {
        "DELETE FROM t_operationparams WHERE OperationID IN (SELECT OperationID FROM t_operation WHERE Object_ID = ?);",
        "DELETE FROM t_operation WHERE Object_ID = ?;",
        "DELETE FROM t_attribute WHERE Object_ID = ?;",
        "DELETE FROM t_diagramobjects WHERE Object_ID = ?;",
        "DELETE FROM t_object WHERE Object_ID = ?;",
    }, elementId);
}

bool SqliteStorageEngine::DeleteConnector(int connectorId) {
    if (!m_db) return false;
    m_connector_cache.erase(connectorId);
    return delete_rows_by_id(m_db, {
        "DELETE FROM t_diagramlinks WHERE ConnectorID = ?;",
        "DELETE FROM t_connector WHERE Connector_ID = ?;",
    }, connectorId);
}

std::string SqliteStorageEngine::SQLQuery(const std::string& sql) {
    if (!m_db) return "<EADATA></EADATA>";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return "<EADATA><Error>" + utils::escape_xml(sqlite3_errmsg(m_db)) + "</Error></EADATA>";
    }

    std::ostringstream xml;
    xml << "<?xml version=\"1.0\"?>\n<EADATA version=\"1.0\" exporter=\"onlyArchitect\">\n  <Dataset_0>\n    <Data>\n";

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
            std::cerr << "onlyArchitect SQLite Execute Error: " << err_msg << std::endl;
            sqlite3_free(err_msg);
        }
        return false;
    }
    return true;
}

bool SqliteStorageEngine::CreateNewRepository(const std::string& path) {
    if (!Open(path)) return false;

    // Clean-room minimal SQLite schema (15 core UML 2.5.1 interoperability tables + indexes).
    static const char* const kQeaSchema[] = {
#include "QeaSchema.inc"
    };

    if (!Execute("BEGIN;")) return false;
    for (const char* stmt : kQeaSchema) {
        if (!Execute(stmt)) {
            Execute("ROLLBACK;");
            return false;
        }
    }
    if (!Execute("INSERT OR IGNORE INTO usys_system (Property, Value) VALUES ('SCHEMA_FORMAT', 'onlyArchitect-QEA-1.0');"
                 "INSERT OR IGNORE INTO usys_system (Property, Value) VALUES ('UML_VERSION', '2.5.1');"
                 "COMMIT;")) {
        Execute("ROLLBACK;");
        return false;
    }

    // Create root Model Package
    Package root_model;
    root_model.SetName("Model");
    root_model.SetParentID(0);
    root_model.SetPackageGUID(utils::generate_guid());
    root_model.SetIsModel(true);
    return SavePackage(root_model);
}

} // namespace ea
