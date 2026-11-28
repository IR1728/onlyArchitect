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
#include "ea/editor/ModelGateway.hpp"

namespace ea {

struct RepositoryCounts {
    size_t packages{0};
    size_t elements{0};
    size_t diagrams{0};
    size_t connectors{0};
    size_t attributes{0};
    size_t operations{0};
};

// Implements the editor engine's ModelGateway port (the persistence layer depends on the engine, not vice versa)
class Repository : public editor::ModelGateway {
public:
    Repository();
    explicit Repository(std::unique_ptr<StorageEngine> engine);
    ~Repository() override;

    // Central EA COM API methods
    bool OpenFile(const std::string& file_path);
    bool CreateRepository(const std::string& file_path);
    void CloseFile();
    bool IsOpen() const noexcept;
    const std::string& GetConnectionString() const noexcept;

    Collection<Package>& GetModels() noexcept { return m_models; }
    const Collection<Package>& GetModels() const noexcept { return m_models; }

    std::shared_ptr<Package> GetPackageByID(int package_id);
    std::shared_ptr<Package> GetPackageByGuid(const Guid& guid);

    std::shared_ptr<Element> GetElementByID(int element_id) override;
    std::shared_ptr<Element> GetElementByGuid(const Guid& guid);

    std::shared_ptr<Diagram> GetDiagramByID(int diagram_id);
    std::shared_ptr<Diagram> GetDiagramByGuid(const Guid& guid);

    std::shared_ptr<Connector> GetConnectorByID(int connector_id) override;
    std::shared_ptr<Connector> GetConnectorByGuid(const Guid& guid);

    // High-level Use Cases for Persistence (Clean Architecture Boundary)
    bool SavePackage(Package& pkg);
    bool SaveElement(Element& elem) override;
    bool SaveConnector(Connector& conn) override;
    bool SaveDiagram(Diagram& diag);
    bool SaveDiagramObject(int diagramId, const DiagramObject& obj);
    bool SaveDiagramLink(const DiagramLink& link) override;
    bool RemoveElementFromDiagram(Diagram& diag, int elementId) override;
    bool RemoveConnectorFromDiagram(Diagram& diag, int connectorId) override;

    // Inverse operations, used by the editor's undo/redo commands
    bool AddElementToDiagram(Diagram& diag, const std::shared_ptr<DiagramObject>& obj) override;
    bool AddConnectorToDiagram(Diagram& diag, const std::shared_ptr<DiagramLink>& link) override;
    bool DeleteConnectorFromDiagram(Diagram& diag, int connectorId) override; // drops the t_diagramlinks row
    // Persist a new (or re-created) element/relationship and register it in the in-memory model
    // (owning package, element connector lists). Returns the live instance GetElementByID() returns.
    std::shared_ptr<Element> AddElement(Element& elem) override;
    std::shared_ptr<Connector> AddConnector(Connector& conn) override;
    bool DeleteElement(int elementId) override;     // removes the element from the model
    bool DeleteConnector(int connectorId) override; // removes the relationship from the model

    // Creates an empty diagram inside a package and registers it in the in-memory package tree.
    // Returns nullptr if the package does not exist or the name is empty.
    std::shared_ptr<Diagram> CreateDiagram(int packageId, const std::string& name, DiagramType type,
                                           const std::string& stereotype = "");
    // Creates a child package (t_package plus the t_object row EA keeps for every package, with
    // PDATA1 = package id) and registers it in the in-memory tree. nullptr if the parent is missing.
    std::shared_ptr<Package> CreatePackage(int parentPackageId, const std::string& name, const std::string& notes = "");

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
