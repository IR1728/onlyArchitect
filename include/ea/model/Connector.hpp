#pragma once

#include <string>
#include <cstdint>
#include "ea/core/Types.hpp"

namespace ea {

class Connector {
public:
    Connector() = default;

    int GetConnectorID() const noexcept { return m_id; }
    void SetConnectorID(int id) noexcept { m_id = id; }

    const Guid& GetConnectorGUID() const noexcept { return m_guid; }
    void SetConnectorGUID(Guid guid) { m_guid = std::move(guid); }

    const std::string& GetName() const noexcept { return m_name; }
    void SetName(std::string name) { m_name = std::move(name); }

    ConnectorType GetType() const noexcept { return m_type; }
    void SetType(ConnectorType type) noexcept { m_type = type; }

    const std::string& GetSubtype() const noexcept { return m_subtype; }
    void SetSubtype(std::string sub) { m_subtype = std::move(sub); }

    const std::string& GetStereotype() const noexcept { return m_stereotype; }
    void SetStereotype(std::string st) { m_stereotype = std::move(st); }

    int GetClientID() const noexcept { return m_client_id; }
    void SetClientID(int id) noexcept { m_client_id = id; }

    int GetSupplierID() const noexcept { return m_supplier_id; }
    void SetSupplierID(int id) noexcept { m_supplier_id = id; }

    // Client/Source end details
    const std::string& GetClientRole() const noexcept { return m_client_role; }
    void SetClientRole(std::string r) { m_client_role = std::move(r); }

    const std::string& GetClientMultiplicity() const noexcept { return m_client_mult; }
    void SetClientMultiplicity(std::string m) { m_client_mult = std::move(m); }

    // Supplier/Target end details
    const std::string& GetSupplierRole() const noexcept { return m_supplier_role; }
    void SetSupplierRole(std::string r) { m_supplier_role = std::move(r); }

    const std::string& GetSupplierMultiplicity() const noexcept { return m_supplier_mult; }
    void SetSupplierMultiplicity(std::string m) { m_supplier_mult = std::move(m); }

    // Direction (e.g., "Source -> Destination", "Bi-Directional", "Unspecified")
    const std::string& GetDirection() const noexcept { return m_direction; }
    void SetDirection(std::string dir) { m_direction = std::move(dir); }

    const std::string& GetNotes() const noexcept { return m_notes; }
    void SetNotes(std::string notes) { m_notes = std::move(notes); }

    bool Update() { return true; }

private:
    int m_id{0};
    Guid m_guid;
    std::string m_name;
    ConnectorType m_type{ConnectorType::Association};
    std::string m_subtype;
    std::string m_stereotype;
    int m_client_id{0};    // Start_Object_ID (Source)
    int m_supplier_id{0};  // End_Object_ID (Target)
    std::string m_client_role;
    std::string m_client_mult;
    std::string m_supplier_role;
    std::string m_supplier_mult;
    std::string m_direction{"Source -> Destination"};
    std::string m_notes;
};

} // namespace ea
