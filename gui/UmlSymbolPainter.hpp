#pragma once

// Material Design 3 / Nord rendering of standard UML symbols on the canvas
// (diseño/ guide, steps 3-4): 12 px corners, tonal header gradient clipped to the card,
// compartments on the 4 px grid with a dedicated visibility column, Gemini selection glow.
// Pure painting: the item gathers what to draw into SymbolContent.

#include <QPainter>
#include <QRectF>
#include <QString>
#include <vector>
#include "ea/core/Types.hpp"

namespace uml_symbols {

struct FeatureLine {
    ea::Scope visibility{ea::Scope::Public};
    QString signature; // "name" or "name()"
    QString type;      // empty: no " : type"
};

struct SymbolContent {
    ea::ObjectType type{ea::ObjectType::Class};
    int ntype{0};       // EA t_object.NType (e.g., 102=FlowFinal, Event 0=Send/1=Accept)
    QString name;
    QString stereotype; // without guillemets
    QString notes;      // Note text
    std::vector<FeatureLine> attributes;
    std::vector<FeatureLine> operations;
};

// Body, header, compartments and text of the symbol inside `rect`
void paintSymbol(QPainter* painter, const QRectF& rect, const SymbolContent& content);

// Gemini focus ring: aura + diagonal primary -> secondary -> tertiary stroke.
// `hover` in [0, 1] fades the ring in when the pointer is over an unselected symbol.
void paintSelectionGlow(QPainter* painter, const QRectF& rect, ea::ObjectType type, bool selected, qreal hover);

// Round M3 resize handles centred on `centers`
void paintHandles(QPainter* painter, const std::vector<QPointF>& centers);

} // namespace uml_symbols
