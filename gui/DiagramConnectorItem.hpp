#pragma once

#include <QGraphicsObject>
#include <QPainterPath>
#include <memory>
#include <vector>
#include "ea/model/DiagramLink.hpp"
#include "ea/model/Connector.hpp"
#include "DiagramElementItem.hpp"

class DiagramConnectorItem : public QGraphicsObject, public ConnectorEndpointObserver {
    Q_OBJECT
public:
    enum { Type = UserType + 2 };
    int type() const override { return Type; }

    DiagramConnectorItem(std::shared_ptr<ea::DiagramLink> dlink,
                         std::shared_ptr<ea::Connector> conn,
                         DiagramElementItem* startItem,
                         DiagramElementItem* endItem,
                         QGraphicsItem* parent = nullptr);
    ~DiagramConnectorItem() override;

    void updatePosition() override;
    void endpointDestroyed(DiagramElementItem* item) override;

    std::shared_ptr<ea::DiagramLink> getDiagramLink() const { return m_dlink; }
    std::shared_ptr<ea::Connector> getConnector() const { return m_conn; }
    int getConnectorId() const;

    DiagramElementItem* startItem() const { return m_startItem; }
    DiagramElementItem* endItem() const { return m_endItem; }

    QPainterPath path() const { return m_path; }
    // An interaction message between two lifelines: drawn horizontally at its own height
    bool isSequenceMessage() const;
    QRectF boundingRect() const override;
    QPainterPath shape() const override;

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

signals:
    void connectorClicked(int connectorId);
    void editRequested(int connectorId);
    void removeRequested(int connectorId);
    // The user dragged a message to another height (scene y)
    void messageDragged(int connectorId, int y);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

private:
    QPointF calculateIntersection(const QRectF& rect, const QPointF& target) const;
    QPointF calculateAnchorBorderPoint(const QRectF& rect, const QPointF& anchor, const QPointF& target, bool snapOrthogonal) const;
    QPolygonF makeArrowHead(const QPointF& tip, const QPointF& from, double size = 12.0) const;
    void buildArrowHead(const QPointF& tip, const QPointF& from, double size = 12.0);
    void buildDiamond(const QPointF& tip, const QPointF& from, double length = 14.0, double width = 8.0);
    void buildMessagePath();
    void paintMessage(QPainter* painter, bool selected);

    std::shared_ptr<ea::DiagramLink> m_dlink;
    std::shared_ptr<ea::Connector> m_conn;
    DiagramElementItem* m_startItem{nullptr};
    DiagramElementItem* m_endItem{nullptr};

    QPainterPath m_path;
    QPolygonF m_startArrow;
    QPolygonF m_endArrow;
    QPolygonF m_diamond;
    std::vector<QPointF> m_points;
    double m_dragOffset{0.0}; // vertical preview while a message is dragged
    double m_pressY{0.0};
    bool m_draggingMessage{false};
};
