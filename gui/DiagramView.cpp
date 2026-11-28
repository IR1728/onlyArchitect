#include "DiagramView.hpp"
#include "DarkTheme.hpp"
#include "M3Icons.hpp"
#include "UndoableEdit.hpp"
#include "EditConnectorDialog.hpp"
#include "EditElementDialog.hpp"
#include "ea/diagram/DiagramRenderer.hpp"
#include "ea/diagram/DiagramLayout.hpp"
#include "ea/core/Utils.hpp"
#include <QWheelEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QPainter>
#include <QImage>
#include <QtSvg/QSvgGenerator>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QRegularExpression>
#include <QDragEnterEvent>
#include <QDropEvent>
#include "ToolboxWidget.hpp"
#include "ElementDrag.hpp"
#include <QDateTime>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QLabel>
#include <QResizeEvent>
#include <QSignalBlocker>
#include "ea/editor/CanvasGeometry.hpp"
#include "SequenceSymbols.hpp"
#include <cmath>

using ea::editor::ElementGeometry;
using ea::editor::GeometryChange;

namespace {

QColor colorForPeer(const QString& username) {
    static const QColor kPalette[] = {
        QColor("#88C0D0"), QColor("#A3BE8C"), QColor("#EBCB8B"),
        QColor("#D08770"), QColor("#B48EAD"), QColor("#BF616A"),
        QColor("#81A1C1"), QColor("#8FBCBB")
    };
    const uint h = qHash(username);
    return kPalette[h % (sizeof(kPalette) / sizeof(kPalette[0]))];
}

class RemoteCursorItem : public QGraphicsItem {
public:
    explicit RemoteCursorItem(const QString& username)
        : m_username(username), m_color(colorForPeer(username)) {
        setZValue(2000);
        setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        setAcceptedMouseButtons(Qt::NoButton);
    }

    QRectF boundingRect() const override {
        return QRectF(-2, -2, 160, 38);
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        painter->setRenderHint(QPainter::Antialiasing);

        // Pointer arrow tip at (0, 0)
        QPolygonF arrow;
        arrow << QPointF(0, 0) << QPointF(0, 16) << QPointF(5, 12) << QPointF(11, 12);
        painter->setPen(QPen(QColor("#1E222A"), 1.2));
        painter->setBrush(m_color);
        painter->drawPolygon(arrow);

        // Name pill next to pointer
        QFont f = painter->font();
        f.setPointSizeF(8.5);
        f.setBold(true);
        painter->setFont(f);
        const QFontMetrics fm(f);
        const int w = std::min(130, fm.horizontalAdvance(m_username) + 14);
        const QRectF pill(10, 12, w, 18);
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_color);
        painter->drawRoundedRect(pill, 7, 7);

        painter->setPen(QColor("#1E222A"));
        painter->drawText(pill, Qt::AlignCenter, m_username);
    }

private:
    QString m_username;
    QColor m_color;
};

} // namespace

DiagramView::DiagramView(std::shared_ptr<ea::Diagram> diagram, ea::Repository& repo, QWidget* parent)
    : QGraphicsView(parent), m_diagram(diagram), m_repo(repo) {
    if (m_diagram) {
        m_diagramId = m_diagram->GetDiagramID();
        m_diagramName = QString::fromStdString(m_diagram->GetName());
    }

    m_scene = new QGraphicsScene(this);
    m_scene->setProperty("snapToGrid", true);
    setScene(m_scene);

    m_undoStack = new QUndoStack(this);

    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::TextAntialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setDragMode(QGraphicsView::NoDrag); // Allows items to be dragged smoothly!
    setAcceptDrops(true);               // Toolbox tools can be dropped on the canvas
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    setBackgroundBrush(ea::gui::m3().surfaceLowest); // M3 surface-container-lowest: infinite canvas

    m_cursorSendTimer.start();
    m_moveSendTimer.start();
    m_cursorCleanupTimer = new QTimer(this);
    m_cursorCleanupTimer->setInterval(2000);
    connect(m_cursorCleanupTimer, &QTimer::timeout, this, [this]() {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        for (auto it = m_remoteCursors.begin(); it != m_remoteCursors.end();) {
            if (now - it->lastSeenMs > 8000) {
                if (it->item) {
                    m_scene->removeItem(it->item);
                    delete it->item;
                }
                it = m_remoteCursors.erase(it);
            } else {
                ++it;
            }
        }
    });
    m_cursorCleanupTimer->start();

    reloadDiagram();
}

void DiagramView::reloadDiagram() {
    if (!m_diagram) return;

    // Items are recreated; commands refer to elements by ID, so the undo history stays valid
    m_linkSourceItem = nullptr;
    m_trackingDrag = false;
    m_dragStartGeometry.clear();

    m_marqueeActive = false;
    m_marqueeItem = nullptr; // owned by the scene, cleared below
    m_linkPreviewLine = nullptr; // owned by the scene, cleared below
    m_remoteCursors.clear(); // scene->clear() deletes all items including remote cursors
    m_scene->clear();
    m_elementItems.clear();
    m_connectorItems.clear();

    for (const auto& obj : m_diagram->GetDiagramObjects()) {
        if (obj) elementShown(obj, m_repo.GetElementByID(obj->GetElementID()));
    }
    for (const auto& link : m_diagram->GetDiagramLinks()) {
        if (link && !link->IsHidden()) connectorShown(link, m_repo.GetConnectorByID(link->GetConnectorID()));
    }
    refreshActivations();

    // Set scene bounds
    QRectF sceneBounds = m_scene->itemsBoundingRect().adjusted(-200, -200, 400, 400);
    if (sceneBounds.width() < 1200) sceneBounds.setWidth(1200);
    if (sceneBounds.height() < 900) sceneBounds.setHeight(900);
    m_scene->setSceneRect(sceneBounds);
}

void DiagramView::refreshDiagram(std::shared_ptr<ea::Diagram> diagram) {
    if (!diagram) return;

    // Preserve camera (zoom + scrollbars), selection, and active remote cursors so remote sync feels seamless
    const QTransform savedTransform = transform();
    const int savedHScroll = horizontalScrollBar()->value();
    const int savedVScroll = verticalScrollBar()->value();

    QList<int> selectedIds;
    for (auto* item : m_scene->selectedItems()) {
        if (auto* elem = dynamic_cast<DiagramElementItem*>(item)) {
            selectedIds.append(elem->getElementId());
        }
    }

    QHash<QString, std::pair<QPointF, qint64>> savedCursors;
    for (auto it = m_remoteCursors.constBegin(); it != m_remoteCursors.constEnd(); ++it) {
        if (it->item) savedCursors.insert(it.key(), {it->item->pos(), it->lastSeenMs});
    }

    m_diagram = std::move(diagram);
    m_diagramId = m_diagram->GetDiagramID();
    m_diagramName = QString::fromStdString(m_diagram->GetName());

    setUpdatesEnabled(false);
    reloadDiagram();

    for (int id : selectedIds) {
        if (auto* item = elementItem(id)) item->setSelected(true);
    }
    for (auto it = savedCursors.constBegin(); it != savedCursors.constEnd(); ++it) {
        auto* cursor = new RemoteCursorItem(it.key());
        cursor->setPos(it.value().first);
        m_scene->addItem(cursor);
        m_remoteCursors.insert(it.key(), {cursor, it.value().second});
    }

    setTransform(savedTransform);
    horizontalScrollBar()->setValue(savedHScroll);
    verticalScrollBar()->setValue(savedVScroll);
    setUpdatesEnabled(true);
    viewport()->update();
}

void DiagramView::updateRemoteCursor(const QString& username, const QPointF& scenePos) {
    if (username.isEmpty()) return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto it = m_remoteCursors.find(username);
    if (it == m_remoteCursors.end() || !it->item) {
        auto* cursor = new RemoteCursorItem(username);
        cursor->setPos(scenePos);
        m_scene->addItem(cursor);
        m_remoteCursors.insert(username, {cursor, now});
    } else {
        it->item->setPos(scenePos);
        it->lastSeenMs = now;
    }
}

void DiagramView::pruneRemoteCursors(const QStringList& activePeers) {
    for (auto it = m_remoteCursors.begin(); it != m_remoteCursors.end();) {
        if (!activePeers.contains(it.key(), Qt::CaseInsensitive)) {
            if (it->item) {
                m_scene->removeItem(it->item);
                delete it->item;
            }
            it = m_remoteCursors.erase(it);
        } else {
            ++it;
        }
    }
}

void DiagramView::clearRemoteCursors() {
    for (auto it = m_remoteCursors.begin(); it != m_remoteCursors.end(); ++it) {
        if (it->item) {
            m_scene->removeItem(it->item);
            delete it->item;
        }
    }
    m_remoteCursors.clear();
}

void DiagramView::applyRemoteElementGeometry(int elementId, const ea::editor::ElementGeometry& geometry) {
    auto* item = elementItem(elementId);
    if (!item) return;
    m_applyingRemoteGeometry = true;
    {
        const QSignalBlocker block(item);
        item->applyGeometry(geometry);
    }
    m_applyingRemoteGeometry = false;
}

bool DiagramView::saveDiagramLayout() {
    if (!m_diagram) return false;
    bool ok = m_repo.SaveDiagram(*m_diagram);
    if (ok) m_undoStack->setClean();
    return ok;
}

void DiagramView::setPanMode(bool enabled) {
    m_panMode = enabled;
    if (m_panMode) {
        setDragMode(QGraphicsView::ScrollHandDrag);
        setCursor(Qt::OpenHandCursor);
    } else {
        setDragMode(QGraphicsView::NoDrag);
        setCursor(Qt::ArrowCursor);
    }
}

void DiagramView::zoomIn() {
    setZoom(ea::editor::zoom_by_steps(zoom(), +1));
}

void DiagramView::zoomOut() {
    setZoom(ea::editor::zoom_by_steps(zoom(), -1));
}

void DiagramView::resetZoom() {
    setZoom(1.0);
}

double DiagramView::zoom() const {
    return transform().m11();
}

void DiagramView::setZoom(double factor) {
    const double z = ea::editor::clamp_zoom(factor);
    setTransform(QTransform::fromScale(z, z));
    m_scaleFactor = z;
    emit zoomChanged(static_cast<int>(std::lround(z * 100)));
}

void DiagramView::fitInView() {
    QRectF r = m_scene->itemsBoundingRect();
    if (!r.isEmpty()) {
        QGraphicsView::fitInView(r.adjusted(-40, -40, 40, 40), Qt::KeepAspectRatio);
        setZoom(zoom()); // a huge or tiny diagram still stays within 10 %..400 %
        centerOn(r.center());
    }
}

void DiagramView::setSnapToGrid(bool on) {
    m_scene->setProperty("snapToGrid", on);
}

bool DiagramView::snapToGrid() const {
    return m_scene->property("snapToGrid").toBool();
}

bool DiagramView::placeElement(int elementId, const QPointF& scenePos) {
    if (!m_diagram) return false;
    auto elem = m_repo.GetElementByID(elementId);
    if (!elem) return false;
    if (elementItem(elementId)) { // already shown: point at it instead of duplicating
        selectOnly(elementId);
        centerOn(elementItem(elementId));
        emit editFailed(QString("\"%1\" ya está en este diagrama.").arg(QString::fromStdString(elem->GetName())));
        return false;
    }
    const auto size = ea::sequence::default_symbol_size(elem->GetType(), m_diagram->GetType());
    const auto [x, y] = ea::editor::snap_position(scenePos.x() - size.width / 2.0, scenePos.y() - size.height / 2.0);
    const int left = static_cast<int>(x), top = -static_cast<int>(y);
    if (!run(std::make_shared<ea::editor::PlaceElementCommand>(editContext(), elementId,
                                                                 ElementGeometry{left, top, left + size.width, top - size.height}))) {
        return false;
    }
    selectOnly(elementId);
    return true;
}

void DiagramView::updateMarquee(const QPointF& p) {
    const QRectF r = QRectF(m_marqueeStart, p).normalized();
    m_marqueeItem->setRect(r);
    // right-to-left: crossing selection (dashed), left-to-right: enclosing selection (solid)
    QPen pen(ea::gui::m3().primary, 1.0, p.x() < m_marqueeStart.x() ? Qt::DashLine : Qt::SolidLine);
    pen.setCosmetic(true);
    m_marqueeItem->setPen(pen);
}

void DiagramView::finishMarquee(const QPointF& end) {
    m_marqueeActive = false;
    m_scene->removeItem(m_marqueeItem);
    delete m_marqueeItem;
    m_marqueeItem = nullptr;

    std::vector<DiagramElementItem*> items;
    std::vector<ea::editor::Box> boxes;
    items.reserve(m_elementItems.size());
    boxes.reserve(m_elementItems.size());
    for (const auto& [id, item] : m_elementItems) {
        items.push_back(item);
        boxes.push_back(item->sceneBox());
    }
    if (!m_marqueeAdditive) m_scene->clearSelection();
    for (size_t i : ea::editor::marquee_select(m_marqueeStart.x(), m_marqueeStart.y(), end.x(), end.y(), boxes)) {
        items[i]->setSelected(true);
    }
}

// ============================================================================
// Item lookup
// ============================================================================

DiagramElementItem* DiagramView::elementItem(int elementId) const {
    auto it = m_elementItems.find(elementId);
    return it != m_elementItems.end() ? it->second : nullptr;
}

DiagramConnectorItem* DiagramView::connectorItem(int connectorId) const {
    auto it = m_connectorItems.find(connectorId);
    return it != m_connectorItems.end() ? it->second : nullptr;
}

DiagramElementItem* DiagramView::elementItemAt(const QPoint& viewPos) const {
    for (auto* item : items(viewPos)) {
        if (auto* elem = dynamic_cast<DiagramElementItem*>(item)) return elem;
    }
    return nullptr;
}

void DiagramView::selectOnly(int elementId) {
    auto* item = elementItem(elementId);
    if (!item) return;
    m_scene->clearSelection();
    item->setSelected(true);
    emit elementSelected(elementId);
}

// ============================================================================
// ea::editor::DiagramObserver — mirror the engine's changes in the scene
// ============================================================================

void DiagramView::wireElementItem(DiagramElementItem* item) {
    connect(item, &DiagramElementItem::elementClicked, this, &DiagramView::elementSelected);
    connect(item, &DiagramElementItem::positionModified, this, [this, item]() {
        emit diagramModified();
        if (!m_applyingRemoteGeometry && m_diagramId > 0 && m_moveSendTimer.elapsed() >= 35) {
            m_moveSendTimer.restart();
            emit elementLiveMoved(m_diagramId, item->getElementId(), item->geometry());
        }
    });
    connect(item, &DiagramElementItem::removeRequested, this, &DiagramView::onRemoveElementRequested);
    connect(item, &DiagramElementItem::editRequested, this, &DiagramView::editElement);
    connect(item, &DiagramElementItem::addAttributeRequested, this, &DiagramView::onAddAttributeRequested);
    connect(item, &DiagramElementItem::addOperationRequested, this, &DiagramView::onAddOperationRequested);
    connect(item, &DiagramElementItem::refineToSequenceRequested, this, &DiagramView::refineUseCaseRequested);
}

void DiagramView::elementShown(const std::shared_ptr<ea::DiagramObject>& obj, const std::shared_ptr<ea::Element>& elem) {
    if (!obj || elementItem(obj->GetElementID())) return;
    auto* item = new DiagramElementItem(obj, elem, m_repo);
    applySymbolStyle(item);
    m_scene->addItem(item);
    m_elementItems.emplace(obj->GetElementID(), item);
    wireElementItem(item);
    emit diagramModified();
}

void DiagramView::elementHidden(int elementId) {
    auto it = m_elementItems.find(elementId);
    if (it == m_elementItems.end()) return;
    auto* item = it->second;

    // Its connectors cannot be drawn without it; their diagram links stay in the model
    for (auto connIt = m_connectorItems.begin(); connIt != m_connectorItems.end();) {
        auto* conn = connIt->second;
        if (conn->startItem() == item || conn->endItem() == item) {
            m_scene->removeItem(conn);
            delete conn;
            connIt = m_connectorItems.erase(connIt);
        } else {
            ++connIt;
        }
    }

    if (m_linkSourceItem == item) m_linkSourceItem = nullptr;
    m_dragStartGeometry.erase(elementId);
    m_scene->removeItem(item);
    m_elementItems.erase(it);
    delete item;
    emit diagramModified();
}

void DiagramView::connectorShown(const std::shared_ptr<ea::DiagramLink>& link, const std::shared_ptr<ea::Connector>& conn) {
    if (!link || !conn || connectorItem(conn->GetConnectorID())) return;
    auto* start = elementItem(conn->GetClientID());
    auto* end = elementItem(conn->GetSupplierID());
    if (!start || !end) return; // an endpoint is not on this diagram

    auto* item = new DiagramConnectorItem(link, conn, start, end);
    m_scene->addItem(item);
    m_connectorItems.emplace(conn->GetConnectorID(), item);
    connect(item, &DiagramConnectorItem::editRequested, this, &DiagramView::editConnector);
    connect(item, &DiagramConnectorItem::removeRequested, this, &DiagramView::onRemoveConnectorRequested);
    connect(item, &DiagramConnectorItem::messageDragged, this, [this](int id, int y) { moveMessage(id, y); });
    if (item->isSequenceMessage()) refreshActivations();
    emit diagramModified();
}

void DiagramView::connectorHidden(int connectorId) {
    auto it = m_connectorItems.find(connectorId);
    if (it == m_connectorItems.end()) return;
    auto* item = it->second;
    m_connectorItems.erase(it);
    m_scene->removeItem(item);
    delete item;
    refreshActivations();
    emit diagramModified();
}

void DiagramView::geometryChanged(int elementId, const ElementGeometry& geometry) {
    if (auto* item = elementItem(elementId)) item->applyGeometry(geometry);
    emit diagramModified();
}

void DiagramView::elementChanged(int elementId) {
    if (auto* item = elementItem(elementId)) item->update();
    emit elementSelected(elementId); // refreshes the Property Editor
}

void DiagramView::connectorChanged(int connectorId) {
    if (auto* item = connectorItem(connectorId)) item->updatePosition();
    refreshActivations();
}

bool DiagramView::isInteractionDiagram() const {
    return m_diagram && m_diagram->GetType() == ea::DiagramType::Sequence;
}

void DiagramView::applySymbolStyle(DiagramElementItem* item) const {
    const auto elem = item->getElement();
    if (!elem) return;
    if (ea::sequence::is_interaction_frame(elem->GetType())) {
        item->setSymbolStyle(DiagramElementItem::SymbolStyle::Frame);
    } else if (isInteractionDiagram() && ea::sequence::is_lifeline_kind(elem->GetType())) {
        item->setSymbolStyle(DiagramElementItem::SymbolStyle::Lifeline);
    }
}

std::vector<const ea::Connector*> DiagramView::diagramMessages() const {
    std::vector<const ea::Connector*> messages;
    messages.reserve(m_connectorItems.size());
    for (const auto& [id, item] : m_connectorItems) {
        const auto conn = item->getConnector();
        if (conn && conn->GetType() == ea::ConnectorType::Message) messages.push_back(conn.get());
    }
    return messages;
}

void DiagramView::refreshActivations() {
    if (!isInteractionDiagram()) return;
    std::unordered_map<int, std::vector<DiagramElementItem::Activation>> perLifeline;
    for (const auto& a : ea::sequence::compute_activations(diagramMessages())) {
        perLifeline[a.lifelineId].push_back({static_cast<double>(a.top), static_cast<double>(a.bottom), a.depth});
    }
    for (const auto& [id, item] : m_elementItems) {
        if (item->symbolStyle() != DiagramElementItem::SymbolStyle::Lifeline) continue;
        auto it = perLifeline.find(id);
        item->setActivations(it != perLifeline.end() ? std::move(it->second) : std::vector<DiagramElementItem::Activation>{});
    }
}

int DiagramView::addMessage(int sourceElementId, int targetElementId, ea::sequence::MessageSort sort, int y,
                            const QString& name) {
    if (!m_diagram || !elementItem(sourceElementId) || !elementItem(targetElementId)) return 0;

    // Number and position of the new message; the ones below it move down one number
    auto placements = ea::sequence::place_message(diagramMessages(), /*connectorId=*/0, y);
    ea::sequence::MessagePosition position{y, 1};
    std::vector<ea::sequence::MessagePlacement> others;
    for (const auto& p : placements) {
        if (p.connectorId == 0) position = p.after;
        else others.push_back(p);
    }

    auto conn = std::make_shared<ea::Connector>();
    conn->SetType(ea::ConnectorType::Message);
    conn->SetName(name.isEmpty() ? (sort == ea::sequence::MessageSort::Reply ? std::string() : "message" + std::to_string(position.sequenceNo))
                                 : name.toStdString());
    conn->SetClientID(sourceElementId);
    conn->SetSupplierID(targetElementId);
    conn->SetDiagramID(m_diagram->GetDiagramID());
    conn->SetConnectorGUID(ea::utils::generate_guid());
    ea::sequence::set_message_sort(*conn, sort);
    ea::sequence::set_message_y(*conn, position.y);
    conn->SetSequenceNo(position.sequenceNo);

    auto add = std::make_shared<ea::editor::AddConnectorCommand>(editContext(), conn);
    std::vector<std::shared_ptr<ea::editor::Command>> parts{add};
    if (!others.empty()) {
        parts.push_back(std::make_shared<ea::editor::PlaceMessagesCommand>(editContext(), std::move(others), "Renumerar mensajes"));
    }
    const std::string description = "Añadir mensaje " + conn->GetName();
    if (!run(std::make_shared<ea::editor::CompositeCommand>(std::move(parts), description))) return 0;
    return add->connectorId();
}

bool DiagramView::moveMessage(int connectorId, int y) {
    auto placements = ea::sequence::place_message(diagramMessages(), connectorId, y);
    if (placements.empty()) return true;
    return run(std::make_shared<ea::editor::PlaceMessagesCommand>(editContext(), std::move(placements), "Mover mensaje"));
}

void DiagramView::modelEdited() {
    emit modelChanged();
}

// ============================================================================
// Running commands
// ============================================================================

ea::editor::EditContext DiagramView::editContext() {
    return {m_repo, m_diagram, *this};
}

bool DiagramView::run(std::shared_ptr<ea::editor::Command> command) {
    m_lastEditFailed = false;
    m_undoStack->push(new UndoableEdit(std::move(command), [this](UndoableEdit::Failure kind, const QString& message) {
        m_lastEditFailed = true;
        if (kind == UndoableEdit::Failure::RuleViolation) emit editRejected(message);
        else emit editFailed(message);
    }));
    return !m_lastEditFailed;
}

void DiagramView::pushGeometryChanges(std::vector<GeometryChange> changes) {
    if (changes.empty()) return;
    if (run(std::make_shared<ea::editor::ChangeGeometryCommand>(editContext(), std::move(changes)))) {
        emit diagramGeometryCommitted();
    }
}

// ============================================================================
// Undoable edits
// ============================================================================

int DiagramView::addElement(ea::ObjectType type, const QString& name, const QPointF& topLeft, const QString& stereotype,
                            int ntype) {
    if (!m_diagram || name.trimmed().isEmpty()) return 0;

    auto elem = std::make_shared<ea::Element>();
    elem->SetName(name.trimmed().toStdString());
    elem->SetType(type);
    elem->SetStereotype(stereotype.toStdString());
    elem->SetNType(ntype);
    elem->SetPackageID(m_diagram->GetPackageID());
    elem->SetElementGUID(ea::utils::generate_guid());

    // EA stores Top/Bottom as negative screen Y
    const int left = static_cast<int>(topLeft.x());
    const int top = -static_cast<int>(topLeft.y());
    const auto size = ea::sequence::default_symbol_size(type, m_diagram->GetType());
    int alignedTop = top;
    if (isInteractionDiagram() && ea::sequence::is_lifeline_kind(type)) {
        // lifelines of an interaction share the same head line
        for (const auto& [id, item] : m_elementItems) {
            if (item->symbolStyle() == DiagramElementItem::SymbolStyle::Lifeline) {
                alignedTop = -static_cast<int>(item->pos().y());
                break;
            }
        }
    }
    const ElementGeometry placement{left, alignedTop, left + size.width, alignedTop - size.height};

    auto command = std::make_shared<ea::editor::AddElementCommand>(editContext(), elem, placement);
    if (!run(command)) return 0;
    selectOnly(command->elementId());
    return command->elementId();
}

int DiagramView::addConnector(int sourceElementId, int targetElementId, ea::ConnectorType type,
                              const QString& stereotype) {
    if (!m_diagram || sourceElementId == targetElementId) return 0;
    if (!elementItem(sourceElementId) || !elementItem(targetElementId)) return 0;

    auto conn = std::make_shared<ea::Connector>();
    conn->SetType(type);
    conn->SetStereotype(stereotype.toStdString());
    conn->SetClientID(sourceElementId);
    conn->SetSupplierID(targetElementId);
    conn->SetConnectorGUID(ea::utils::generate_guid());

    auto command = std::make_shared<ea::editor::AddConnectorCommand>(editContext(), conn);
    return run(command) ? command->connectorId() : 0;
}

void DiagramView::removeFromDiagram(const QList<int>& elementIds, const QList<int>& connectorIds) {
    const auto total = elementIds.size() + connectorIds.size();
    if (total == 0) return;

    if (total > 1) m_undoStack->beginMacro(QString("Eliminar %1 elementos").arg(total));
    // Connectors first: undo then restores elements before re-showing their connectors
    for (int id : connectorIds) {
        if (connectorItem(id)) run(std::make_shared<ea::editor::HideConnectorCommand>(editContext(), id));
    }
    for (int id : elementIds) {
        if (elementItem(id)) run(std::make_shared<ea::editor::RemoveElementFromDiagramCommand>(editContext(), id));
    }
    if (total > 1) m_undoStack->endMacro();
}

void DiagramView::removeSelection() {
    // Collect IDs first: removing an element deletes its connector items, which may be selected too
    QList<int> elementIds;
    QList<int> connectorIds;
    for (auto* item : m_scene->selectedItems()) {
        if (auto* elemItem = dynamic_cast<DiagramElementItem*>(item)) {
            elementIds.append(elemItem->getElementId());
        } else if (auto* connItem = dynamic_cast<DiagramConnectorItem*>(item)) {
            connectorIds.append(connItem->getConnectorId());
        }
    }
    removeFromDiagram(elementIds, connectorIds);
}

bool DiagramView::modifyElement(int elementId, const QString& description,
                                const std::function<void(ea::Element&)>& mutation) {
    auto elem = m_repo.GetElementByID(elementId);
    if (!elem) return false;
    auto command = ea::editor::make_edit_element_command(editContext(), *elem, mutation, description.toStdString());
    return !command || run(std::move(command)); // no change: nothing to do, nothing to undo
}

// ============================================================================
// Interactive entry points
// ============================================================================

void DiagramView::addNewElement(ea::ObjectType type, const QString& defaultName) {
    if (!m_diagram) return;

    bool ok = false;
    QString title = QString("Add %1").arg(QString::fromStdString(to_string(type)));
    QString name = QInputDialog::getText(this, title, "Element Name:", QLineEdit::Normal,
                                        defaultName.isEmpty() ? QString("New%1").arg(QString::fromStdString(to_string(type))) : defaultName, &ok);
    if (!ok || name.isEmpty()) return;

    // Centred in the current view
    const QPointF centre = mapToScene(viewport()->rect().center());
    addElement(type, name, centre - QPointF(DiagramElementItem::DefaultWidth / 2.0, DiagramElementItem::DefaultHeight / 2.0));
}

void DiagramView::updateLinkBanner(const QString& text) {
    if (text.isEmpty()) {
        if (m_linkBanner) m_linkBanner->hide();
        return;
    }
    if (!m_linkBanner) {
        m_linkBanner = new QLabel(viewport());
        m_linkBanner->setAlignment(Qt::AlignCenter);
        m_linkBanner->setAttribute(Qt::WA_TransparentForMouseEvents);
        ea::gui::setThemedStyleSheet(m_linkBanner,
            "QLabel { background-color: @primaryContainer; color: @onPrimaryContainer; border: 1px solid @primary; "
            "border-radius: 12px; padding: 6px 14px; font-size: 11.5px; font-weight: 600; }");
    }
    m_linkBanner->setText(text);
    m_linkBanner->adjustSize();
    const int x = std::max(12, (viewport()->width() - m_linkBanner->width()) / 2);
    m_linkBanner->move(x, 12);
    m_linkBanner->show();
    m_linkBanner->raise();
}

void DiagramView::updateLinkPreview(const QPointF& scenePos) {
    if (!m_linkingMode || !m_linkSourceItem) return;
    QPointF origin = m_linkSourceItem->getCenter();
    if (isInteractionDiagram() && m_linkSourceItem->symbolStyle() == DiagramElementItem::SymbolStyle::Lifeline) {
        origin = QPointF(m_linkSourceItem->pos().x() + m_linkSourceItem->getRect().width() / 2.0, m_linkStartY);
    }
    if (!m_linkPreviewLine) {
        m_linkPreviewLine = new QGraphicsLineItem();
        QPen pen(ea::gui::m3().primary, 2.0, Qt::DashLine);
        pen.setCosmetic(true);
        m_linkPreviewLine->setPen(pen);
        m_linkPreviewLine->setZValue(1500);
        m_scene->addItem(m_linkPreviewLine);
    }
    m_linkPreviewLine->setLine(QLineF(origin, scenePos));
}

void DiagramView::clearLinkPreview() {
    if (m_linkPreviewLine) {
        m_scene->removeItem(m_linkPreviewLine);
        delete m_linkPreviewLine;
        m_linkPreviewLine = nullptr;
    }
}

void DiagramView::startLinkMode() {
    clearLinkPreview();
    m_linkingMode = true;
    m_linkSourceItem = nullptr;
    m_linkTool.reset();
    setCursor(Qt::CrossCursor);
    updateLinkBanner("Modo conexión: haz clic en el elemento ORIGEN (Esc cancela)");
}

void DiagramView::startLinkMode(const ea::PaletteTool& relationshipTool) {
    startLinkMode();
    m_linkTool = relationshipTool;
    updateLinkBanner(QString("Conectando %1: haz clic en el elemento ORIGEN (Esc cancela)")
                         .arg(QString::fromStdString(relationshipTool.label)));
}

void DiagramView::stopLinkMode() {
    clearLinkPreview();
    m_linkingMode = false;
    m_linkSourceItem = nullptr;
    m_linkTool.reset();
    setCursor(Qt::ArrowCursor);
    updateLinkBanner(QString());
}

QString DiagramView::defaultNameFor(const ea::PaletteTool& tool) const {
    // EA style: kind name plus a counter ("Class3"), unique among the symbols of this diagram
    QString base = QString::fromStdString(tool.label);
    base.remove(' ');
    base.remove(QRegularExpression("[^\\w]"));
    int n = 1;
    for (const auto& [id, item] : m_elementItems) {
        if (item->getElement() && item->getElement()->GetType() == tool.element) ++n;
    }
    return base + QString::number(n);
}

int DiagramView::applyTool(const ea::PaletteTool& tool, const QPointF& scenePos) {
    if (!tool.isElement()) {
        startLinkMode(tool);
        return 0;
    }
    const auto size = ea::sequence::default_symbol_size(tool.element, m_diagram ? m_diagram->GetType() : ea::DiagramType::Logical);
    const bool lifeline = isInteractionDiagram() && ea::sequence::is_lifeline_kind(tool.element);
    // a lifeline is placed by its head; other symbols by their centre
    const QPointF topLeft = scenePos - QPointF(size.width / 2.0, lifeline ? sequence_symbols::HeadHeight / 2 : size.height / 2.0);
    return addElement(tool.element, defaultNameFor(tool), topLeft, QString::fromStdString(tool.stereotype), tool.ntype);
}

int DiagramView::applyToolAtCentre(const ea::PaletteTool& tool) {
    return applyTool(tool, mapToScene(viewport()->rect().center()));
}

void DiagramView::dragEnterEvent(QDragEnterEvent* event) {
    auto tool = ea::gui::decodePaletteTool(event->mimeData());
    if ((tool && tool->isElement()) || ea::gui::decodeElementDrag(event->mimeData())) event->acceptProposedAction();
    else QGraphicsView::dragEnterEvent(event);
}

void DiagramView::dragMoveEvent(QDragMoveEvent* event) {
    auto tool = ea::gui::decodePaletteTool(event->mimeData());
    if ((tool && tool->isElement()) || ea::gui::decodeElementDrag(event->mimeData())) event->acceptProposedAction();
    else QGraphicsView::dragMoveEvent(event);
}

void DiagramView::dropEvent(QDropEvent* event) {
    if (auto elementId = ea::gui::decodeElementDrag(event->mimeData())) { // from the Model Explorer
        event->acceptProposedAction();
        placeElement(*elementId, mapToScene(event->position().toPoint()));
        return;
    }
    auto tool = ea::gui::decodePaletteTool(event->mimeData());
    if (!tool || !tool->isElement()) {
        QGraphicsView::dropEvent(event);
        return;
    }
    event->acceptProposedAction();
    applyTool(*tool, mapToScene(event->position().toPoint())); // guiaSecuencia §2.3: drag from Toolbox → canvas
}

void DiagramView::createConnectorBetween(DiagramElementItem* startItem, DiagramElementItem* endItem) {
    if (!startItem || !endItem || startItem == endItem || !m_diagram) return;

    QStringList types = {"Association", "Generalization (Inheritance)", "Realization", "Dependency", "Composition", "Aggregation"};
    bool ok = false;
    QString chosen = QInputDialog::getItem(this, "Create Connector", "Connector Type:", types, 0, false, &ok);
    if (!ok) return;

    ea::ConnectorType ctype = ea::ConnectorType::Association;
    if (chosen.startsWith("Generalization")) ctype = ea::ConnectorType::Generalization;
    else if (chosen == "Realization") ctype = ea::ConnectorType::Realization;
    else if (chosen == "Dependency") ctype = ea::ConnectorType::Dependency;
    else if (chosen == "Composition") ctype = ea::ConnectorType::Composition;
    else if (chosen == "Aggregation") ctype = ea::ConnectorType::Aggregation;

    addConnector(startItem->getElementId(), endItem->getElementId(), ctype);
}

void DiagramView::editElement(int elementId) {
    auto elem = m_repo.GetElementByID(elementId);
    if (!elem) return;

    auto before = ea::editor::ElementSnapshot::capture(*elem);
    EditElementDialog dlg(elem, m_repo, this);
    if (dlg.exec() != QDialog::Accepted || !dlg.isModified()) return;

    // The dialog edited the element in place; turn that into an undoable command
    auto after = ea::editor::ElementSnapshot::capture(*elem);
    before.restoreInto(*elem);
    std::string description = "Editar \"" + after.name + "\"";
    run(std::make_shared<ea::editor::EditElementCommand>(editContext(), elementId, std::move(before), std::move(after),
                                                         std::move(description)));
}

void DiagramView::editConnector(int connectorId) {
    auto conn = m_repo.GetConnectorByID(connectorId);
    if (!conn) return;
    EditConnectorDialog dlg(conn, m_repo, this);
    if (dlg.exec() != QDialog::Accepted || !dlg.isModified()) return;
    connectorChanged(connectorId);
    if (auto* item = connectorItem(connectorId)) item->update();
    emit modelChanged();
}

void DiagramView::renameSelection() {
    const auto selected = m_scene->selectedItems();
    if (selected.isEmpty()) return;

    if (auto* elemItem = dynamic_cast<DiagramElementItem*>(selected.first()); elemItem && elemItem->getElement()) {
        const QString cur = QString::fromStdString(elemItem->getElement()->GetName());
        bool ok = false;
        const QString next = QInputDialog::getText(this, "Renombrar Elemento", "Nuevo nombre:", QLineEdit::Normal, cur, &ok).trimmed();
        if (ok && !next.isEmpty() && next != cur) {
            modifyElement(elemItem->getElementId(), QString("Renombrar a \"%1\"").arg(next), [&next](ea::Element& e) {
                e.SetName(next.toStdString());
            });
        }
        return;
    }

    if (auto* connItem = dynamic_cast<DiagramConnectorItem*>(selected.first()); connItem && connItem->getConnector()) {
        auto conn = connItem->getConnector();
        const QString cur = QString::fromStdString(conn->GetName());
        bool ok = false;
        const QString title = connItem->isSequenceMessage() ? "Renombrar Mensaje" : "Renombrar Relación";
        const QString next = QInputDialog::getText(this, title, "Nombre / etiqueta:", QLineEdit::Normal, cur, &ok).trimmed();
        if (ok && next != cur) {
            conn->SetName(next.toStdString());
            m_repo.SaveConnector(*conn);
            connectorChanged(conn->GetConnectorID());
            connItem->update();
            emit modelChanged();
        }
    }
}

void DiagramView::onAddAttributeRequested(int elementId) {
    bool ok = false;
    QString name = QInputDialog::getText(this, "Add Attribute", "Attribute name:", QLineEdit::Normal, "newAttr", &ok);
    if (!ok || name.isEmpty()) return;
    modifyElement(elementId, QString("Añadir atributo %1").arg(name), [&name](ea::Element& elem) {
        elem.GetAttributes().AddNew(name.toStdString(), "int");
    });
}

void DiagramView::onAddOperationRequested(int elementId) {
    bool ok = false;
    QString name = QInputDialog::getText(this, "Add Operation", "Operation name:", QLineEdit::Normal, "newOperation", &ok);
    if (!ok || name.isEmpty()) return;
    modifyElement(elementId, QString("Añadir operación %1").arg(name), [&name](ea::Element& elem) {
        elem.GetMethods().AddNew(name.toStdString(), "void");
    });
}

void DiagramView::applyAutoLayout() {
    if (!m_diagram) return;

    // "Before" is what is on screen: the stored size of tiny symbols is normalised when they are drawn
    std::unordered_map<int, ElementGeometry> before;
    before.reserve(m_elementItems.size());
    for (const auto& [id, item] : m_elementItems) {
        before.emplace(id, item->geometry());
    }

    // The layout rewrites the DiagramObjects; the command moves the symbols (and can move them back)
    ea::DiagramLayout::Layout(*m_diagram, m_repo, ea::LayoutAlgorithm::ForceDirected);

    std::vector<GeometryChange> changes;
    for (const auto& obj : m_diagram->GetDiagramObjects()) {
        if (!obj) continue;
        auto it = before.find(obj->GetElementID());
        const auto after = ElementGeometry::of(*obj);
        if (it != before.end() && !(after == it->second)) changes.push_back({obj->GetElementID(), it->second, after});
    }
    pushGeometryChanges(std::move(changes));
}

void DiagramView::onRemoveElementRequested(int elementId) {
    removeFromDiagram({elementId}, {});
}

void DiagramView::onRemoveConnectorRequested(int connectorId) {
    removeFromDiagram({}, {connectorId});
}

// ============================================================================
// Drag tracking: a move or resize becomes one undo step
// ============================================================================

void DiagramView::beginGeometryTracking(const QPoint& viewPos) {
    // Only what this drag can change: the selection (moved together) and the pressed symbol (resize grip).
    // clear() keeps the map's buckets, so repeated clicks do not reallocate.
    m_dragStartGeometry.clear();
    for (auto* item : m_scene->selectedItems()) {
        if (auto* elem = dynamic_cast<DiagramElementItem*>(item)) {
            m_dragStartGeometry.emplace(elem->getElementId(), elem->geometry());
        }
    }
    if (auto* pressed = elementItemAt(viewPos)) {
        m_dragStartGeometry.emplace(pressed->getElementId(), pressed->geometry());
    }
    m_trackingDrag = true;
}

void DiagramView::commitGeometryTracking() {
    m_trackingDrag = false;
    std::vector<GeometryChange> changes;
    for (const auto& [id, before] : m_dragStartGeometry) {
        auto* item = elementItem(id);
        if (!item) continue;
        const auto after = item->geometry();
        if (!(after == before)) changes.push_back({id, before, after});
    }
    m_dragStartGeometry.clear();
    pushGeometryChanges(std::move(changes));
}

// ============================================================================
// Input handling
// ============================================================================

void DiagramView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        m_middleMousePanning = true;
        m_lastPanPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (m_linkingMode && event->button() == Qt::LeftButton) {
        if (auto* item = elementItemAt(event->pos())) {
            if (!m_linkSourceItem) {
                m_linkSourceItem = item;
                m_linkStartY = mapToScene(event->pos()).y();
                item->setSelected(true);
                const QString srcName = item->getElement() ? QString::fromStdString(item->getElement()->GetName()) : QStringLiteral("Origen");
                const QString toolLabel = m_linkTool ? QString::fromStdString(m_linkTool->label) : QStringLiteral("Relación");
                updateLinkBanner(QString("%1 desde \"%2\" → haz clic en el elemento DESTINO (Esc cancela)").arg(toolLabel, srcName));
                updateLinkPreview(mapToScene(event->pos()));
            } else {
                auto* source = m_linkSourceItem;
                const auto tool = m_linkTool;
                const int messageY = static_cast<int>(std::lround(m_linkStartY));
                stopLinkMode();
                if (tool && tool->relationship == ea::ConnectorType::Message && isInteractionDiagram()) {
                    addMessage(source->getElementId(), item->getElementId(),
                               static_cast<ea::sequence::MessageSort>(tool->ntype), messageY);
                } else if (tool) {
                    addConnector(source->getElementId(), item->getElementId(), tool->relationship,
                                 QString::fromStdString(tool->stereotype));
                } else {
                    createConnectorBetween(source, item); // asks for the type
                }
            }
            event->accept();
            return;
        }
        stopLinkMode(); // clicking the background cancels linking
    }

    if (event->button() == Qt::LeftButton && !m_panMode && !m_spacePressed && !itemAt(event->pos())) {
        // background: marquee selection (Ctrl adds to the current selection)
        m_marqueeActive = true;
        m_marqueeAdditive = event->modifiers() & Qt::ControlModifier;
        m_marqueeStart = mapToScene(event->pos());
        m_marqueeItem = new QGraphicsRectItem();
        m_marqueeItem->setBrush(ea::gui::withAlpha(ea::gui::m3().primary, 30));
        m_marqueeItem->setZValue(1000);
        m_scene->addItem(m_marqueeItem);
        updateMarquee(m_marqueeStart);
        event->accept();
        return;
    }

    QGraphicsView::mousePressEvent(event); // updates the selection first

    if (event->button() == Qt::LeftButton && !m_panMode && !m_spacePressed) {
        beginGeometryTracking(event->pos());
    }
}

void DiagramView::mouseMoveEvent(QMouseEvent* event) {
    if (m_diagramId > 0 && m_cursorSendTimer.elapsed() >= 40) {
        m_cursorSendTimer.restart();
        emit cursorMovedInScene(m_diagramId, mapToScene(event->pos()));
    }
    if (m_linkingMode && m_linkSourceItem) {
        updateLinkPreview(mapToScene(event->pos()));
    }
    if (m_middleMousePanning) {
        QPoint delta = event->pos() - m_lastPanPos;
        m_lastPanPos = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    if (m_marqueeActive) {
        updateMarquee(mapToScene(event->pos()));
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void DiagramView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton && m_middleMousePanning) {
        m_middleMousePanning = false;
        setCursor(m_panMode ? Qt::OpenHandCursor : Qt::ArrowCursor);
        event->accept();
        return;
    }
    if (m_marqueeActive && event->button() == Qt::LeftButton) {
        finishMarquee(mapToScene(event->pos()));
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);

    if (event->button() == Qt::LeftButton && m_trackingDrag) commitGeometryTracking();
}

void DiagramView::contextMenuEvent(QContextMenuEvent* event) {
    // If clicked on an item, let item handle it
    auto item = itemAt(event->pos());
    if (item) {
        QGraphicsView::contextMenuEvent(event);
        return;
    }

    // Background diagram context menu
    QMenu menu(this);
    auto undoAct = menu.addAction(QString("↶ Deshacer %1").arg(m_undoStack->undoText()));
    undoAct->setEnabled(m_undoStack->canUndo());
    auto redoAct = menu.addAction(QString("↷ Rehacer %1").arg(m_undoStack->redoText()));
    redoAct->setEnabled(m_undoStack->canRedo());
    menu.addSeparator();
    auto addClassAct = menu.addAction(ea::gui::M3Icons::icon("view_agenda"), "Add Class");
    auto addInterfaceAct = menu.addAction(ea::gui::M3Icons::icon("radio_button_unchecked"), "Add Interface");
    auto addNoteAct = menu.addAction(ea::gui::M3Icons::icon("sticky_note_2"), "Add Note");
    menu.addSeparator();
    auto linkAct = menu.addAction(ea::gui::M3Icons::icon("link"), "Link Elements Mode");
    menu.addSeparator();
    auto autoFitAct = menu.addAction(ea::gui::M3Icons::icon("fit_screen"), "Ajustar tamaño ideal de cajas (Auto-Fit)");
    auto autoLayoutAct = menu.addAction(ea::gui::M3Icons::icon("auto_awesome"), "Auto-Layout (Spring / Force-directed)");
    auto toggleTitleAct = menu.addAction(m_showCanvasTitle ? "Ocultar título en el lienzo" : "Mostrar título en el lienzo");
    auto saveAct = menu.addAction(ea::gui::M3Icons::icon("save"), "Save Diagram Layout");

    auto chosen = menu.exec(event->globalPos());
    if (chosen == undoAct) {
        m_undoStack->undo();
    } else if (chosen == redoAct) {
        m_undoStack->redo();
    } else if (chosen == addClassAct) {
        addNewElement(ea::ObjectType::Class, "NewClass");
    } else if (chosen == addInterfaceAct) {
        addNewElement(ea::ObjectType::Interface, "NewInterface");
    } else if (chosen == addNoteAct) {
        addNewElement(ea::ObjectType::Note, "Note");
    } else if (chosen == linkAct) {
        startLinkMode();
    } else if (chosen == autoFitAct) {
        autoFitElementSizes();
    } else if (chosen == toggleTitleAct) {
        setShowCanvasTitle(!m_showCanvasTitle);
    } else if (chosen == autoLayoutAct) {
        applyAutoLayout();
    } else if (chosen == saveAct) {
        saveDiagramLayout();
    }
}

void DiagramView::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape && m_linkingMode) {
        stopLinkMode();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_F2) {
        renameSelection();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        removeSelection();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spacePressed = true;
        setDragMode(QGraphicsView::ScrollHandDrag);
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void DiagramView::keyReleaseEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spacePressed = false;
        if (!m_panMode) {
            setDragMode(QGraphicsView::NoDrag);
            setCursor(Qt::ArrowCursor);
        }
        event->accept();
        return;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void DiagramView::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0) zoomIn();
        else zoomOut();
        event->accept();
    } else {
        QGraphicsView::wheelEvent(event);
    }
}

void DiagramView::drawBackground(QPainter* painter, const QRectF& rect) {
    // M3 surface-container-lowest: deep Polar Night canvas
    painter->fillRect(rect, ea::gui::m3().surfaceLowest);

    // Subtle dot grid in Nord 3 (outline), translucent so diagrams stay in front
    constexpr int GridDotAlpha = 150;
    painter->setPen(QPen(ea::gui::withAlpha(ea::gui::m3().outline, GridDotAlpha), 1.2));
    const int gridSize = 20;

    qreal left = int(rect.left()) - (int(rect.left()) % gridSize);
    qreal top = int(rect.top()) - (int(rect.top()) % gridSize);

    for (qreal x = left; x < rect.right(); x += gridSize) {
        for (qreal y = top; y < rect.bottom(); y += gridSize) {
            painter->drawPoint(QPointF(x, y));
        }
    }

    // Título del diagrama en el lienzo (oculto por defecto al editar para no estorbar; visible en exportación)
    if (m_showCanvasTitle && !m_diagramName.isEmpty()) {
        painter->save();
        painter->setFont(ea::gui::M3Typography::headlineMedium());
        painter->setPen(ea::gui::m3().onSurfaceVariant);
        painter->drawText(QPointF(40, 50), m_diagramName);
        painter->restore();
    }

    // Guía visual amigable cuando el lienzo está vacío
    if (m_elementItems.empty()) {
        painter->save();
        const QRectF hintRect(140, 180, 520, 86);
        painter->setPen(QPen(ea::gui::withAlpha(ea::gui::m3().outline, 140), 1.0, Qt::DashLine));
        painter->setBrush(ea::gui::withAlpha(ea::gui::m3().surfaceContainer, 150));
        painter->drawRoundedRect(hintRect, 14, 14);

        painter->setFont(ea::gui::M3Typography::titleSmall());
        painter->setPen(ea::gui::m3().onSurfaceVariant);
        painter->drawText(hintRect.adjusted(16, 14, -16, -40), Qt::AlignCenter,
                          "Lienzo vacío — Empieza a modelar");
        painter->setFont(ea::gui::M3Typography::bodyMedium());
        painter->setPen(ea::gui::m3().onSurfaceMuted);
        painter->drawText(hintRect.adjusted(16, 40, -16, -12), Qt::AlignCenter | Qt::TextWordWrap,
                          "Arrastra elementos desde la Toolbox (izquierda) o haz clic derecho aquí.");
        painter->restore();
    }
}

void DiagramView::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    if (m_linkBanner && m_linkBanner->isVisible()) {
        const int x = std::max(12, (viewport()->width() - m_linkBanner->width()) / 2);
        m_linkBanner->move(x, 12);
    }
}

void DiagramView::setShowCanvasTitle(bool show) {
    if (m_showCanvasTitle == show) return;
    m_showCanvasTitle = show;
    viewport()->update();
}

void DiagramView::autoFitElementSizes() {
    if (!m_diagram) return;
    std::vector<GeometryChange> changes;
    const QFont titleFont = ea::gui::M3Typography::titleSmall();
    const QFont monoFont = ea::gui::M3Typography::bodySmallMono();
    const QFontMetricsF titleFm(titleFont);
    const QFontMetricsF monoFm(monoFont);

    for (const auto& [id, item] : m_elementItems) {
        if (!item) continue;
        auto elem = item->getElement();
        if (!elem) continue;
        const auto t = elem->GetType();
        if (t != ea::ObjectType::Class && t != ea::ObjectType::Interface &&
            t != ea::ObjectType::Enumeration && t != ea::ObjectType::DataType &&
            t != ea::ObjectType::Component && t != ea::ObjectType::Object) {
            continue;
        }
        const QString st = QString::fromStdString(elem->GetStereotype()).trimmed().toLower();
        if ((t == ea::ObjectType::Object || t == ea::ObjectType::Class) &&
            (st == "boundary" || st == "control" || st == "entity") &&
            elem->GetAttributes().size() == 0 && elem->GetMethods().size() == 0) {
            continue; // keep robustness icons compact
        }

        const QString name = QString::fromStdString(elem->GetName());
        qreal idealW = std::max(115.0, titleFm.horizontalAdvance(name) + 28.0);
        qreal idealH = elem->GetStereotype().empty() ? 38.0 : 48.0;

        const size_t nAttrs = elem->GetAttributes().size();
        const size_t nMeths = elem->GetMethods().size();
        if (nAttrs > 0) {
            idealH += 12.0 + static_cast<qreal>(nAttrs) * 18.0;
            for (size_t i = 0; i < nAttrs; ++i) {
                if (auto a = elem->GetAttributes().GetAt(static_cast<short>(i))) {
                    QString line = QString::fromStdString(a->GetName());
                    if (!a->GetType().empty()) line += " : " + QString::fromStdString(a->GetType());
                    idealW = std::max(idealW, monoFm.horizontalAdvance(line) + 38.0);
                }
            }
        }
        if (nMeths > 0) {
            idealH += 14.0 + static_cast<qreal>(nMeths) * 18.0;
            for (size_t i = 0; i < nMeths; ++i) {
                if (auto m = elem->GetMethods().GetAt(static_cast<short>(i))) {
                    QString line = QString::fromStdString(m->GetName()) + "()";
                    if (!m->GetReturnType().empty()) line += " : " + QString::fromStdString(m->GetReturnType());
                    idealW = std::max(idealW, monoFm.horizontalAdvance(line) + 38.0);
                }
            }
        }
        idealW = std::min(idealW, 340.0);
        idealH = std::max(idealH, 64.0);

        const auto before = item->geometry();
        const int newW = std::max(before.width(), static_cast<int>(std::ceil(idealW)));
        const int newH = std::max(before.height(), static_cast<int>(std::ceil(idealH)));
        if (newW != before.width() || newH != before.height()) {
            const int dx = (newW - before.width()) / 2;
            const int dy = (newH - before.height()) / 2;
            const ElementGeometry after{before.left - dx, before.top + dy,
                                        before.left - dx + newW, before.top + dy - newH};
            changes.push_back({id, before, after});
        }
    }
    if (!changes.empty()) {
        run(std::make_shared<ea::editor::ChangeGeometryCommand>(editContext(), std::move(changes)));
        emit diagramGeometryCommitted();
    }
}

void DiagramView::exportToSvg(const QString& filePath) {
    if (!m_diagram || !m_scene) return;
    const auto savedSelection = m_scene->selectedItems();
    m_scene->clearSelection();

    QRectF bounds = m_scene->itemsBoundingRect();
    if (bounds.isEmpty()) bounds = QRectF(0, 0, 800, 600);
    const qreal margin = 36.0;
    const qreal titleBand = 46.0;
    const QRectF sourceRect = bounds.adjusted(-margin, -(margin + titleBand), margin, margin);

    QSvgGenerator generator;
    generator.setFileName(filePath);
    generator.setSize(sourceRect.size().toSize());
    generator.setViewBox(QRectF(0, 0, sourceRect.width(), sourceRect.height()));
    generator.setTitle(m_diagramName);
    generator.setDescription(QStringLiteral("Exported by onlyArchitect Studio"));

    QPainter painter(&generator);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.fillRect(QRectF(0, 0, sourceRect.width(), sourceRect.height()), ea::gui::m3().surfaceLowest);
    if (!m_diagramName.isEmpty()) {
        painter.setFont(ea::gui::M3Typography::headlineMedium());
        painter.setPen(ea::gui::m3().onSurfaceVariant);
        painter.drawText(QPointF(24.0, 34.0), m_diagramName);
    }
    m_scene->render(&painter, QRectF(0, 0, sourceRect.width(), sourceRect.height()), sourceRect);
    painter.end();

    for (auto* item : savedSelection) {
        if (item) item->setSelected(true);
    }
}

void DiagramView::exportToPng(const QString& filePath) {
    if (!m_diagram || !m_scene) return;
    const auto savedSelection = m_scene->selectedItems();
    m_scene->clearSelection();

    QRectF bounds = m_scene->itemsBoundingRect();
    if (bounds.isEmpty()) bounds = QRectF(0, 0, 800, 600);
    const qreal margin = 36.0;
    const qreal titleBand = 46.0;
    const QRectF sourceRect = bounds.adjusted(-margin, -(margin + titleBand), margin, margin);

    // 2x HiDPI crisp export (capped at 8192 px on longest edge to avoid huge allocations)
    qreal scale = 2.0;
    const qreal maxDim = std::max(sourceRect.width(), sourceRect.height());
    if (maxDim * scale > 8192.0) scale = 8192.0 / maxDim;

    const QSize pixelSize(std::max(1, static_cast<int>(std::ceil(sourceRect.width() * scale))),
                          std::max(1, static_cast<int>(std::ceil(sourceRect.height() * scale))));
    QImage image(pixelSize, QImage::Format_ARGB32);
    image.fill(ea::gui::m3().surfaceLowest);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.scale(scale, scale);

    if (!m_diagramName.isEmpty()) {
        painter.setFont(ea::gui::M3Typography::headlineMedium());
        painter.setPen(ea::gui::m3().onSurfaceVariant);
        painter.drawText(QPointF(24.0, 34.0), m_diagramName);
    }
    m_scene->render(&painter, QRectF(0, 0, sourceRect.width(), sourceRect.height()), sourceRect);
    painter.end();

    image.save(filePath, "PNG");

    for (auto* item : savedSelection) {
        if (item) item->setSelected(true);
    }
}
