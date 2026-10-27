#include "ea/model/Diagram.hpp"
#include "ea/core/Utils.hpp"

namespace ea {

Diagram::Diagram() {
    m_guid = utils::generate_guid();

    m_objects = Collection<DiagramObject>(
        [this](const std::string& /*name*/, const std::string& /*type*/) {
            auto obj = std::make_shared<DiagramObject>();
            obj->SetDiagramID(m_id);
            return obj;
        }
    );

    m_links = Collection<DiagramLink>(
        [this](const std::string& /*name*/, const std::string& /*type*/) {
            auto link = std::make_shared<DiagramLink>();
            link->SetDiagramID(m_id);
            return link;
        }
    );
}

} // namespace ea
