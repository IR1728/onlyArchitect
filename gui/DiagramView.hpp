#pragma once

class QGraphicsLineItem;
class QGraphicsRectItem;
class QLabel;

#include <QElapsedTimer>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHash>
#include <QTimer>
#include <QUndoStack>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include "ea/editor/DiagramObserver.hpp"
#include "ea/diagram/DiagramPalette.hpp"
#include "ea/diagram/SequenceModel.hpp"
#include "ea/editor/EditCommands.hpp"
#include "ea/model/Diagram.hpp"
#include "ea/repository/Repository.hpp"
#include "DiagramElementItem.hpp"
#include "DiagramConnectorItem.hpp"

// Canvas of one diagram. Turns user gestures into editor-engine commands (pushed on its own
// QUndoStack) and, as the engine's DiagramObserver, mirrors their effects in the scene.
class DiagramView : public QGraphicsView, private ea::editor::DiagramObserver {
    Q_OBJECT
public:
    explicit DiagramView(std::shared_ptr<ea::Diagram> diagram, ea::Repository& repo, QWidget* parent = nullptr);

    int getDiagramId() const { return m_diagramId; }
    QString getDiagramName() const { return m_diagramName; }
    std::shared_ptr<ea::Diagram> getDiagram() const { return m_diagram; }

    // Every edit made through this view is recorded here (Ctrl+Z / Ctrl+Y)
    QUndoStack* undoStack() const { return m_undoStack; }

    void reloadDiagram();
    // In-place reload after a remote sync update: keeps zoom, scrollbars, selection and live cursors
    void refreshDiagram(std::shared_ptr<ea::Diagram> diagram);
    // Live multiplayer presence & drag updates (no SQLite reload)
    void updateRemoteCursor(const QString& username, const QPointF& scenePos);
    void pruneRemoteCursors(const QStringList& activePeers);
    void clearRemoteCursors();
    void applyRemoteElementGeometry(int elementId, const ea::editor::ElementGeometry& geometry);
    // Zoom is kept between 10 % and 400 % (ea::editor::MinZoom / MaxZoom)
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void fitInView();
    double zoom() const;
    void setZoom(double factor);

    // Snapping to the grid while dragging and resizing (on by default; Alt bypasses it)
    void setSnapToGrid(bool on);
    bool snapToGrid() const;

    // An existing model element dropped on the canvas (from the Model Explorer), centred on `scenePos`;
    // returns false if it could not be placed (e.g. already on the diagram, then it is selected)
    bool placeElement(int elementId, const QPointF& scenePos);

    void setPanMode(bool enabled);
    bool isPanMode() const { return m_panMode; }

    bool saveDiagramLayout();

    // --- Interactive entry points (ask the user, then edit) ---
    void addNewElement(ea::ObjectType type, const QString& defaultName = "");
    // Link mode: the next two clicked symbols become source and target. With a relationship tool the
    // relationship is created directly; without one the user is asked for its type.
    void startLinkMode();
    void startLinkMode(const ea::PaletteTool& relationshipTool);
    bool isLinking() const { return m_linkingMode; }
    // A Toolbox tool: elements are created (centred on `scenePos`), relationships start link mode
    int applyTool(const ea::PaletteTool& tool, const QPointF& scenePos);
    int applyToolAtCentre(const ea::PaletteTool& tool);
    void editElement(int elementId);
    void editConnector(int connectorId);
    void renameSelection();
    void applyAutoLayout();

    // --- Undoable edits without dialogs; each one is a single undo step ---
    // Return the new ID, or 0 if nothing was created
    int addElement(ea::ObjectType type, const QString& name, const QPointF& topLeft,
                   const QString& stereotype = QString(), int ntype = 0);
    int addConnector(int sourceElementId, int targetElementId, ea::ConnectorType type,
                     const QString& stereotype = QString());
    // Interaction messages (sequence diagrams): created at height `y`, the others renumbered; one undo step
    int addMessage(int sourceElementId, int targetElementId, ea::sequence::MessageSort sort, int y,
                   const QString& name = QString());
    bool moveMessage(int connectorId, int y);
    bool isInteractionDiagram() const;
    void removeFromDiagram(const QList<int>& elementIds, const QList<int>& connectorIds);
    void removeSelection();
    bool modifyElement(int elementId, const QString& description, const std::function<void(ea::Element&)>& mutation);

    DiagramElementItem* elementItem(int elementId) const;
    DiagramConnectorItem* connectorItem(int connectorId) const;

    void exportToSvg(const QString& filePath);
    void exportToPng(const QString& filePath);
    void setShowCanvasTitle(bool show);
    bool showCanvasTitle() const { return m_showCanvasTitle; }
    void autoFitElementSizes();

signals:
    void zoomChanged(int percent);
    void elementSelected(int elementId);
    void diagramModified();
    // Emitted after a drag/resize/auto-layout finishes so multiplayer sync saves & pushes automatically
    void diagramGeometryCommitted();
    // Throttled live multiplayer signals
    void cursorMovedInScene(int diagramId, const QPointF& scenePos);
    void elementLiveMoved(int diagramId, int elementId, const ea::editor::ElementGeometry& geometry);
    // Elements or relationships were created, deleted or edited (Model Explorer must refresh)
    void modelChanged();
    // An edit could not be carried out; the model was left unchanged
    void editFailed(const QString& message);
    // An edit was refused because it breaks a UML rule (e.g. Interface → Class generalization)
    void editRejected(const QString& reason);
    // User asked to refine a Use Case into a linked Sequence Diagram
    void refineUseCaseRequested(int useCaseElementId);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onRemoveElementRequested(int elementId);
    void onRemoveConnectorRequested(int connectorId);
    void onAddAttributeRequested(int elementId);
    void onAddOperationRequested(int elementId);

private:
    // ea::editor::DiagramObserver: the engine reports what changed
    void elementShown(const std::shared_ptr<ea::DiagramObject>& obj, const std::shared_ptr<ea::Element>& elem) override;
    void elementHidden(int elementId) override;
    void connectorShown(const std::shared_ptr<ea::DiagramLink>& link, const std::shared_ptr<ea::Connector>& conn) override;
    void connectorHidden(int connectorId) override;
    void geometryChanged(int elementId, const ea::editor::ElementGeometry& geometry) override;
    void elementChanged(int elementId) override;
    void connectorChanged(int connectorId) override;
    void modelEdited() override;

    void applySymbolStyle(DiagramElementItem* item) const;
    std::vector<const ea::Connector*> diagramMessages() const;
    void refreshActivations();

    ea::editor::EditContext editContext();
    // Pushes the command as one undo step; false if it failed (editFailed has been emitted)
    bool run(std::shared_ptr<ea::editor::Command> command);
    void pushGeometryChanges(std::vector<ea::editor::GeometryChange> changes);

    void createConnectorBetween(DiagramElementItem* startItem, DiagramElementItem* endItem);
    void stopLinkMode();
    QString defaultNameFor(const ea::PaletteTool& tool) const;
    void wireElementItem(DiagramElementItem* item);
    void selectOnly(int elementId);
    DiagramElementItem* elementItemAt(const QPoint& viewPos) const;
    void beginGeometryTracking(const QPoint& viewPos);
    void commitGeometryTracking();

    std::shared_ptr<ea::Diagram> m_diagram;
    ea::Repository& m_repo;
    int m_diagramId{0};
    QString m_diagramName;

    QGraphicsScene* m_scene{nullptr};
    QUndoStack* m_undoStack{nullptr};
    bool m_lastEditFailed{false};
    std::unordered_map<int, DiagramElementItem*> m_elementItems;
    std::unordered_map<int, DiagramConnectorItem*> m_connectorItems;

    // Geometry of the symbols a left-button drag can move or resize (the selection + the pressed one)
    std::unordered_map<int, ea::editor::ElementGeometry> m_dragStartGeometry;
    bool m_trackingDrag{false};

    double m_scaleFactor{1.0};
    bool m_showCanvasTitle{false}; // hidden while editing (tab bar already shows it); shown on PNG/SVG export
    // Marquee (rubber band) selection on the background
    bool m_marqueeActive{false};
    bool m_marqueeAdditive{false};
    QPointF m_marqueeStart;
    QGraphicsRectItem* m_marqueeItem{nullptr};
    void updateMarquee(const QPointF& scenePos);
    void finishMarquee(const QPointF& scenePos);
    bool m_panMode{false};
    bool m_spacePressed{false};
    bool m_middleMousePanning{false};
    QPoint m_lastPanPos;

    // Linking mode state
    bool m_linkingMode{false};
    DiagramElementItem* m_linkSourceItem{nullptr};
    std::optional<ea::PaletteTool> m_linkTool; // relationship chosen in the Toolbox, if any
    double m_linkStartY{0.0};                  // where the source was clicked (a message's height)
    QGraphicsLineItem* m_linkPreviewLine{nullptr};
    QLabel* m_linkBanner{nullptr};
    void updateLinkBanner(const QString& text);
    void updateLinkPreview(const QPointF& scenePos);
    void clearLinkPreview();

    // Live multiplayer cursors & throttled broadcast state
    struct RemoteCursorEntry {
        QGraphicsItem* item{nullptr};
        qint64 lastSeenMs{0};
    };
    QHash<QString, RemoteCursorEntry> m_remoteCursors;
    QTimer* m_cursorCleanupTimer{nullptr};
    QElapsedTimer m_cursorSendTimer;
    QElapsedTimer m_moveSendTimer;
    bool m_applyingRemoteGeometry{false};
};
