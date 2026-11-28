#include "DiagramConnectorItem.hpp"
#include "DarkTheme.hpp"
#include "M3Icons.hpp"
#include "DiagramElementItem.hpp"
#include <QPainter>
#include <QPen>
#include <QMenu>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QStyleOptionGraphicsItem>
#include <QFontMetrics>
#include <cmath>
#include <numbers>
#include "SequenceSymbols.hpp"
#include "ea/diagram/SequenceModel.hpp"

DiagramConnectorItem::DiagramConnectorItem(std::shared_ptr<ea::DiagramLink> dlink,
                                           std::shared_ptr<ea::Connector> conn,
                                           DiagramElementItem* startItem,
                                           DiagramElementItem* endItem,
                                           QGraphicsItem* parent)
    : QGraphicsObject(parent), m_dlink(dlink), m_conn(conn),
      m_startItem(startItem), m_endItem(endItem) {

    // Crisp z-order above background, containers, and element shadows, so arrowheads & labels are fully visible
    setZValue(5);
    setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges);

    if (m_startItem) m_startItem->addConnector(this);
    if (m_endItem) m_endItem->addConnector(this);

    updatePosition();
}

DiagramConnectorItem::~DiagramConnectorItem() {
    if (m_startItem) m_startItem->removeConnector(this);
    if (m_endItem) m_endItem->removeConnector(this);
}

void DiagramConnectorItem::endpointDestroyed(DiagramElementItem* item) {
    if (m_startItem == item) m_startItem = nullptr;
    if (m_endItem == item) m_endItem = nullptr;
}

int DiagramConnectorItem::getConnectorId() const {
    if (m_conn) return m_conn->GetConnectorID();
    if (m_dlink) return m_dlink->GetConnectorID();
    return 0;
}

QRectF DiagramConnectorItem::boundingRect() const {
    return shape().boundingRect().adjusted(-25, -25, 50, 50);
}

QPainterPath DiagramConnectorItem::shape() const {
    QPainterPathStroker stroker;
    stroker.setWidth(10.0); // 10px clickable hit-test zone
    return stroker.createStroke(m_path);
}

QPointF DiagramConnectorItem::calculateIntersection(const QRectF& rect, const QPointF& target) const {
    QPointF center = rect.center();
    double dx = target.x() - center.x();
    double dy = target.y() - center.y();

    if (std::abs(dx) < 1e-4 && std::abs(dy) < 1e-4) return center;

    double t_min = 1.0;

    // Right edge
    if (dx > 0) {
        double t = (rect.right() - center.x()) / dx;
        if (t >= 0 && t < t_min) {
            double y = center.y() + t * dy;
            if (y >= rect.top() - 1.0 && y <= rect.bottom() + 1.0) t_min = t;
        }
    } else if (dx < 0) { // Left edge
        double t = (rect.left() - center.x()) / dx;
        if (t >= 0 && t < t_min) {
            double y = center.y() + t * dy;
            if (y >= rect.top() - 1.0 && y <= rect.bottom() + 1.0) t_min = t;
        }
    }

    // Bottom edge
    if (dy > 0) {
        double t = (rect.bottom() - center.y()) / dy;
        if (t >= 0 && t < t_min) {
            double x = center.x() + t * dx;
            if (x >= rect.left() - 1.0 && x <= rect.right() + 1.0) t_min = t;
        }
    } else if (dy < 0) { // Top edge
        double t = (rect.top() - center.y()) / dy;
        if (t >= 0 && t < t_min) {
            double x = center.x() + t * dx;
            if (x >= rect.left() - 1.0 && x <= rect.right() + 1.0) t_min = t;
        }
    }

    return center + t_min * QPointF(dx, dy);
}

QPointF DiagramConnectorItem::calculateAnchorBorderPoint(const QRectF& rect, const QPointF& anchor,
                                                         const QPointF& target, bool snapOrthogonal) const {
    const QPointF clamped(std::clamp(anchor.x(), rect.left() + 1.0, rect.right() - 1.0),
                          std::clamp(anchor.y(), rect.top() + 1.0, rect.bottom() - 1.0));

    // When routing to/from an EA waypoint, project orthogonally onto the box edge so 90° segments stay razor-straight.
    if (snapOrthogonal) {
        const bool targetInX = (target.x() >= rect.left() - 2.0 && target.x() <= rect.right() + 2.0);
        const bool targetInY = (target.y() >= rect.top() - 2.0 && target.y() <= rect.bottom() + 2.0);
        if (targetInX && !targetInY) {
            const double x = std::clamp(target.x(), rect.left() + 2.0, rect.right() - 2.0);
            const double y = (target.y() < rect.center().y()) ? rect.top() : rect.bottom();
            return QPointF(x, y);
        }
        if (targetInY && !targetInX) {
            const double y = std::clamp(target.y(), rect.top() + 2.0, rect.bottom() - 2.0);
            const double x = (target.x() < rect.center().x()) ? rect.left() : rect.right();
            return QPointF(x, y);
        }
    }

    const double dx = target.x() - clamped.x();
    const double dy = target.y() - clamped.y();
    if (std::abs(dx) < 1e-4 && std::abs(dy) < 1e-4) return clamped;

    double t_min = 1.0;
    if (dx > 0) {
        double t = (rect.right() - clamped.x()) / dx;
        double y = clamped.y() + t * dy;
        if (t >= 0 && t < t_min && y >= rect.top() - 1.0 && y <= rect.bottom() + 1.0) t_min = t;
    } else if (dx < 0) {
        double t = (rect.left() - clamped.x()) / dx;
        double y = clamped.y() + t * dy;
        if (t >= 0 && t < t_min && y >= rect.top() - 1.0 && y <= rect.bottom() + 1.0) t_min = t;
    }
    if (dy > 0) {
        double t = (rect.bottom() - clamped.y()) / dy;
        double x = clamped.x() + t * dx;
        if (t >= 0 && t < t_min && x >= rect.left() - 1.0 && x <= rect.right() + 1.0) t_min = t;
    } else if (dy < 0) {
        double t = (rect.top() - clamped.y()) / dy;
        double x = clamped.x() + t * dx;
        if (t >= 0 && t < t_min && x >= rect.left() - 1.0 && x <= rect.right() + 1.0) t_min = t;
    }
    return clamped + t_min * QPointF(dx, dy);
}

QPolygonF DiagramConnectorItem::makeArrowHead(const QPointF& tip, const QPointF& from, double size) const {
    const double dx = tip.x() - from.x();
    const double dy = tip.y() - from.y();
    const double angle = std::atan2(dy, dx);
    QPointF p1 = tip - QPointF(std::cos(angle - std::numbers::pi / 6.0) * size, std::sin(angle - std::numbers::pi / 6.0) * size);
    QPointF p2 = tip - QPointF(std::cos(angle + std::numbers::pi / 6.0) * size, std::sin(angle + std::numbers::pi / 6.0) * size);
    QPolygonF arrow;
    arrow << tip << p1 << p2;
    return arrow;
}

void DiagramConnectorItem::buildArrowHead(const QPointF& tip, const QPointF& from, double size) {
    m_endArrow = makeArrowHead(tip, from, size);
}

void DiagramConnectorItem::buildDiamond(const QPointF& tip, const QPointF& from, double length, double width) {
    double dx = from.x() - tip.x();
    double dy = from.y() - tip.y();
    double len = std::hypot(dx, dy);
    if (len < 1e-4) return;
    double ux = dx / len;
    double uy = dy / len;
    double nx = -uy;
    double ny = ux;

    QPointF pTip = tip;
    QPointF pMid1 = tip + QPointF(ux * (length / 2.0) + nx * (width / 2.0),
                                  uy * (length / 2.0) + ny * (width / 2.0));
    QPointF pBase = tip + QPointF(ux * length, uy * length);
    QPointF pMid2 = tip + QPointF(ux * (length / 2.0) - nx * (width / 2.0),
                                  uy * (length / 2.0) - ny * (width / 2.0));

    m_diamond.clear();
    m_diamond << pTip << pMid1 << pBase << pMid2;
}

bool DiagramConnectorItem::isSequenceMessage() const {
    return m_conn && m_conn->GetType() == ea::ConnectorType::Message && m_startItem && m_endItem &&
           m_startItem->symbolStyle() == DiagramElementItem::SymbolStyle::Lifeline &&
           m_endItem->symbolStyle() == DiagramElementItem::SymbolStyle::Lifeline;
}

void DiagramConnectorItem::buildMessagePath() {
    namespace ss = sequence_symbols;
    constexpr double SelfLoopWidth = 40.0, SelfLoopHeight = 20.0, BarHalf = ss::ActivationWidth / 2;
    const double y = ea::sequence::message_y(*m_conn) + m_dragOffset;
    const double sx = m_startItem->pos().x() + m_startItem->getRect().width() / 2;
    const double ex = m_endItem->pos().x() + m_endItem->getRect().width() / 2;

    prepareGeometryChange();
    m_path = QPainterPath();
    QPointF tip, from;
    if (m_startItem == m_endItem) { // self message: a loop on the right of the lifeline
        m_path.moveTo(sx + BarHalf, y);
        m_path.lineTo(sx + SelfLoopWidth, y);
        m_path.lineTo(sx + SelfLoopWidth, y + SelfLoopHeight);
        tip = QPointF(sx + BarHalf, y + SelfLoopHeight);
        from = QPointF(sx + SelfLoopWidth, y + SelfLoopHeight);
        m_path.lineTo(tip);
    } else {
        const double dir = ex >= sx ? 1.0 : -1.0;
        double endX = ex - dir * BarHalf;
        if (ea::sequence::message_sort(*m_conn) == ea::sequence::MessageSort::Create) {
            // a create message points at the head of the created lifeline
            endX = dir > 0 ? m_endItem->pos().x() : m_endItem->pos().x() + m_endItem->getRect().width();
        }
        from = QPointF(sx + dir * BarHalf, y);
        tip = QPointF(endX, y);
        m_path.moveTo(from);
        m_path.lineTo(tip);
    }
    buildArrowHead(tip, from, 11.0);
    update();
}

void DiagramConnectorItem::paintMessage(QPainter* painter, bool selected) {
    using ea::sequence::MessageSort;
    const auto sort = ea::sequence::message_sort(*m_conn);
    const QColor color = selected ? ea::gui::m3().primary : ea::gui::m3().onSurfaceVariant;
    const bool dashed = sort == MessageSort::Reply || sort == MessageSort::Create;
    painter->setPen(QPen(color, selected ? 2.2 : 1.5, dashed ? Qt::DashLine : Qt::SolidLine));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(m_path);

    if (m_endArrow.size() >= 3) {
        painter->setPen(QPen(color, 1.5));
        if (sort == MessageSort::Synchronous || sort == MessageSort::Delete) { // filled head: synchronous call
            painter->setBrush(color);
            painter->drawPolygon(m_endArrow);
        } else { // open head: asynchronous, reply, create
            painter->drawLine(m_endArrow[1], m_endArrow[0]);
            painter->drawLine(m_endArrow[0], m_endArrow[2]);
        }
    }
    if (sort == MessageSort::Delete) { // destruction occurrence
        const QPointF end = m_path.currentPosition();
        painter->setPen(QPen(ea::gui::m3().error, 2.2));
        painter->drawLine(end + QPointF(-9, -9), end + QPointF(9, 9));
        painter->drawLine(end + QPointF(-9, 9), end + QPointF(9, -9));
    }

    const QString label = QString::fromStdString(ea::sequence::message_label(*m_conn));
    if (label.isEmpty()) return;
    QFont font = ea::gui::M3Typography::labelMedium();
    painter->setFont(font);
    const QFontMetricsF fm(font);
    const QRectF bounds = m_path.boundingRect();
    const QRectF textRect(bounds.center().x() - fm.horizontalAdvance(label) / 2 - 2, bounds.top() - fm.height() - 3,
                          fm.horizontalAdvance(label) + 4, fm.height());
    painter->setPen(ea::gui::m3().onSurface);
    painter->drawText(textRect, Qt::AlignCenter, label);
}

void DiagramConnectorItem::updatePosition() {
    if (!m_startItem || !m_endItem) return;
    if (isSequenceMessage()) {
        buildMessagePath();
        return;
    }

    // Use pure element bounds (excluding drop shadows) for exact border contacts
    QRectF startRect(m_startItem->pos().x(), m_startItem->pos().y(),
                    m_startItem->getRect().width(), m_startItem->getRect().height());
    QRectF endRect(m_endItem->pos().x(), m_endItem->pos().y(),
                  m_endItem->getRect().width(), m_endItem->getRect().height());

    std::vector<QPointF> points;

    // 1. Waypoints & EA anchor offsets (SX, SY, EX, EY from t_diagramlinks.Geometry)
    std::vector<ea::Point> waypoints;
    ea::DiagramLink::AnchorOffsets anchors;
    if (m_dlink) {
        waypoints = m_dlink->ParseWaypoints();
        anchors = m_dlink->ParseGeometryAnchors();
    }

    // EA Y-axis points UP, screen Y points DOWN -> screen offset is (+SX, -SY) / (+EX, -EY)
    const QPointF startAnchor = startRect.center() + QPointF(anchors.sx, -anchors.sy);
    const QPointF endAnchor = endRect.center() + QPointF(anchors.ex, -anchors.ey);

    if (!waypoints.empty()) {
        QPointF firstWp(waypoints.front().x, -waypoints.front().y);
        QPointF p1 = calculateAnchorBorderPoint(startRect, startAnchor, firstWp, true);
        points.push_back(p1);

        for (const auto& wp : waypoints) {
            points.push_back(QPointF(wp.x, -wp.y));
        }

        QPointF lastWp(waypoints.back().x, -waypoints.back().y);
        QPointF p2 = calculateAnchorBorderPoint(endRect, endAnchor, lastWp, true);
        points.push_back(p2);
    } else {
        // Direct route: snap horizontally/vertically when boxes overlap along an axis (EA orthogonal default)
        QPointF p1 = calculateAnchorBorderPoint(startRect, startAnchor, endAnchor, false);
        QPointF p2 = calculateAnchorBorderPoint(endRect, endAnchor, startAnchor, false);

        const double overlapLeft = std::max(startRect.left(), endRect.left()) + 6.0;
        const double overlapRight = std::min(startRect.right(), endRect.right()) - 6.0;
        const double overlapTop = std::max(startRect.top(), endRect.top()) + 6.0;
        const double overlapBottom = std::min(startRect.bottom(), endRect.bottom()) - 6.0;

        if (!anchors.hasSx && !anchors.hasEx && overlapLeft < overlapRight &&
            (startRect.bottom() < endRect.top() || endRect.bottom() < startRect.top())) {
            const double x = std::clamp((startAnchor.x() + endAnchor.x()) / 2.0, overlapLeft, overlapRight);
            p1 = QPointF(x, startRect.center().y() < endRect.center().y() ? startRect.bottom() : startRect.top());
            p2 = QPointF(x, startRect.center().y() < endRect.center().y() ? endRect.top() : endRect.bottom());
        } else if (!anchors.hasSy && !anchors.hasEy && overlapTop < overlapBottom &&
                   (startRect.right() < endRect.left() || endRect.right() < startRect.left())) {
            const double y = std::clamp((startAnchor.y() + endAnchor.y()) / 2.0, overlapTop, overlapBottom);
            p1 = QPointF(startRect.center().x() < endRect.center().x() ? startRect.right() : startRect.left(), y);
            p2 = QPointF(startRect.center().x() < endRect.center().x() ? endRect.left() : endRect.right(), y);
        }
        points.push_back(p1);
        points.push_back(p2);
    }

    m_points = points;
    prepareGeometryChange();
    m_path = QPainterPath();
    if (!points.empty()) {
        m_path.moveTo(points.front());
        for (size_t i = 1; i < points.size(); ++i) {
            m_path.lineTo(points[i]);
        }
    }

    // Build markers (arrowhead / diamond) at BOTH endpoints
    m_startArrow.clear();
    m_endArrow.clear();
    m_diamond.clear();
    if (points.size() >= 2) {
        QPointF pStart = points.front();
        QPointF pStartNext = points[1];
        QPointF pEnd = points.back();
        QPointF pEndPrev = points[points.size() - 2];

        m_startArrow = makeArrowHead(pStart, pStartNext, 12.0);
        m_endArrow = makeArrowHead(pEnd, pEndPrev, 12.0);
        buildDiamond(pStart, pStartNext, 14.0, 9.0);
    }

    update();
}

void DiagramConnectorItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* /*widget*/) {
    if (!m_startItem || !m_endItem || m_path.isEmpty()) return;

    painter->setRenderHint(QPainter::Antialiasing);

    bool isSelected = option->state & QStyle::State_Selected;
    if (isSequenceMessage()) {
        paintMessage(painter, isSelected);
        return;
    }

    // Paleta de conectores para tema oscuro moderno
    // Nord Frost: secondary at rest, primary when selected (M3 selection)
    QColor lineColor = isSelected ? ea::gui::m3().primary : ea::gui::m3().secondary;
    double lineWidth = isSelected ? 2.0 : 1.5;
    Qt::PenStyle lineStyle = Qt::SolidLine;

    ea::ConnectorType type = m_conn ? m_conn->GetType() : ea::ConnectorType::Association;

    if (type == ea::ConnectorType::Realization || type == ea::ConnectorType::Dependency ||
        type == ea::ConnectorType::Usage || type == ea::ConnectorType::Abstraction ||
        type == ea::ConnectorType::UseCaseLink || type == ea::ConnectorType::PackageImport ||
        type == ea::ConnectorType::PackageMerge) {
        lineStyle = Qt::DashLine;
    } else if (type == ea::ConnectorType::NoteLink) {
        lineStyle = Qt::DotLine;
    }

    // 1. Draw connector stroke
    QPen linePen(lineColor, lineWidth, lineStyle);
    linePen.setCapStyle(Qt::RoundCap);
    linePen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(linePen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(m_path);

    // 2. Draw End Markers based on UML type
    if (type == ea::ConnectorType::Generalization || type == ea::ConnectorType::Realization) {
        // Hollow closed triangle
        painter->setPen(QPen(lineColor, 1.5));
        painter->setBrush(ea::gui::m3().surfaceLow); // Fondo oscuro
        painter->drawPolygon(m_endArrow);
    } else if (type == ea::ConnectorType::Composition) {
        // Solid diamond at start
        painter->setPen(QPen(lineColor, 1.5));
        painter->setBrush(lineColor);
        painter->drawPolygon(m_diamond);
    } else if (type == ea::ConnectorType::Aggregation) {
        // Hollow diamond at start
        painter->setPen(QPen(lineColor, 1.5));
        painter->setBrush(ea::gui::m3().surfaceLow);
        painter->drawPolygon(m_diamond);
    } else if (type == ea::ConnectorType::Dependency || type == ea::ConnectorType::Usage ||
               type == ea::ConnectorType::Abstraction || type == ea::ConnectorType::ControlFlow ||
               type == ea::ConnectorType::ObjectFlow || type == ea::ConnectorType::Transition ||
               type == ea::ConnectorType::UseCaseLink || type == ea::ConnectorType::CommunicationMessage ||
               type == ea::ConnectorType::PackageImport || type == ea::ConnectorType::PackageMerge) {
        // Open arrow at target
        painter->setPen(QPen(lineColor, 1.8));
        if (m_endArrow.size() >= 3) {
            painter->drawLine(m_endArrow[1], m_endArrow[0]);
            painter->drawLine(m_endArrow[0], m_endArrow[2]);
        }
    } else if (type == ea::ConnectorType::Message) {
        // Message arrow
        painter->setPen(QPen(lineColor, 1.6));
        painter->setBrush(lineColor);
        painter->drawPolygon(m_endArrow);
    } else if (type == ea::ConnectorType::Association) {
        const std::string dir = m_conn ? m_conn->GetDirection() : "Unspecified";
        const bool navToDest = (dir == "Source -> Destination" || dir == "Bi-Directional" ||
                                (m_conn && m_conn->IsDestNavigable()));
        const bool navToSource = (dir == "Destination -> Source" || dir == "Bi-Directional" ||
                                  (m_conn && m_conn->IsSourceNavigable()));
        painter->setPen(QPen(lineColor, 1.8));
        if (navToDest && m_endArrow.size() >= 3) {
            painter->drawLine(m_endArrow[1], m_endArrow[0]);
            painter->drawLine(m_endArrow[0], m_endArrow[2]);
        }
        if (navToSource && m_startArrow.size() >= 3) {
            painter->drawLine(m_startArrow[1], m_startArrow[0]);
            painter->drawLine(m_startArrow[0], m_startArrow[2]);
        }
    } else if (type == ea::ConnectorType::Assembly) {
        // Lollipop + Socket de Ensamble UML (proto2.jpeg)
        QPointF mid = m_path.pointAtPercent(0.5);
        painter->setPen(QPen(lineColor, 1.6));
        painter->setBrush(ea::gui::m3().primary); // Bolita cian
        painter->drawEllipse(mid, 5, 5);

        // Zócalo semicircular
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(lineColor, 2.0));
        painter->drawArc(QRectF(mid.x() - 9, mid.y() - 9, 18, 18), 45 * 16, 180 * 16);
    } else if (type == ea::ConnectorType::Delegate) {
        // Delegación con línea discontinua y flecha abierta
        painter->setPen(QPen(lineColor, 1.6, Qt::DashLine));
        if (m_endArrow.size() >= 3) {
            painter->drawLine(m_endArrow[1], m_endArrow[0]);
            painter->drawLine(m_endArrow[0], m_endArrow[2]);
        }
    }

    // 3. Labels: Name, Stereotype, Cardinalities / Roles
    QFont labelFont = ea::gui::M3Typography::labelMedium();
    painter->setFont(labelFont);

    // Midpoint Name, Stereotype & UML 2.5.1 Reading-Direction Triangle (► / ◄ / ▲ / ▼)
    if (m_conn && (!m_conn->GetName().empty() || !m_conn->GetStereotype().empty())) {
        const QPointF mid = m_path.pointAtPercent(0.5);

        QString labelText;
        if (!m_conn->GetStereotype().empty()) {
            labelText += QString("«%1» ").arg(QString::fromStdString(m_conn->GetStereotype()));
        }
        if (!m_conn->GetName().empty()) {
            labelText += QString::fromStdString(m_conn->GetName());
        }

        // For named Associations (UML 2.5.1 §11.5.4 / EA convention), show solid reading-direction triangle
        QString readArrow;
        bool arrowBefore = false;
        if (type == ea::ConnectorType::Association && !m_conn->GetName().empty() && m_points.size() >= 2) {
            QPointF dirVec = m_points.back() - m_points.front();
            if (m_conn->GetDirection() == "Destination -> Source") dirVec = -dirVec;
            if (std::abs(dirVec.x()) >= std::abs(dirVec.y())) {
                if (dirVec.x() >= 0) { readArrow = QStringLiteral(" ▶"); arrowBefore = false; }
                else { readArrow = QStringLiteral("◀ "); arrowBefore = true; }
            } else {
                if (dirVec.y() >= 0) { readArrow = QStringLiteral(" ▼"); arrowBefore = false; }
                else { readArrow = QStringLiteral("▲ "); arrowBefore = true; }
            }
        }
        const QString displayText = arrowBefore ? (readArrow + labelText) : (labelText + readArrow);

        QFontMetrics fm(labelFont);
        QRect textRect = fm.boundingRect(displayText);
        QRectF bgRect(mid.x() - textRect.width() / 2.0 - 6,
                      mid.y() - textRect.height() / 2.0 - 3,
                      textRect.width() + 12,
                      textRect.height() + 6);

        painter->setPen(QPen(ea::gui::m3().surfaceHighest, 1.0));
        painter->setBrush(ea::gui::withAlpha(ea::gui::m3().surfaceLow, 232));
        painter->drawRoundedRect(bgRect, 4, 4);

        painter->setPen(ea::gui::m3().onSurface);
        painter->drawText(bgRect, Qt::AlignCenter, displayText);
    }

    auto drawEndLabel = [&](const QPointF& endPt, const QPointF& inwardPt, const QString& text) {
        if (text.isEmpty()) return;
        const QPointF diff = inwardPt - endPt;
        const double len = std::hypot(diff.x(), diff.y());
        if (len < 1e-3) return;
        const QPointF u = diff / len;
        const QPointF n(-u.y(), u.x()); // perpendicular offset so label sits clear of the line
        const double along = std::min(20.0, len * 0.25);
        const QPointF pos = endPt + u * along + n * 12.0;

        QFont mf = labelFont;
        mf.setPointSizeF(8.0);
        painter->setFont(mf);
        const QFontMetricsF fm(mf);
        const qreal tw = fm.horizontalAdvance(text);
        const qreal th = fm.height();
        const QRectF r(pos.x() - tw / 2.0 - 2.0, pos.y() - th / 2.0, tw + 4.0, th);
        painter->fillRect(r, ea::gui::withAlpha(ea::gui::m3().surfaceLowest, 195));
        painter->setPen(ea::gui::m3().primary);
        painter->drawText(r, Qt::AlignCenter, text);
    };

    // Source Role & Cardinality Label (clean UML notation without [ ] guard brackets)
    if (m_conn && m_points.size() >= 2 &&
        (!m_conn->GetClientRole().empty() || !m_conn->GetClientMultiplicity().empty())) {
        QString sText;
        if (!m_conn->GetClientRole().empty()) sText += QString("+%1 ").arg(QString::fromStdString(m_conn->GetClientRole()));
        if (!m_conn->GetClientMultiplicity().empty()) sText += QString::fromStdString(m_conn->GetClientMultiplicity());
        drawEndLabel(m_points.front(), m_points[1], sText.trimmed());
    }

    // Dest Role & Cardinality Label
    if (m_conn && m_points.size() >= 2 &&
        (!m_conn->GetSupplierRole().empty() || !m_conn->GetSupplierMultiplicity().empty())) {
        QString dText;
        if (!m_conn->GetSupplierRole().empty()) dText += QString("+%1 ").arg(QString::fromStdString(m_conn->GetSupplierRole()));
        if (!m_conn->GetSupplierMultiplicity().empty()) dText += QString::fromStdString(m_conn->GetSupplierMultiplicity());
        drawEndLabel(m_points.back(), m_points[m_points.size() - 2], dText.trimmed());
    }
}

void DiagramConnectorItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit connectorClicked(getConnectorId());
        if (isSequenceMessage()) {
            m_draggingMessage = true;
            m_pressY = event->scenePos().y();
            m_dragOffset = 0.0;
        }
    }
    QGraphicsObject::mousePressEvent(event);
}

void DiagramConnectorItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    if (!m_draggingMessage) {
        QGraphicsObject::mouseMoveEvent(event);
        return;
    }
    m_dragOffset = event->scenePos().y() - m_pressY; // messages only move vertically
    buildMessagePath();
}

void DiagramConnectorItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    if (m_draggingMessage) {
        m_draggingMessage = false;
        const double offset = m_dragOffset;
        m_dragOffset = 0.0;
        constexpr double MinDrag = 3.0; // a click is not a move
        if (std::abs(offset) >= MinDrag) {
            emit messageDragged(getConnectorId(), ea::sequence::message_y(*m_conn) + static_cast<int>(std::lround(offset)));
        }
        if (m_startItem && m_endItem) buildMessagePath(); // back to the model position (the command moves it)
    }
    QGraphicsObject::mouseReleaseEvent(event);
}

void DiagramConnectorItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    if (m_conn) emit editRequested(getConnectorId());
    event->accept();
}

void DiagramConnectorItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event) {
    QMenu menu;
    const QString editLabel = isSequenceMessage() ? "Editar mensaje..." : "Editar relación / multiplicidades...";
    auto editAct = menu.addAction(ea::gui::M3Icons::icon("edit"), editLabel);
    menu.addSeparator();
    auto delAct = menu.addAction(ea::gui::M3Icons::icon("delete"), "Quitar del diagrama");
    auto chosen = menu.exec(event->screenPos());
    if (chosen == editAct) {
        if (m_conn) emit editRequested(getConnectorId());
    } else if (chosen == delAct) {
        emit removeRequested(getConnectorId());
    }
}
