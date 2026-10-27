#include "ea/model/Element.hpp"
#include "ea/model/Diagram.hpp"
#include "ea/core/Utils.hpp"

namespace ea {

Element::Element() {
    m_guid = utils::generate_guid();

    m_attributes = Collection<Attribute>(
        [this](const std::string& name, const std::string& type) {
            auto attr = std::make_shared<Attribute>();
            attr->SetName(name);
            attr->SetType(type.empty() ? "int" : type);
            attr->SetObjectID(m_id);
            attr->SetAttributeGUID(utils::generate_guid());
            return attr;
        }
    );

    m_methods = Collection<Method>(
        [this](const std::string& name, const std::string& return_type) {
            auto meth = std::make_shared<Method>();
            meth->SetName(name);
            meth->SetReturnType(return_type.empty() ? "void" : return_type);
            meth->SetObjectID(m_id);
            meth->SetMethodGUID(utils::generate_guid());
            return meth;
        }
    );

    m_connectors = Collection<Connector>(
        [this](const std::string& name, const std::string& type) {
            auto conn = std::make_shared<Connector>();
            conn->SetName(name);
            conn->SetType(connector_type_from_string(type));
            conn->SetClientID(m_id);
            conn->SetConnectorGUID(utils::generate_guid());
            return conn;
        }
    );

    m_tagged_values = Collection<TaggedValue>(
        [this](const std::string& name, const std::string& val) {
            auto tag = std::make_shared<TaggedValue>();
            tag->SetName(name);
            tag->SetValue(val);
            tag->SetElementID(m_id);
            tag->SetPropertyGUID(utils::generate_guid());
            return tag;
        }
    );

    m_elements = Collection<Element>(
        [this](const std::string& name, const std::string& type) {
            auto elem = std::make_shared<Element>();
            elem->SetName(name);
            elem->SetType(object_type_from_string(type));
            elem->SetParentID(m_id);
            elem->SetPackageID(m_package_id);
            return elem;
        }
    );

    m_diagrams = std::make_unique<Collection<Diagram>>(
        [this](const std::string& name, const std::string& type) {
            auto diag = std::make_shared<Diagram>();
            diag->SetName(name);
            diag->SetType(diagram_type_from_string(type));
            diag->SetParentID(m_id);
            diag->SetPackageID(m_package_id);
            return diag;
        }
    );
}

Element::~Element() = default;

Collection<Diagram>& Element::GetDiagrams() noexcept {
    return *m_diagrams;
}

const Collection<Diagram>& Element::GetDiagrams() const noexcept {
    return *m_diagrams;
}

} // namespace ea
