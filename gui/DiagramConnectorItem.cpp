#include "DiagramConnectorItem.hpp"
#include "DiagramElementItem.hpp"
#include <QPainter>
#include <QPen>
#include <QMenu>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QStyleOptionGraphicsItem>
#include <QFontMetrics>
#include <cmath>

DiagramConnectorItem::DiagramConnectorItem(std::shared_ptr<ea::DiagramLink> dlink,
                                           std::shared_ptr<ea::Connector> conn,
                                           DiagramElementItem* startItem,
                                           DiagramElementItem* endItem,
                                           QGraphicsItem* parent)
    : QGraphicsObject(parent), m_dlink(dlink), m_conn(conn),
      m_startItem(startItem), m_endItem(endItem) {

    // Crisp z-order above background and element shadows, so arrowheads & labels are fully visible
    setZValue(1);
    setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges);

    if (m_startItem) m_startItem->addConnector(this);
    if (m_endItem) m_endItem->addConnector(this);

    updatePosition();
}

DiagramConnectorItem::~DiagramConnectorItem() {
    if (m_startItem) m_startItem->removeConnector(this);
    if (m_endItem) m_endItem->removeConnector(this);
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

void DiagramConnectorItem::buildArrowHead(const QPointF& tip, const QPointF& from, double size) {
    double dx = tip.x() - from.x();
    double dy = tip.y() - from.y();
    double angle = std::atan2(dy, dx);

    QPointF p1 = tip - QPointF(std::cos(angle - M_PI / 6.0) * size, std::sin(angle - M_PI / 6.0) * size);
    QPointF p2 = tip - QPointF(std::cos(angle + M_PI / 6.0) * size, std::sin(angle + M_PI / 6.0) * size);

    m_endArrow.clear();
    m_endArrow << tip << p1 << p2;
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

void DiagramConnectorItem::updatePosition() {
    if (!m_startItem || !m_endItem) return;

    // Use pure element bounds (excluding drop shadows) for exact border contacts
    QRectF startRect(m_startItem->pos().x(), m_startItem->pos().y(),
                    m_startItem->getRect().width(), m_startItem->getRect().height());
    QRectF endRect(m_endItem->pos().x(), m_endItem->pos().y(),
                  m_endItem->getRect().width(), m_endItem->getRect().height());

    std::vector<QPointF> points;

    // 1. Waypoints from EA diagram link path (if available)
    std::vector<ea::Point> waypoints;
    if (m_dlink) {
        waypoints = m_dlink->ParseWaypoints();
    }

    if (!waypoints.empty()) {
        QPointF firstWp(waypoints.front().x, -waypoints.front().y);
        QPointF p1 = calculateIntersection(startRect, firstWp);
        points.push_back(p1);

        for (const auto& wp : waypoints) {
            points.push_back(QPointF(wp.x, -wp.y));
        }

        QPointF lastWp(waypoints.back().x, -waypoints.back().y);
        QPointF p2 = calculateIntersection(endRect, lastWp);
        points.push_back(p2);
    } else {
        // Direct route: calculate clean intersection between center-to-center ray
        QPointF p1 = calculateIntersection(startRect, endRect.center());
        QPointF p2 = calculateIntersection(endRect, startRect.center());
        points.push_back(p1);
        points.push_back(p2);
    }

    prepareGeometryChange();
    m_path = QPainterPath();
    if (!points.empty()) {
        m_path.moveTo(points.front());
        for (size_t i = 1; i < points.size(); ++i) {
            m_path.lineTo(points[i]);
        }
    }

    // Build markers (arrowhead / diamond) at endpoints
    if (points.size() >= 2) {
        QPointF pStart = points.front();
        QPointF pStartNext = points[1];
        QPointF pEnd = points.back();
        QPointF pEndPrev = points[points.size() - 2];

        buildArrowHead(pEnd, pEndPrev, 13.0);
        buildDiamond(pStart, pStartNext, 14.0, 9.0);
    }

    update();
}

void DiagramConnectorItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* /*widget*/) {
    if (!m_startItem || !m_endItem || m_path.isEmpty()) return;

    painter->setRenderHint(QPainter::Antialiasing);

    bool isSelected = option->state & QStyle::State_Selected;

    // Paleta de conectores para tema oscuro moderno
    QColor lineColor = isSelected ? QColor(56, 189, 248) : QColor(148, 163, 184); // Cian si seleccionado, plata (#94a3b8) por defecto
    double lineWidth = isSelected ? 2.4 : 1.6;
    Qt::PenStyle lineStyle = Qt::SolidLine;

    ea::ConnectorType type = m_conn ? m_conn->GetType() : ea::ConnectorType::Association;

    if (type == ea::ConnectorType::Realization || type == ea::ConnectorType::Dependency) {
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
        painter->setBrush(QColor(30, 34, 45)); // Fondo oscuro
        painter->drawPolygon(m_endArrow);
    } else if (type == ea::ConnectorType::Composition) {
        // Solid diamond at start
        painter->setPen(QPen(lineColor, 1.5));
        painter->setBrush(lineColor);
        painter->drawPolygon(m_diamond);
    } else if (type == ea::ConnectorType::Aggregation) {
        // Hollow diamond at start
        painter->setPen(QPen(lineColor, 1.5));
        painter->setBrush(QColor(30, 34, 45));
        painter->drawPolygon(m_diamond);
    } else if (type == ea::ConnectorType::Dependency) {
        // Open arrow at end
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
        std::string dir = m_conn ? m_conn->GetDirection() : "Unspecified";
        if (dir == "Source -> Destination" || dir == "Bi-Directional") {
            painter->setPen(QPen(lineColor, 1.8));
            if (m_endArrow.size() >= 3) {
                painter->drawLine(m_endArrow[1], m_endArrow[0]);
                painter->drawLine(m_endArrow[0], m_endArrow[2]);
            }
        }
    } else if (type == ea::ConnectorType::Assembly) {
        // Lollipop + Socket de Ensamble UML (proto2.jpeg)
        QPointF mid = m_path.pointAtPercent(0.5);
        painter->setPen(QPen(lineColor, 1.6));
        painter->setBrush(QColor(56, 189, 248)); // Bolita cian
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
    QFont labelFont("sans-serif", 8, QFont::Normal);
    painter->setFont(labelFont);

    // Midpoint Name & Stereotype Label
    if (m_conn && (!m_conn->GetName().empty() || !m_conn->GetStereotype().empty())) {
        QPointF mid = m_path.pointAtPercent(0.5);

        QString labelText;
        if (!m_conn->GetStereotype().empty()) {
            labelText += QString("«%1» ").arg(QString::fromStdString(m_conn->GetStereotype()));
        }
        if (!m_conn->GetName().empty()) {
            labelText += QString::fromStdString(m_conn->GetName());
        }

        QFontMetrics fm(labelFont);
        QRect textRect = fm.boundingRect(labelText);
        QRectF bgRect(mid.x() - textRect.width() / 2.0 - 5,
                      mid.y() - textRect.height() / 2.0 - 3,
                      textRect.width() + 10,
                      textRect.height() + 6);

        // Píldora oscura moderna para lectura clara
        painter->setPen(QPen(QColor(56, 66, 82), 1.0));
        painter->setBrush(QColor(30, 34, 45, 230));
        painter->drawRoundedRect(bgRect, 4, 4);

        painter->setPen(QColor(241, 245, 249));
        painter->drawText(bgRect, Qt::AlignCenter, labelText);
    }

    // Source Role & Cardinality Label
    if (m_conn && (!m_conn->GetClientRole().empty() || !m_conn->GetClientMultiplicity().empty())) {
        QPointF pStart = m_path.pointAtPercent(0.12);
        QString sText;
        if (!m_conn->GetClientRole().empty()) sText += QString("+%1 ").arg(QString::fromStdString(m_conn->GetClientRole()));
        if (!m_conn->GetClientMultiplicity().empty()) sText += QString("[%1]").arg(QString::fromStdString(m_conn->GetClientMultiplicity()));

        painter->setPen(QColor(56, 189, 248)); // Cian roles
        painter->drawText(pStart + QPointF(6, -4), sText);
    }

    // Dest Role & Cardinality Label
    if (m_conn && (!m_conn->GetSupplierRole().empty() || !m_conn->GetSupplierMultiplicity().empty())) {
        QPointF pEnd = m_path.pointAtPercent(0.88);
        QString dText;
        if (!m_conn->GetSupplierRole().empty()) dText += QString("+%1 ").arg(QString::fromStdString(m_conn->GetSupplierRole()));
        if (!m_conn->GetSupplierMultiplicity().empty()) dText += QString("[%1]").arg(QString::fromStdString(m_conn->GetSupplierMultiplicity()));

        painter->setPen(QColor(56, 189, 248)); // Cian roles
        painter->drawText(pEnd + QPointF(6, -4), dText);
    }
}

void DiagramConnectorItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit connectorClicked(getConnectorId());
    }
    QGraphicsObject::mousePressEvent(event);
}

void DiagramConnectorItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event) {
    QMenu menu;
    auto delAct = menu.addAction("🗑️ Delete Connector");
    auto chosen = menu.exec(event->screenPos());
    if (chosen == delAct) {
        emit removeRequested(getConnectorId());
    }
}
