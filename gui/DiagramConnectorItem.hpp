#pragma once

#include <QGraphicsObject>
#include <QPainterPath>
#include <memory>
#include <vector>
#include "ea/model/DiagramLink.hpp"
#include "ea/model/Connector.hpp"

class DiagramElementItem;

class DiagramConnectorItem : public QGraphicsObject {
    Q_OBJECT
public:
    DiagramConnectorItem(std::shared_ptr<ea::DiagramLink> dlink,
                         std::shared_ptr<ea::Connector> conn,
                         DiagramElementItem* startItem,
                         DiagramElementItem* endItem,
                         QGraphicsItem* parent = nullptr);
    ~DiagramConnectorItem() override;

    void updatePosition();

    std::shared_ptr<ea::DiagramLink> getDiagramLink() const { return m_dlink; }
    std::shared_ptr<ea::Connector> getConnector() const { return m_conn; }
    int getConnectorId() const;

    DiagramElementItem* startItem() const { return m_startItem; }
    DiagramElementItem* endItem() const { return m_endItem; }

    QPainterPath path() const { return m_path; }
    QRectF boundingRect() const override;
    QPainterPath shape() const override;

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

signals:
    void connectorClicked(int connectorId);
    void removeRequested(int connectorId);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

private:
    QPointF calculateIntersection(const QRectF& rect, const QPointF& target) const;
    void buildArrowHead(const QPointF& tip, const QPointF& from, double size = 12.0);
    void buildDiamond(const QPointF& tip, const QPointF& from, double length = 14.0, double width = 8.0);

    std::shared_ptr<ea::DiagramLink> m_dlink;
    std::shared_ptr<ea::Connector> m_conn;
    DiagramElementItem* m_startItem{nullptr};
    DiagramElementItem* m_endItem{nullptr};

    QPainterPath m_path;
    QPolygonF m_endArrow;
    QPolygonF m_diamond;
};
