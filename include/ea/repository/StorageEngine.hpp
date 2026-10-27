#pragma once

#include <string>
#include <memory>
#include <vector>
#include "ea/core/Types.hpp"

namespace ea {

class Package;
class Element;
class Diagram;
class DiagramObject;
class Connector;

class StorageEngine {
public:
    virtual ~StorageEngine() = default;

    virtual bool Open(const std::string& path) = 0;
    virtual void Close() = 0;
    virtual bool IsOpen() const noexcept = 0;
    virtual const std::string& GetPath() const noexcept = 0;

    // Loading model entities
    virtual std::vector<std::shared_ptr<Package>> LoadRootPackages() = 0;
    virtual std::shared_ptr<Package> LoadPackage(int package_id) = 0;
    virtual std::shared_ptr<Package> LoadPackageByGuid(const Guid& guid) = 0;

    virtual std::shared_ptr<Element> LoadElement(int element_id) = 0;
    virtual std::shared_ptr<Element> LoadElementByGuid(const Guid& guid) = 0;

    virtual std::shared_ptr<Diagram> LoadDiagram(int diagram_id) = 0;
    virtual std::shared_ptr<Diagram> LoadDiagramByGuid(const Guid& guid) = 0;

    virtual std::shared_ptr<Connector> LoadConnector(int connector_id) = 0;
    virtual std::shared_ptr<Connector> LoadConnectorByGuid(const Guid& guid) = 0;

    // Saving entities
    virtual bool SavePackage(Package& pkg) = 0;
    virtual bool SaveElement(Element& elem) = 0;
    virtual bool SaveConnector(Connector& conn) = 0;
    virtual bool SaveDiagram(Diagram& diag) = 0;
    virtual bool SaveDiagramObject(int diagramId, const DiagramObject& obj) = 0;

    // Direct SQL execution matching EA COM API
    virtual std::string SQLQuery(const std::string& sql) = 0;
    virtual bool Execute(const std::string& sql) = 0;

    // Create a new blank/initialized repository
    virtual bool CreateNewRepository(const std::string& path) = 0;
};

} // namespace ea
