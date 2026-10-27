#pragma once

#include <string>
#include <memory>
#include <cstdint>
#include "ea/core/Types.hpp"
#include "ea/core/Collection.hpp"
#include "ea/model/Attribute.hpp"
#include "ea/model/Method.hpp"
#include "ea/model/Connector.hpp"
#include "ea/model/TaggedValue.hpp"

namespace ea {

class Diagram; // Forward declaration

class Element {
public:
    Element();
    ~Element();

    int GetElementID() const noexcept { return m_id; }
    void SetElementID(int id) noexcept { m_id = id; }

    const Guid& GetElementGUID() const noexcept { return m_guid; }
    void SetElementGUID(Guid guid) { m_guid = std::move(guid); }

    const std::string& GetName() const noexcept { return m_name; }
    void SetName(std::string name) { m_name = std::move(name); }

    ObjectType GetType() const noexcept { return m_type; }
    void SetType(ObjectType type) noexcept { m_type = type; }

    const std::string& GetSubtype() const noexcept { return m_subtype; }
    void SetSubtype(std::string sub) { m_subtype = std::move(sub); }

    const std::string& GetStereotype() const noexcept { return m_stereotype; }
    void SetStereotype(std::string st) { m_stereotype = std::move(st); }

    int GetPackageID() const noexcept { return m_package_id; }
    void SetPackageID(int id) noexcept { m_package_id = id; }

    int GetParentID() const noexcept { return m_parent_id; }
    void SetParentID(int id) noexcept { m_parent_id = id; }

    const std::string& GetNotes() const noexcept { return m_notes; }
    void SetNotes(std::string notes) { m_notes = std::move(notes); }

    const std::string& GetStatus() const noexcept { return m_status; }
    void SetStatus(std::string status) { m_status = std::move(status); }

    const std::string& GetAuthor() const noexcept { return m_author; }
    void SetAuthor(std::string auth) { m_author = std::move(auth); }

    const std::string& GetVersion() const noexcept { return m_version; }
    void SetVersion(std::string ver) { m_version = std::move(ver); }

    const std::string& GetComplexity() const noexcept { return m_complexity; }
    void SetComplexity(std::string comp) { m_complexity = std::move(comp); }

    const std::string& GetGenType() const noexcept { return m_gentype; }
    void SetGenType(std::string gt) { m_gentype = std::move(gt); }

    const std::string& GetGenFile() const noexcept { return m_genfile; }
    void SetGenFile(std::string gf) { m_genfile = std::move(gf); }

    int GetClassifierID() const noexcept { return m_classifier_id; }
    void SetClassifierID(int id) noexcept { m_classifier_id = id; }

    bool IsAbstract() const noexcept { return m_is_abstract; }
    void SetAbstract(bool a) noexcept { m_is_abstract = a; }

    // Collections
    Collection<Attribute>& GetAttributes() noexcept { return m_attributes; }
    const Collection<Attribute>& GetAttributes() const noexcept { return m_attributes; }

    Collection<Method>& GetMethods() noexcept { return m_methods; }
    const Collection<Method>& GetMethods() const noexcept { return m_methods; }

    Collection<Connector>& GetConnectors() noexcept { return m_connectors; }
    const Collection<Connector>& GetConnectors() const noexcept { return m_connectors; }

    Collection<TaggedValue>& GetTaggedValues() noexcept { return m_tagged_values; }
    const Collection<TaggedValue>& GetTaggedValues() const noexcept { return m_tagged_values; }

    Collection<Element>& GetElements() noexcept { return m_elements; }
    const Collection<Element>& GetElements() const noexcept { return m_elements; }

    Collection<Diagram>& GetDiagrams() noexcept;
    const Collection<Diagram>& GetDiagrams() const noexcept;

    bool Update() { return true; }

private:
    int m_id{0};
    Guid m_guid;
    std::string m_name;
    ObjectType m_type{ObjectType::Class};
    std::string m_subtype;
    std::string m_stereotype;
    int m_package_id{0};
    int m_parent_id{0};
    std::string m_notes;
    std::string m_status{"Proposed"};
    std::string m_author;
    std::string m_version{"1.0"};
    std::string m_complexity{"1"};
    std::string m_gentype{"<none>"};
    std::string m_genfile;
    int m_classifier_id{0};
    bool m_is_abstract{false};

    Collection<Attribute> m_attributes;
    Collection<Method> m_methods;
    Collection<Connector> m_connectors;
    Collection<TaggedValue> m_tagged_values;
    Collection<Element> m_elements;
    std::unique_ptr<Collection<Diagram>> m_diagrams;
};

} // namespace ea
