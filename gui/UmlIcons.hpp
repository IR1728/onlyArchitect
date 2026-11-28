#pragma once

// Material Symbol for each UML element / relationship kind, tinted with the Nord accent of its
// canvas symbol. Shared by the project browser (ModelTreeWidget) and the toolbox so both show
// the same glyph for the same kind.

#include <QColor>
#include <QIcon>
#include <QString>

#include "ea/core/Types.hpp"

namespace ea::gui {

struct UmlGlyph {
    QString symbol; // Material Symbol name
    QColor color;
};

UmlGlyph umlElementGlyph(ObjectType type);
UmlGlyph umlRelationshipGlyph(ConnectorType type);

QIcon umlElementIcon(ObjectType type);
QIcon umlRelationshipIcon(ConnectorType type);

} // namespace ea::gui
