#pragma once

#include <QGraphicsObject>
#include <memory>
#include <vector>
#include "ea/model/DiagramObject.hpp"
#include "ea/model/Element.hpp"
#include "ea/repository/Repository.hpp"

class DiagramConnectorItem;

class DiagramElementItem : public QGraphicsObject {
    Q_OBJECT
public:
    DiagramElementItem(std::shared_ptr<ea::DiagramObject> dobj,
                       std::shared_ptr<ea::Element> elem,
                       ea::Repository& repo,
                       QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

    std::shared_ptr<ea::DiagramObject> getDiagramObject() const { return m_dobj; }
    std::shared_ptr<ea::Element> getElement() const { return m_elem; }
    int getElementId() const;

    void addConnector(DiagramConnectorItem* conn);
    void removeConnector(DiagramConnectorItem* conn);

    QPointF getCenter() const;
    QRectF getRect() const { return m_rect; }
    void setRect(const QRectF& r);

    void updateCoordinatesFromScene();

signals:
    void elementClicked(int elementId);
    void elementDoubleClicked(int elementId);
    void positionModified();
    void removeRequested(int elementId);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    QRectF resizeGripRect() const;

    std::shared_ptr<ea::DiagramObject> m_dobj;
    std::shared_ptr<ea::Element> m_elem;
    ea::Repository& m_repo;
    QRectF m_rect;
    std::vector<DiagramConnectorItem*> m_connectors;

    bool m_isResizing{false};
    QPointF m_resizeStartPos;
    QSizeF m_resizeStartSize;
};
