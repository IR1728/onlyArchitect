#include "ea/model/Package.hpp"
#include "ea/core/Utils.hpp"

namespace ea {

Package::Package() {
    m_guid = utils::generate_guid();

    m_packages = Collection<Package>(
        [this](const std::string& name, const std::string& /*type*/) {
            auto pkg = std::make_shared<Package>();
            pkg->SetName(name);
            pkg->SetParentID(m_id);
            return pkg;
        }
    );

    m_elements = Collection<Element>(
        [this](const std::string& name, const std::string& type) {
            auto elem = std::make_shared<Element>();
            elem->SetName(name);
            elem->SetType(object_type_from_string(type));
            elem->SetPackageID(m_id);
            return elem;
        }
    );

    m_diagrams = Collection<Diagram>(
        [this](const std::string& name, const std::string& type) {
            auto diag = std::make_shared<Diagram>();
            diag->SetName(name);
            diag->SetType(diagram_type_from_string(type));
            diag->SetPackageID(m_id);
            return diag;
        }
    );

    m_connectors = Collection<Connector>(
        [](const std::string& name, const std::string& type) {
            auto conn = std::make_shared<Connector>();
            conn->SetName(name);
            conn->SetType(connector_type_from_string(type));
            conn->SetConnectorGUID(utils::generate_guid());
            return conn;
        }
    );
}

Package::~Package() = default;

} // namespace ea
