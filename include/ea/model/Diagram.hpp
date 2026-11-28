#pragma once

#include <string>
#include <memory>
#include <cstdint>
#include "ea/core/Types.hpp"
#include "ea/core/Collection.hpp"
#include "ea/model/DiagramObject.hpp"
#include "ea/model/DiagramLink.hpp"

namespace ea {

class Diagram {
public:
    Diagram();

    int GetDiagramID() const noexcept { return m_id; }
    void SetDiagramID(int id) noexcept { m_id = id; }

    int GetPackageID() const noexcept { return m_package_id; }
    void SetPackageID(int id) noexcept { m_package_id = id; }

    int GetParentID() const noexcept { return m_parent_id; }
    void SetParentID(int id) noexcept { m_parent_id = id; }

    const Guid& GetDiagramGUID() const noexcept { return m_guid; }
    void SetDiagramGUID(Guid guid) { m_guid = std::move(guid); }

    const std::string& GetName() const noexcept { return m_name; }
    void SetName(std::string name) { m_name = std::move(name); }

    DiagramType GetType() const noexcept { return m_type; }
    void SetType(DiagramType type) noexcept { m_type = type; }

    const std::string& GetStereotype() const noexcept { return m_stereotype; }
    void SetStereotype(std::string st) { m_stereotype = std::move(st); }

    const std::string& GetVersion() const noexcept { return m_version; }
    void SetVersion(std::string ver) { m_version = std::move(ver); }

    const std::string& GetAuthor() const noexcept { return m_author; }
    void SetAuthor(std::string auth) { m_author = std::move(auth); }

    const std::string& GetNotes() const noexcept { return m_notes; }
    void SetNotes(std::string notes) { m_notes = std::move(notes); }

    int GetCx() const noexcept { return m_cx; }
    void SetCx(int cx) noexcept { m_cx = cx; }

    int GetCy() const noexcept { return m_cy; }
    void SetCy(int cy) noexcept { m_cy = cy; }

    int GetScale() const noexcept { return m_scale; }
    void SetScale(int scale) noexcept { m_scale = scale; }

    // Collections
    Collection<DiagramObject>& GetDiagramObjects() noexcept { return m_objects; }
    const Collection<DiagramObject>& GetDiagramObjects() const noexcept { return m_objects; }

    Collection<DiagramLink>& GetDiagramLinks() noexcept { return m_links; }
    const Collection<DiagramLink>& GetDiagramLinks() const noexcept { return m_links; }

    bool Update() { return true; }

private:
    int m_id{0};
    int m_package_id{0};
    int m_parent_id{0};
    Guid m_guid;
    std::string m_name;
    DiagramType m_type{DiagramType::Logical};
    std::string m_stereotype;
    std::string m_version{"1.0"};
    std::string m_author;
    std::string m_notes;
    int m_cx{1000};
    int m_cy{800};
    int m_scale{100};

    Collection<DiagramObject> m_objects;
    Collection<DiagramLink> m_links;
};

} // namespace ea
