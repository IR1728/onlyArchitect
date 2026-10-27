#include "ea/repository/Repository.hpp"
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
    return "17.2.1721.15"; // EA 17.2 compatible
}

} // namespace ea
