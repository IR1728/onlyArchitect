#pragma once

#include <string>
#include <cstdint>
#include "ea/core/Types.hpp"

namespace ea {

class Attribute {
public:
    Attribute() = default;

    int GetAttributeID() const noexcept { return m_id; }
    void SetAttributeID(int id) noexcept { m_id = id; }

    int GetObjectID() const noexcept { return m_object_id; }
    void SetObjectID(int id) noexcept { m_object_id = id; }

    const std::string& GetName() const noexcept { return m_name; }
    void SetName(std::string name) { m_name = std::move(name); }

    Scope GetVisibility() const noexcept { return m_visibility; }
    void SetVisibility(Scope scope) noexcept { m_visibility = scope; }

    const std::string& GetType() const noexcept { return m_type; }
    void SetType(std::string type) { m_type = std::move(type); }

    const std::string& GetDefault() const noexcept { return m_default; }
    void SetDefault(std::string def) { m_default = std::move(def); }

    const std::string& GetNotes() const noexcept { return m_notes; }
    void SetNotes(std::string notes) { m_notes = std::move(notes); }

    bool IsStatic() const noexcept { return m_is_static; }
    void SetStatic(bool s) noexcept { m_is_static = s; }

    bool IsConst() const noexcept { return m_is_const; }
    void SetConst(bool c) noexcept { m_is_const = c; }

    const Guid& GetAttributeGUID() const noexcept { return m_guid; }
    void SetAttributeGUID(Guid guid) { m_guid = std::move(guid); }

    int GetPos() const noexcept { return m_pos; }
    void SetPos(int pos) noexcept { m_pos = pos; }

    bool Update() { return true; }

private:
    int m_id{0};
    int m_object_id{0};
    std::string m_name;
    Scope m_visibility{Scope::Public};
    std::string m_type;
    std::string m_default;
    std::string m_notes;
    bool m_is_static{false};
    bool m_is_const{false};
    Guid m_guid;
    int m_pos{0};
};

} // namespace ea
