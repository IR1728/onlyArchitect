#pragma once

#include <QGraphicsObject>
#include <memory>
#include <vector>
#include "ea/model/DiagramObject.hpp"
#include "ea/model/Element.hpp"
#include "ea/repository/Repository.hpp"
#include "ea/editor/ElementGeometry.hpp"
#include "ea/editor/CanvasGeometry.hpp"
#include "UmlSymbolPainter.hpp"

// Abstract observer interface (DIP / ADP Chapter 14 of documentProgramming3.md)
class ConnectorEndpointObserver {
public:
    virtual ~ConnectorEndpointObserver() = default;
    virtual void updatePosition() = 0;
    // Called from the element's destructor; the observer must drop its pointer to it
    virtual void endpointDestroyed(class DiagramElementItem* item) = 0;
};

class DiagramElementItem : public QGraphicsObject {
    Q_OBJECT
public:
    enum { Type = UserType + 1 };
    // Size given to a new symbol, and fallback when stored geometry is corrupt (<= 4 px)
    static constexpr int DefaultWidth = 170;
    static constexpr int DefaultHeight = 100;
    static constexpr int MinStoredSize = 4;
    int type() const override { return Type; }

    DiagramElementItem(std::shared_ptr<ea::DiagramObject> dobj,
                       std::shared_ptr<ea::Element> elem,
                       ea::Repository& repo,
                       QGraphicsItem* parent = nullptr);
    ~DiagramElementItem() override;

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

    std::shared_ptr<ea::DiagramObject> getDiagramObject() const { return m_dobj; }
    std::shared_ptr<ea::Element> getElement() const { return m_elem; }
    int getElementId() const;

    void addConnector(ConnectorEndpointObserver* conn);
    void removeConnector(ConnectorEndpointObserver* conn);

    QPointF getCenter() const;
    // Notation on interaction diagrams: lifeline (head + dashed line) or frame (fragment, ref)
    enum class SymbolStyle { Standard, Lifeline, Frame };
    void setSymbolStyle(SymbolStyle style);
    SymbolStyle symbolStyle() const { return m_style; }
    // Execution specifications in scene y (top, bottom, nesting depth); only drawn on lifelines
    struct Activation { double top; double bottom; int depth; };
    void setActivations(std::vector<Activation> activations);
    const std::vector<Activation>& activations() const { return m_activations; }

    // Resize handles (8, shown while selected), in item coordinates
    static constexpr double HandleSize = 8.0;
    ea::editor::Handle handleAt(const QPointF& itemPos) const;
    ea::editor::Box sceneBox() const;

    QRectF getRect() const { return m_rect; }
    void setRect(const QRectF& r);

    void updateCoordinatesFromScene();

    // What is on screen (may differ from a DiagramObject the constructor normalised)
    ea::editor::ElementGeometry geometry() const;
    void applyGeometry(const ea::editor::ElementGeometry& g);

signals:
    void elementClicked(int elementId);
    void elementDoubleClicked(int elementId);
    void positionModified();
    void removeRequested(int elementId);
    // Model edits are performed by the view so that they go through the undo stack
    void editRequested(int elementId);
    void addAttributeRequested(int elementId);
    void addOperationRequested(int elementId);
    void refineToSequenceRequested(int elementId);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:

    std::shared_ptr<ea::DiagramObject> m_dobj;
    std::shared_ptr<ea::Element> m_elem;
    ea::Repository& m_repo;
    QRectF m_rect;
    SymbolStyle m_style{SymbolStyle::Standard};
    std::vector<Activation> m_activations;
    bool paintInteractionSymbol(QPainter* painter, bool selected);
    std::vector<ConnectorEndpointObserver*> m_connectors;

    ea::editor::Handle m_activeHandle{ea::editor::Handle::None};
    QPointF m_resizeStartPos;
    ea::editor::Box m_resizeStartBox;
    bool m_userMoving{false}; // a mouse drag is moving the symbol: positions snap to the grid
    void paintSymbol(QPainter* painter, const QStyleOptionGraphicsItem* option);
    void paintHandles(QPainter* painter) const;
    void animateHover(qreal target);
    uml_symbols::SymbolContent symbolContent() const;

    // Gemini hover glow, faded in / out with the M3 emphasized easing
    class QVariantAnimation* m_hoverAnim{nullptr};
    qreal m_hoverScale{0.0};
    bool snapping() const;
};
