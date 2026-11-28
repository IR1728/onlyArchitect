#pragma once

#include <string>
#include <memory>
#include <vector>
#include "ea/model/Diagram.hpp"
#include "ea/repository/Repository.hpp"
#include "ea/diagram/CoordinateSystem.hpp"

namespace ea {

class DiagramRenderer {
public:
    DiagramRenderer();
    ~DiagramRenderer();

    // Renders the diagram directly to an SVG string or file
    std::string RenderToSVG(const Diagram& diagram, Repository& repo);
    bool RenderToSVGFile(const Diagram& diagram, Repository& repo, const std::string& output_path);

    // Renders the diagram to a PNG image using Cairo
    bool RenderToPNGFile(const Diagram& diagram, Repository& repo, const std::string& output_path);

private:
    void RenderSVGObject(std::ostringstream& svg, const DiagramObject& dobj, Repository& repo, const CoordinateSystem& coords);
    void RenderSVGConnector(std::ostringstream& svg, const DiagramLink& dlink, Repository& repo, const CoordinateSystem& coords);

    void RenderCairoObject(void* cr, const DiagramObject& dobj, Repository& repo, const CoordinateSystem& coords);
    void RenderCairoConnector(void* cr, const DiagramLink& dlink, Repository& repo, const CoordinateSystem& coords);
};

} // namespace ea
