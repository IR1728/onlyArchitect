#pragma once

#include <string>
#include <memory>
#include <vector>
#include "ea/core/Types.hpp"
#include "ea/core/Collection.hpp"
#include "ea/model/Package.hpp"
#include "ea/model/Element.hpp"
#include "ea/model/Diagram.hpp"
#include "ea/model/Connector.hpp"
#include "ea/repository/StorageEngine.hpp"

namespace ea {

struct RepositoryCounts {
    size_t packages{0};
    size_t elements{0};
    size_t diagrams{0};
    size_t connectors{0};
    size_t attributes{0};
    size_t operations{0};
};

class Repository {
public:
    Repository();
    explicit Repository(std::unique_ptr<StorageEngine> engine);
    ~Repository();

    // Central EA COM API methods
    bool OpenFile(const std::string& file_path);
    void CloseFile();
    bool IsOpen() const noexcept;
    const std::string& GetConnectionString() const noexcept;

    Collection<Package>& GetModels() noexcept { return m_models; }
    const Collection<Package>& GetModels() const noexcept { return m_models; }

    std::shared_ptr<Package> GetPackageByID(int package_id);
    std::shared_ptr<Package> GetPackageByGuid(const Guid& guid);

    std::shared_ptr<Element> GetElementByID(int element_id);
    std::shared_ptr<Element> GetElementByGuid(const Guid& guid);

    std::shared_ptr<Diagram> GetDiagramByID(int diagram_id);
    std::shared_ptr<Diagram> GetDiagramByGuid(const Guid& guid);

    std::shared_ptr<Connector> GetConnectorByID(int connector_id);
    std::shared_ptr<Connector> GetConnectorByGuid(const Guid& guid);

    std::string SQLQuery(const std::string& sql);
    bool Execute(const std::string& sql);

    std::shared_ptr<Package> CreateModel(const std::string& name);

    // Repository diagnostics & metadata
    RepositoryCounts GetCounts();
    std::string GetLibraryVersion() const;
    const std::string& GetLastError() const noexcept { return m_last_error; }

    StorageEngine* GetStorageEngine() noexcept { return m_storage.get(); }

private:
    std::unique_ptr<StorageEngine> m_storage;
    Collection<Package> m_models;
    std::string m_connection_string;
    std::string m_last_error;
};

} // namespace ea
