#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <sqlite3.h>
#include "ea/repository/StorageEngine.hpp"
#include "ea/model/DiagramObject.hpp"

namespace ea {

class SqliteStorageEngine : public StorageEngine {
public:
    SqliteStorageEngine();
    ~SqliteStorageEngine() override;

    bool Open(const std::string& path) override;
    void Close() override;
    bool IsOpen() const noexcept override { return m_db != nullptr; }
    const std::string& GetPath() const noexcept override { return m_path; }

    std::vector<std::shared_ptr<Package>> LoadRootPackages() override;
    std::shared_ptr<Package> LoadPackage(int package_id) override;
    std::shared_ptr<Package> LoadPackageByGuid(const Guid& guid) override;

    std::shared_ptr<Element> LoadElement(int element_id) override;
    std::shared_ptr<Element> LoadElementByGuid(const Guid& guid) override;

    std::shared_ptr<Diagram> LoadDiagram(int diagram_id) override;
    std::shared_ptr<Diagram> LoadDiagramByGuid(const Guid& guid) override;

    std::shared_ptr<Connector> LoadConnector(int connector_id) override;
    std::shared_ptr<Connector> LoadConnectorByGuid(const Guid& guid) override;

    bool SavePackage(Package& pkg) override;
    bool SaveElement(Element& elem) override;
    bool SaveConnector(Connector& conn) override;
    bool SaveDiagram(Diagram& diag) override;
    bool SaveDiagramObject(int diagramId, const DiagramObject& obj) override;

    std::string SQLQuery(const std::string& sql) override;
    bool Execute(const std::string& sql) override;

    bool CreateNewRepository(const std::string& path) override;

private:
    void LoadPackageChildren(Package& pkg);
    void LoadElementDetails(Element& elem);
    void LoadDiagramDetails(Diagram& diag);

    sqlite3* m_db{nullptr};
    std::string m_path;

    // Cache to prevent duplicate loads & cycles
    std::unordered_map<int, std::shared_ptr<Package>> m_package_cache;
    std::unordered_map<int, std::shared_ptr<Element>> m_element_cache;
    std::unordered_map<int, std::shared_ptr<Diagram>> m_diagram_cache;
    std::unordered_map<int, std::shared_ptr<Connector>> m_connector_cache;
};

} // namespace ea
