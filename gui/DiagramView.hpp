#pragma once

#include <QGraphicsView>
#include <QGraphicsScene>
#include <memory>
#include <unordered_map>
#include "ea/model/Diagram.hpp"
#include "ea/repository/Repository.hpp"
#include "DiagramElementItem.hpp"
#include "DiagramConnectorItem.hpp"

class DiagramView : public QGraphicsView {
    Q_OBJECT
public:
    explicit DiagramView(std::shared_ptr<ea::Diagram> diagram, ea::Repository& repo, QWidget* parent = nullptr);

    int getDiagramId() const { return m_diagramId; }
    QString getDiagramName() const { return m_diagramName; }
    std::shared_ptr<ea::Diagram> getDiagram() const { return m_diagram; }

    void reloadDiagram();
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void fitInView();

    void setPanMode(bool enabled);
    bool isPanMode() const { return m_panMode; }

    bool saveDiagramLayout();

    void addNewElement(ea::ObjectType type, const QString& defaultName = "");
    void startLinkMode();

    void exportToSvg(const QString& filePath);
    void exportToPng(const QString& filePath);

signals:
    void elementSelected(int elementId);
    void diagramModified();

protected:
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;

private slots:
    void onRemoveElementRequested(int elementId);
    void onRemoveConnectorRequested(int connectorId);

private:
    void createConnectorBetween(DiagramElementItem* startItem, DiagramElementItem* endItem);

    std::shared_ptr<ea::Diagram> m_diagram;
    ea::Repository& m_repo;
    int m_diagramId{0};
    QString m_diagramName;

    QGraphicsScene* m_scene{nullptr};
    std::unordered_map<int, DiagramElementItem*> m_elementItems;
    std::vector<DiagramConnectorItem*> m_connectorItems;

    double m_scaleFactor{1.0};
    bool m_panMode{false};
    bool m_spacePressed{false};
    bool m_middleMousePanning{false};
    QPoint m_lastPanPos;

    // Linking mode state
    bool m_linkingMode{false};
    DiagramElementItem* m_linkSourceItem{nullptr};
};
