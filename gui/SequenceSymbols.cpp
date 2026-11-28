#include "SequenceSymbols.hpp"
#include "DarkTheme.hpp"
#include <QFontMetricsF>
#include <QPainter>
#include <QPolygonF>

namespace sequence_symbols {

namespace {

QColor bodyColor() { return ea::gui::m3().surfaceContainer; }
QColor borderColor() { return ea::gui::m3().surfaceHighest; }
QColor accentColor() { return ea::gui::m3().primary; }
QColor textColor() { return ea::gui::m3().onSurface; }
QColor lifelineColor() { return ea::gui::m3().onSurfaceMuted; }
QColor activationColor() { return ea::gui::m3().surfaceHighest; }

QPen borderPen(bool selected, double width = 1.4) {
    return QPen(selected ? accentColor() : borderColor(), selected ? width + 1.0 : width);
}

// Pentagon label in the top-left corner of a frame; returns its height
double drawFrameTag(QPainter* p, const QRectF& rect, const QString& text, bool selected) {
    QFont f = ea::gui::M3Typography::labelMedium();
    f.setWeight(QFont::DemiBold);
    p->setFont(f);
    const QFontMetricsF fm(f);
    const double w = fm.horizontalAdvance(text) + 18, h = fm.height() + 6, cut = 7;
    QPolygonF tag;
    tag << rect.topLeft() << QPointF(rect.left() + w, rect.top()) << QPointF(rect.left() + w, rect.top() + h - cut)
        << QPointF(rect.left() + w - cut, rect.top() + h) << QPointF(rect.left(), rect.top() + h);
    p->setPen(borderPen(selected));
    p->setBrush(bodyColor());
    p->drawPolygon(tag);
    p->setPen(accentColor());
    p->drawText(QRectF(rect.left() + 6, rect.top(), w - 12, h), Qt::AlignVCenter | Qt::AlignLeft, text);
    return h;
}

void drawStickFigure(QPainter* p, QPointF top) {
    p->setPen(QPen(accentColor(), 2.0));
    p->setBrush(bodyColor());
    p->drawEllipse(top + QPointF(0, 8), 7, 7);
    p->drawLine(top + QPointF(0, 15), top + QPointF(0, 32));
    p->drawLine(top + QPointF(-11, 21), top + QPointF(11, 21));
    p->drawLine(top + QPointF(0, 32), top + QPointF(-9, 44));
    p->drawLine(top + QPointF(0, 32), top + QPointF(9, 44));
}

} // namespace

double headHeight(LifelineHead head) {
    return head == LifelineHead::Actor ? ActorHeadHeight : HeadHeight;
}

void paintLifeline(QPainter* p, const QRectF& rect, const QString& title, LifelineHead head,
                   const std::vector<ActivationBar>& activations, bool selected) {
    const double hh = std::min(headHeight(head), rect.height());
    const double cx = rect.center().x();
    QFont f = ea::gui::M3Typography::titleSmall();
    const QFontMetricsF fm(f);
    double textWidth = 0;
    for (const auto& line : title.split('\n')) textWidth = std::max(textWidth, fm.horizontalAdvance(line));
    // the head grows around the lifeline when the title is wider than the symbol (narrow EA lifelines)
    const double headWidth = std::max(rect.width(), textWidth + 12);
    const QRectF headRect(cx - headWidth / 2, rect.top(), headWidth, hh);

    // Dashed lifeline from the head to the bottom of the symbol
    p->setPen(QPen(selected ? accentColor() : lifelineColor(), 1.2, Qt::DashLine));
    p->drawLine(QPointF(cx, headRect.bottom()), QPointF(cx, rect.bottom()));

    // Execution specifications, nested ones shifted to the right
    p->setPen(borderPen(selected, 1.0));
    p->setBrush(activationColor());
    for (const auto& a : activations) {
        const double top = std::max(a.top, headRect.bottom());
        const double bottom = std::min(a.bottom, rect.bottom());
        if (bottom <= top) continue;
        const double x = cx - ActivationWidth / 2 + a.depth * ActivationNestOffset;
        p->drawRect(QRectF(x, top, ActivationWidth, bottom - top));
    }

    p->setFont(f);
    if (head == LifelineHead::Actor) {
        drawStickFigure(p, QPointF(cx, headRect.top() + 2));
        p->setPen(textColor());
        p->drawText(QRectF(headRect.left(), headRect.top() + 48, headRect.width(), 20), Qt::AlignCenter, title);
        return;
    }
    p->setPen(borderPen(selected));
    p->setBrush(bodyColor());
    p->drawRoundedRect(headRect, 4, 4);
    p->setPen(textColor());
    p->drawText(headRect.adjusted(4, 2, -4, -2), Qt::AlignCenter, title);
}

void paintCombinedFragment(QPainter* p, const QRectF& rect, const QString& op, const QString& guard, bool selected) {
    p->setPen(borderPen(selected));
    p->setBrush(ea::gui::withAlpha(ea::gui::m3().primary, 12)); // almost transparent: lifelines stay visible
    p->drawRect(rect);
    const double tagHeight = drawFrameTag(p, rect, op, selected);
    if (guard.isEmpty()) return;
    p->setFont(ea::gui::M3Typography::labelMedium());
    p->setPen(textColor());
    p->drawText(QRectF(rect.left() + 8, rect.top() + tagHeight + 2, rect.width() - 16, 18), Qt::AlignLeft | Qt::AlignVCenter,
                QString("[%1]").arg(guard));
}

void paintInteractionUse(QPainter* p, const QRectF& rect, const QString& name, bool selected) {
    p->setPen(borderPen(selected));
    p->setBrush(bodyColor());
    p->drawRect(rect);
    drawFrameTag(p, rect, "ref", selected);
    p->setFont(ea::gui::M3Typography::titleSmall());
    p->setPen(textColor());
    p->drawText(rect.adjusted(8, 8, -8, -4), Qt::AlignCenter | Qt::TextWordWrap, name);
}

} // namespace sequence_symbols
