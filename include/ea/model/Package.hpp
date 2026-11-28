#pragma once

#include <string>
#include <memory>
#include <cstdint>
#include "ea/core/Types.hpp"
#include "ea/core/Collection.hpp"
#include "ea/model/Element.hpp"
#include "ea/model/Diagram.hpp"
#include "ea/model/Connector.hpp"

namespace ea {

class Package {
public:
    Package();
    ~Package();

    int GetPackageID() const noexcept { return m_id; }
    void SetPackageID(int id) noexcept { m_id = id; }

    int GetParentID() const noexcept { return m_parent_id; }
    void SetParentID(int id) noexcept { m_parent_id = id; }

    const Guid& GetPackageGUID() const noexcept { return m_guid; }
    void SetPackageGUID(Guid guid) { m_guid = std::move(guid); }

    const std::string& GetName() const noexcept { return m_name; }
    void SetName(std::string name) { m_name = std::move(name); }

    const std::string& GetNotes() const noexcept { return m_notes; }
    void SetNotes(std::string notes) { m_notes = std::move(notes); }

    const std::string& GetVersion() const noexcept { return m_version; }
    void SetVersion(std::string ver) { m_version = std::move(ver); }

    const std::string& GetUMLVersion() const noexcept { return m_uml_version; }
    void SetUMLVersion(std::string ver) { m_uml_version = std::move(ver); }

    bool IsModel() const noexcept { return m_is_model; }
    void SetIsModel(bool m) noexcept { m_is_model = m; }

    // Sub-collections
    Collection<Package>& GetPackages() noexcept { return m_packages; }
    const Collection<Package>& GetPackages() const noexcept { return m_packages; }

    Collection<Element>& GetElements() noexcept { return m_elements; }
    const Collection<Element>& GetElements() const noexcept { return m_elements; }

    Collection<Diagram>& GetDiagrams() noexcept { return m_diagrams; }
    const Collection<Diagram>& GetDiagrams() const noexcept { return m_diagrams; }

    Collection<Connector>& GetConnectors() noexcept { return m_connectors; }
    const Collection<Connector>& GetConnectors() const noexcept { return m_connectors; }

    bool Update() { return true; }

private:
    int m_id{0};
    int m_parent_id{0};
    Guid m_guid;
    std::string m_name;
    std::string m_notes;
    std::string m_version{"1.0"};
    std::string m_uml_version{"2.5"};
    bool m_is_model{false};

    Collection<Package> m_packages;
    Collection<Element> m_elements;
    Collection<Diagram> m_diagrams;
    Collection<Connector> m_connectors;
};

} // namespace ea
