#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "ea/core/Types.hpp"

namespace ea {

struct Parameter {
    std::string name;
    std::string type;
    std::string default_value;
    std::string kind{"in"}; // in, out, inout
    int pos{0};
};

class Method {
public:
    Method() = default;

    int GetMethodID() const noexcept { return m_id; }
    void SetMethodID(int id) noexcept { m_id = id; }

    int GetObjectID() const noexcept { return m_object_id; }
    void SetObjectID(int id) noexcept { m_object_id = id; }

    const std::string& GetName() const noexcept { return m_name; }
    void SetName(std::string name) { m_name = std::move(name); }

    Scope GetVisibility() const noexcept { return m_visibility; }
    void SetVisibility(Scope scope) noexcept { m_visibility = scope; }

    const std::string& GetReturnType() const noexcept { return m_return_type; }
    void SetReturnType(std::string type) { m_return_type = std::move(type); }

    const std::string& GetNotes() const noexcept { return m_notes; }
    void SetNotes(std::string notes) { m_notes = std::move(notes); }

    bool IsStatic() const noexcept { return m_is_static; }
    void SetStatic(bool s) noexcept { m_is_static = s; }

    bool IsAbstract() const noexcept { return m_is_abstract; }
    void SetAbstract(bool a) noexcept { m_is_abstract = a; }

    bool IsConst() const noexcept { return m_is_const; }
    void SetConst(bool c) noexcept { m_is_const = c; }

    const Guid& GetMethodGUID() const noexcept { return m_guid; }
    void SetMethodGUID(Guid guid) { m_guid = std::move(guid); }

    int GetPos() const noexcept { return m_pos; }
    void SetPos(int pos) noexcept { m_pos = pos; }

    const std::vector<Parameter>& GetParameters() const noexcept { return m_parameters; }
    void AddParameter(Parameter p) { m_parameters.push_back(std::move(p)); }

    bool Update() { return true; }

private:
    int m_id{0};
    int m_object_id{0};
    std::string m_name;
    Scope m_visibility{Scope::Public};
    std::string m_return_type{"void"};
    std::string m_notes;
    bool m_is_static{false};
    bool m_is_abstract{false};
    bool m_is_const{false};
    Guid m_guid;
    int m_pos{0};
    std::vector<Parameter> m_parameters;
};

} // namespace ea
