#pragma once

#include <string>
#include <cstdint>
#include "ea/core/Types.hpp"

namespace ea {

class TaggedValue {
public:
    TaggedValue() = default;
    TaggedValue(int id, int element_id, std::string name, std::string value, std::string notes = "")
        : m_id(id), m_element_id(element_id), m_name(std::move(name)), m_value(std::move(value)), m_notes(std::move(notes)) {}

    int GetPropertyID() const noexcept { return m_id; }
    void SetPropertyID(int id) noexcept { m_id = id; }

    int GetElementID() const noexcept { return m_element_id; }
    void SetElementID(int id) noexcept { m_element_id = id; }

    const std::string& GetName() const noexcept { return m_name; }
    void SetName(std::string name) { m_name = std::move(name); }

    const std::string& GetValue() const noexcept { return m_value; }
    void SetValue(std::string val) { m_value = std::move(val); }

    const std::string& GetNotes() const noexcept { return m_notes; }
    void SetNotes(std::string notes) { m_notes = std::move(notes); }

    const Guid& GetPropertyGUID() const noexcept { return m_guid; }
    void SetPropertyGUID(Guid guid) { m_guid = std::move(guid); }

    bool Update() { return true; }

private:
    int m_id{0};
    int m_element_id{0};
    std::string m_name;
    std::string m_value;
    std::string m_notes;
    Guid m_guid;
};

} // namespace ea
