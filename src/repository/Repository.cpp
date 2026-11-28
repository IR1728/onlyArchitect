#include "ea/repository/Repository.hpp"
#include <algorithm>
#include "ea/repository/SqliteStorageEngine.hpp"
#include "ea/repository/EapxImporter.hpp"
#include "ea/core/Utils.hpp"

namespace ea {

Repository::Repository()
    : m_storage(std::make_unique<SqliteStorageEngine>()) {}

Repository::Repository(std::unique_ptr<StorageEngine> engine)
    : m_storage(std::move(engine)) {}

Repository::~Repository() {
    CloseFile();
}

bool Repository::OpenFile(const std::string& file_path) {
    CloseFile();
    m_connection_string = file_path;

    std::string db_path = file_path;
    if (EapxImporter::IsEapxFile(file_path)) {
        db_path = EapxImporter::ConvertToSqlite(file_path);
        if (db_path.empty()) {
            m_last_error = "Failed to convert MS Access repository (.eapx/.eap) to SQLite: " + file_path;
            return false;
        }
    }

    if (!m_storage->Open(db_path)) {
        m_last_error = "Failed to open repository database: " + db_path;
        return false;
    }

    auto roots = m_storage->LoadRootPackages();
    m_models.Clear();
    for (auto& root : roots) {
        m_models.AddExisting(root);
    }

    return true;
}

void Repository::CloseFile() {
    if (m_storage && m_storage->IsOpen()) {
        m_storage->Close();
    }
    m_models.Clear();
    m_connection_string.clear();
}

bool Repository::IsOpen() const noexcept {
    return m_storage && m_storage->IsOpen();
}

const std::string& Repository::GetConnectionString() const noexcept {
    return m_connection_string;
}

std::shared_ptr<Package> Repository::GetPackageByID(int package_id) {
    if (!m_storage || !m_storage->IsOpen()) return nullptr;
    return m_storage->LoadPackage(package_id);
}

std::shared_ptr<Package> Repository::GetPackageByGuid(const Guid& guid) {
    if (!m_storage || !m_storage->IsOpen()) return nullptr;
    return m_storage->LoadPackageByGuid(guid);
}

std::shared_ptr<Element> Repository::GetElementByID(int element_id) {
    if (!m_storage || !m_storage->IsOpen()) return nullptr;
    return m_storage->LoadElement(element_id);
}

std::shared_ptr<Element> Repository::GetElementByGuid(const Guid& guid) {
    if (!m_storage || !m_storage->IsOpen()) return nullptr;
    return m_storage->LoadElementByGuid(guid);
}

std::shared_ptr<Diagram> Repository::GetDiagramByID(int diagram_id) {
    if (!m_storage || !m_storage->IsOpen()) return nullptr;
    return m_storage->LoadDiagram(diagram_id);
}

std::shared_ptr<Diagram> Repository::GetDiagramByGuid(const Guid& guid) {
    if (!m_storage || !m_storage->IsOpen()) return nullptr;
    return m_storage->LoadDiagramByGuid(guid);
}

std::shared_ptr<Connector> Repository::GetConnectorByID(int connector_id) {
    if (!m_storage || !m_storage->IsOpen()) return nullptr;
    return m_storage->LoadConnector(connector_id);
}

std::shared_ptr<Connector> Repository::GetConnectorByGuid(const Guid& guid) {
    if (!m_storage || !m_storage->IsOpen()) return nullptr;
    return m_storage->LoadConnectorByGuid(guid);
}

bool Repository::CreateRepository(const std::string& file_path) {
    if (!m_storage) return false;
    CloseFile();
    if (!m_storage->CreateNewRepository(file_path)) {
        m_last_error = "Failed to create repository: " + file_path;
        return false;
    }
    return OpenFile(file_path);
}

bool Repository::SavePackage(Package& pkg) {
    if (!IsOpen()) return false;
    return m_storage->SavePackage(pkg);
}

bool Repository::SaveElement(Element& elem) {
    if (!IsOpen()) return false;
    return m_storage->SaveElement(elem);
}

bool Repository::SaveConnector(Connector& conn) {
    if (!IsOpen()) return false;
    return m_storage->SaveConnector(conn);
}

bool Repository::SaveDiagram(Diagram& diag) {
    if (!IsOpen()) return false;
    return m_storage->SaveDiagram(diag);
}

bool Repository::SaveDiagramObject(int diagramId, const DiagramObject& obj) {
    if (!IsOpen()) return false;
    return m_storage->SaveDiagramObject(diagramId, obj);
}

bool Repository::SaveDiagramLink(const DiagramLink& link) {
    if (!IsOpen()) return false;
    return m_storage->SaveDiagramLink(link);
}

bool Repository::RemoveElementFromDiagram(Diagram& diag, int elementId) {
    for (short i = 0; i < diag.GetDiagramObjects().Count(); ++i) {
        auto dobj = diag.GetDiagramObjects().GetAt(i);
        if (dobj && dobj->GetElementID() == elementId) {
            diag.GetDiagramObjects().Delete(i);
            break;
        }
    }
    if (!IsOpen()) return false;
    return m_storage->DeleteDiagramObject(diag.GetDiagramID(), elementId);
}

bool Repository::RemoveConnectorFromDiagram(Diagram& diag, int connectorId) {
    // EA shows every connector between two elements of a diagram unless its
    // t_diagramlinks row is Hidden, so removing means hiding, not deleting the row.
    std::shared_ptr<DiagramLink> link;
    for (short i = 0; i < diag.GetDiagramLinks().Count(); ++i) {
        auto dlink = diag.GetDiagramLinks().GetAt(i);
        if (dlink && dlink->GetConnectorID() == connectorId) {
            link = dlink;
            break;
        }
    }
    if (!link) {
        link = std::make_shared<DiagramLink>(diag.GetDiagramID(), connectorId);
        diag.GetDiagramLinks().AddExisting(link);
    }
    link->SetHidden(true);
    if (!IsOpen()) return false;
    return m_storage->SaveDiagramLink(*link);
}

bool Repository::AddElementToDiagram(Diagram& diag, const std::shared_ptr<DiagramObject>& obj) {
    if (!obj) return false;
    obj->SetDiagramID(diag.GetDiagramID());
    bool present = false;
    for (const auto& existing : diag.GetDiagramObjects()) {
        if (existing && existing->GetElementID() == obj->GetElementID()) {
            present = true;
            break;
        }
    }
    if (!present) diag.GetDiagramObjects().AddExisting(obj);
    if (!IsOpen()) return false;
    return m_storage->SaveDiagramObject(diag.GetDiagramID(), *obj);
}

bool Repository::AddConnectorToDiagram(Diagram& diag, const std::shared_ptr<DiagramLink>& link) {
    if (!link) return false;
    link->SetDiagramID(diag.GetDiagramID());
    bool present = false;
    for (const auto& existing : diag.GetDiagramLinks()) {
        if (existing == link) {
            present = true;
            break;
        }
    }
    if (!present) diag.GetDiagramLinks().AddExisting(link);
    if (!IsOpen()) return false;
    return m_storage->SaveDiagramLink(*link);
}

bool Repository::DeleteConnectorFromDiagram(Diagram& diag, int connectorId) {
    for (short i = 0; i < diag.GetDiagramLinks().Count(); ++i) {
        auto dlink = diag.GetDiagramLinks().GetAt(i);
        if (dlink && dlink->GetConnectorID() == connectorId) {
            diag.GetDiagramLinks().Delete(i);
            break;
        }
    }
    if (!IsOpen()) return false;
    return m_storage->DeleteDiagramLink(diag.GetDiagramID(), connectorId);
}

namespace {
template <typename T, typename IdOf>
void remove_by_id(Collection<T>& items, int id, IdOf idOf) {
    for (short i = 0; i < items.Count(); ++i) {
        auto item = items.GetAt(i);
        if (item && idOf(*item) == id) {
            items.Delete(i);
            return;
        }
    }
}

template <typename T>
void add_if_absent(Collection<T>& items, const std::shared_ptr<T>& item) {
    if (std::find(items.begin(), items.end(), item) == items.end()) items.AddExisting(item);
}
} // namespace

std::shared_ptr<Element> Repository::AddElement(Element& elem) {
    if (!IsOpen() || !m_storage->SaveElement(elem)) return nullptr;
    auto live = m_storage->LoadElement(elem.GetElementID());
    if (!live) return nullptr;
    // Packages are cached, so this is the same Package object the Model Explorer shows
    if (auto pkg = GetPackageByID(live->GetPackageID())) add_if_absent(pkg->GetElements(), live);
    return live;
}

std::shared_ptr<Connector> Repository::AddConnector(Connector& conn) {
    if (!IsOpen() || !m_storage->SaveConnector(conn)) return nullptr;
    auto live = m_storage->LoadConnector(conn.GetConnectorID());
    if (!live) return nullptr;
    for (int endId : {live->GetClientID(), live->GetSupplierID()}) {
        if (auto end = GetElementByID(endId)) add_if_absent(end->GetConnectors(), live);
    }
    return live;
}

std::shared_ptr<Diagram> Repository::CreateDiagram(int packageId, const std::string& name, DiagramType type,
                                                  const std::string& stereotype) {
    if (!IsOpen() || name.empty()) return nullptr;
    auto pkg = GetPackageByID(packageId);
    if (!pkg) return nullptr;

    Diagram diag;
    diag.SetPackageID(packageId);
    diag.SetName(name);
    diag.SetType(type);
    diag.SetStereotype(stereotype);
    diag.SetDiagramGUID(utils::generate_guid());
    if (!m_storage->SaveDiagram(diag)) {
        m_last_error = "Failed to save diagram: " + name;
        return nullptr;
    }

    // Packages are cached, so this is the Package object the Model Explorer shows
    auto live = m_storage->LoadDiagram(diag.GetDiagramID());
    if (live) add_if_absent(pkg->GetDiagrams(), live);
    return live;
}

std::shared_ptr<Package> Repository::CreatePackage(int parentPackageId, const std::string& name,
                                                  const std::string& notes) {
    if (!IsOpen() || name.empty()) return nullptr;
    auto parent = GetPackageByID(parentPackageId);
    if (!parent) return nullptr;

    Package pkg;
    pkg.SetParentID(parentPackageId);
    pkg.SetName(name);
    pkg.SetNotes(notes);
    pkg.SetPackageGUID(utils::generate_guid());
    if (!m_storage->SavePackage(pkg)) {
        m_last_error = "Failed to save package: " + name;
        return nullptr;
    }
    // EA shows a package through its t_object row (same GUID, PDATA1 = Package_ID)
    Element node;
    node.SetType(ObjectType::Package);
    node.SetName(name);
    node.SetNotes(notes);
    node.SetPackageID(parentPackageId);
    node.SetElementGUID(pkg.GetPackageGUID());
    auto element = AddElement(node);
    if (!element || !m_storage->Execute("UPDATE t_object SET PDATA1 = '" + std::to_string(pkg.GetPackageID()) +
                                        "' WHERE Object_ID = " + std::to_string(element->GetElementID()) + ";")) {
        m_last_error = "Failed to save the package element: " + name;
        return nullptr;
    }
    auto live = m_storage->LoadPackage(pkg.GetPackageID());
    if (live) add_if_absent(parent->GetPackages(), live);
    return live;
}

bool Repository::DeleteElement(int elementId) {
    if (!IsOpen()) return false;
    if (auto elem = GetElementByID(elementId)) {
        if (auto pkg = GetPackageByID(elem->GetPackageID())) {
            remove_by_id(pkg->GetElements(), elementId, [](const Element& e) { return e.GetElementID(); });
        }
    }
    return m_storage->DeleteElement(elementId);
}

bool Repository::DeleteConnector(int connectorId) {
    if (!IsOpen()) return false;
    if (auto conn = GetConnectorByID(connectorId)) {
        for (int endId : {conn->GetClientID(), conn->GetSupplierID()}) {
            if (auto end = GetElementByID(endId)) {
                remove_by_id(end->GetConnectors(), connectorId, [](const Connector& c) { return c.GetConnectorID(); });
            }
        }
    }
    return m_storage->DeleteConnector(connectorId);
}

std::string Repository::SQLQuery(const std::string& sql) {
    if (!m_storage || !m_storage->IsOpen()) return "<EADATA><Error>Repository not open</Error></EADATA>";
    return m_storage->SQLQuery(sql);
}

bool Repository::Execute(const std::string& sql) {
    if (!m_storage || !m_storage->IsOpen()) return false;
    return m_storage->Execute(sql);
}

std::shared_ptr<Package> Repository::CreateModel(const std::string& name) {
    auto model = std::make_shared<Package>();
    model->SetName(name);
    model->SetParentID(0);
    model->SetPackageGUID(utils::generate_guid());
    model->SetIsModel(true);

    if (m_storage && m_storage->IsOpen()) {
        m_storage->SavePackage(*model);
    }
    m_models.AddExisting(model);
    return model;
}

RepositoryCounts Repository::GetCounts() {
    RepositoryCounts counts;
    if (!m_storage || !m_storage->IsOpen()) return counts;

    auto count_query = [this](const std::string& table) -> size_t {
        std::string xml = m_storage->SQLQuery("SELECT count(*) as c FROM " + table + ";");
        auto pos = xml.find("<c>");
        if (pos != std::string::npos) {
            auto end = xml.find("</c>", pos);
            if (end != std::string::npos) {
                try {
                    return std::stoull(xml.substr(pos + 3, end - pos - 3));
                } catch (...) {}
            }
        }
        return 0;
    };

    counts.packages = count_query("t_package");
    counts.elements = count_query("t_object");
    counts.diagrams = count_query("t_diagram");
    counts.connectors = count_query("t_connector");
    counts.attributes = count_query("t_attribute");
    counts.operations = count_query("t_operation");

    return counts;
}

std::string Repository::GetLibraryVersion() const {
    return "1.0.2";
}

} // namespace ea
