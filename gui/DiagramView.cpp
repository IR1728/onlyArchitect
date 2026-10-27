#include "DiagramView.hpp"
#include "ea/diagram/DiagramRenderer.hpp"
#include "ea/diagram/DiagramLayout.hpp"
#include "ea/core/Utils.hpp"
#include <QWheelEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QPainter>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>

DiagramView::DiagramView(std::shared_ptr<ea::Diagram> diagram, ea::Repository& repo, QWidget* parent)
    : QGraphicsView(parent), m_diagram(diagram), m_repo(repo) {
    if (m_diagram) {
        m_diagramId = m_diagram->GetDiagramID();
        m_diagramName = QString::fromStdString(m_diagram->GetName());
    }

    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::TextAntialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setDragMode(QGraphicsView::NoDrag); // Allows items to be dragged smoothly!
    setBackgroundBrush(QColor(22, 25, 34)); // Dark studio theme (#161922)

    reloadDiagram();
}

void DiagramView::reloadDiagram() {
    if (!m_diagram) return;

    m_scene->clear();
    m_elementItems.clear();
    m_connectorItems.clear();

    std::vector<std::shared_ptr<ea::DiagramObject>> objects;
    for (size_t i = 0; i < m_diagram->GetDiagramObjects().size(); ++i) {
        objects.push_back(m_diagram->GetDiagramObjects().GetAt(static_cast<short>(i)));
    }

    std::vector<std::shared_ptr<ea::DiagramLink>> links;
    for (size_t i = 0; i < m_diagram->GetDiagramLinks().size(); ++i) {
        links.push_back(m_diagram->GetDiagramLinks().GetAt(static_cast<short>(i)));
    }

    // 1. Create Element Items using direct EA coordinates
    for (const auto& obj : objects) {
        if (!obj) continue;
        auto elem = m_repo.GetElementByID(obj->GetElementID());

        auto item = new DiagramElementItem(obj, elem, m_repo);
        m_scene->addItem(item);
        m_elementItems[obj->GetElementID()] = item;

        connect(item, &DiagramElementItem::elementClicked, this, &DiagramView::elementSelected);
        connect(item, &DiagramElementItem::positionModified, this, &DiagramView::diagramModified);
        connect(item, &DiagramElementItem::removeRequested, this, &DiagramView::onRemoveElementRequested);
    }

    // 2. Create Connector Items
    for (const auto& link : links) {
        if (!link || link->IsHidden()) continue;
        auto conn = m_repo.GetConnectorByID(link->GetConnectorID());
        if (!conn) continue;

        auto itStart = m_elementItems.find(conn->GetClientID());
        auto itEnd = m_elementItems.find(conn->GetSupplierID());

        if (itStart != m_elementItems.end() && itEnd != m_elementItems.end()) {
            auto connItem = new DiagramConnectorItem(link, conn, itStart->second, itEnd->second);
            m_scene->addItem(connItem);
            m_connectorItems.push_back(connItem);

            connect(connItem, &DiagramConnectorItem::removeRequested, this, &DiagramView::onRemoveConnectorRequested);
        }
    }

    // Set scene bounds
    QRectF sceneBounds = m_scene->itemsBoundingRect().adjusted(-200, -200, 400, 400);
    if (sceneBounds.width() < 1200) sceneBounds.setWidth(1200);
    if (sceneBounds.height() < 900) sceneBounds.setHeight(900);
    m_scene->setSceneRect(sceneBounds);
}

bool DiagramView::saveDiagramLayout() {
    if (!m_diagram || !m_repo.GetStorageEngine()) return false;
    return m_repo.GetStorageEngine()->SaveDiagram(*m_diagram);
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
    scale(1.2, 1.2);
    m_scaleFactor *= 1.2;
}

void DiagramView::zoomOut() {
    scale(1.0 / 1.2, 1.0 / 1.2);
    m_scaleFactor /= 1.2;
}

void DiagramView::resetZoom() {
    resetTransform();
    m_scaleFactor = 1.0;
}

void DiagramView::fitInView() {
    QRectF r = m_scene->itemsBoundingRect();
    if (!r.isEmpty()) {
        QGraphicsView::fitInView(r.adjusted(-40, -40, 40, 40), Qt::KeepAspectRatio);
    }
}

void DiagramView::addNewElement(ea::ObjectType type, const QString& defaultName) {
    if (!m_diagram) return;

    bool ok = false;
    QString title = QString("Add %1").arg(QString::fromStdString(to_string(type)));
    QString name = QInputDialog::getText(this, title, "Element Name:", QLineEdit::Normal,
                                        defaultName.isEmpty() ? QString("New%1").arg(QString::fromStdString(to_string(type))) : defaultName, &ok);
    if (!ok || name.isEmpty()) return;

    // Center in current view
    QPointF centerScene = mapToScene(viewport()->rect().center());

    // 1. Create Element in Repository
    auto pkg = m_repo.GetPackageByID(m_diagram->GetPackageID());
    auto elem = std::make_shared<ea::Element>();
    elem->SetName(name.toStdString());
    elem->SetType(type);
    elem->SetPackageID(m_diagram->GetPackageID());
    elem->SetElementGUID(ea::utils::generate_guid());

    if (m_repo.GetStorageEngine()) {
        m_repo.GetStorageEngine()->SaveElement(*elem);
    }

    // 2. Create DiagramObject
    auto dobj = std::make_shared<ea::DiagramObject>();
    dobj->SetDiagramID(m_diagram->GetDiagramID());
    dobj->SetElementID(elem->GetElementID());
    dobj->SetLeft(static_cast<int>(centerScene.x() - 85));
    dobj->SetRight(static_cast<int>(centerScene.x() + 85));
    dobj->SetTop(-static_cast<int>(centerScene.y() - 50));
    dobj->SetBottom(-static_cast<int>(centerScene.y() + 50));

    m_diagram->GetDiagramObjects().AddExisting(dobj);

    if (m_repo.GetStorageEngine()) {
        m_repo.GetStorageEngine()->SaveDiagramObject(m_diagram->GetDiagramID(), *dobj);
    }

    // 3. Add Item to Scene
    auto item = new DiagramElementItem(dobj, elem, m_repo);
    m_scene->addItem(item);
    m_elementItems[elem->GetElementID()] = item;

    connect(item, &DiagramElementItem::elementClicked, this, &DiagramView::elementSelected);
    connect(item, &DiagramElementItem::positionModified, this, &DiagramView::diagramModified);
    connect(item, &DiagramElementItem::removeRequested, this, &DiagramView::onRemoveElementRequested);

    // Select new item
    m_scene->clearSelection();
    item->setSelected(true);
    emit elementSelected(elem->GetElementID());
    emit diagramModified();
}

void DiagramView::startLinkMode() {
    m_linkingMode = true;
    m_linkSourceItem = nullptr;
    setCursor(Qt::CrossCursor);
}

void DiagramView::createConnectorBetween(DiagramElementItem* startItem, DiagramElementItem* endItem) {
    if (!startItem || !endItem || startItem == endItem) return;

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

    auto conn = std::make_shared<ea::Connector>();
    conn->SetType(ctype);
    conn->SetClientID(startItem->getElementId());
    conn->SetSupplierID(endItem->getElementId());
    conn->SetConnectorGUID(ea::utils::generate_guid());

    if (m_repo.GetStorageEngine()) {
        m_repo.GetStorageEngine()->SaveConnector(*conn);
    }

    auto dlink = std::make_shared<ea::DiagramLink>(m_diagram->GetDiagramID(), conn->GetConnectorID());
    m_diagram->GetDiagramLinks().AddExisting(dlink);

    auto connItem = new DiagramConnectorItem(dlink, conn, startItem, endItem);
    m_scene->addItem(connItem);
    m_connectorItems.push_back(connItem);

    connect(connItem, &DiagramConnectorItem::removeRequested, this, &DiagramView::onRemoveConnectorRequested);

    emit diagramModified();
}

void DiagramView::onRemoveElementRequested(int elementId) {
    auto it = m_elementItems.find(elementId);
    if (it != m_elementItems.end()) {
        auto item = it->second;

        // Remove from diagram objects
        for (short i = 0; i < m_diagram->GetDiagramObjects().Count(); ++i) {
            auto dobj = m_diagram->GetDiagramObjects().GetAt(i);
            if (dobj && dobj->GetElementID() == elementId) {
                m_diagram->GetDiagramObjects().Delete(i);
                break;
            }
        }

        // Delete from database
        if (m_repo.GetStorageEngine()) {
            std::string sql = "DELETE FROM t_diagramobjects WHERE Diagram_ID = " +
                              std::to_string(m_diagram->GetDiagramID()) +
                              " AND Object_ID = " + std::to_string(elementId) + ";";
            m_repo.GetStorageEngine()->Execute(sql);
        }

        m_scene->removeItem(item);
        m_elementItems.erase(it);
        delete item;

        emit diagramModified();
    }
}

void DiagramView::onRemoveConnectorRequested(int connectorId) {
    auto it = std::find_if(m_connectorItems.begin(), m_connectorItems.end(), [connectorId](DiagramConnectorItem* ci) {
        return ci && ci->getConnectorId() == connectorId;
    });

    if (it != m_connectorItems.end()) {
        auto item = *it;

        // Remove from diagram links
        for (short i = 0; i < m_diagram->GetDiagramLinks().Count(); ++i) {
            auto dlink = m_diagram->GetDiagramLinks().GetAt(i);
            if (dlink && dlink->GetConnectorID() == connectorId) {
                m_diagram->GetDiagramLinks().Delete(i);
                break;
            }
        }

        // Delete from database
        if (m_repo.GetStorageEngine()) {
            std::string sql = "DELETE FROM t_diagramlinks WHERE DiagramID = " +
                              std::to_string(m_diagram->GetDiagramID()) +
                              " AND ConnectorID = " + std::to_string(connectorId) + ";";
            m_repo.GetStorageEngine()->Execute(sql);
        }

        m_scene->removeItem(item);
        m_connectorItems.erase(it);
        delete item;

        emit diagramModified();
    }
}

void DiagramView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        m_middleMousePanning = true;
        m_lastPanPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (m_linkingMode && event->button() == Qt::LeftButton) {
        auto item = qgraphicsitem_cast<DiagramElementItem*>(itemAt(event->pos()));
        if (item) {
            if (!m_linkSourceItem) {
                m_linkSourceItem = item;
                item->setSelected(true);
            } else {
                createConnectorBetween(m_linkSourceItem, item);
                m_linkingMode = false;
                m_linkSourceItem = nullptr;
                setCursor(Qt::ArrowCursor);
            }
            event->accept();
            return;
        } else {
            // Cancel linking
            m_linkingMode = false;
            m_linkSourceItem = nullptr;
            setCursor(Qt::ArrowCursor);
        }
    }

    QGraphicsView::mousePressEvent(event);
}

void DiagramView::mouseMoveEvent(QMouseEvent* event) {
    if (m_middleMousePanning) {
        QPoint delta = event->pos() - m_lastPanPos;
        m_lastPanPos = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
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
    QGraphicsView::mouseReleaseEvent(event);
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
    auto addClassAct = menu.addAction("➕ Add Class");
    auto addInterfaceAct = menu.addAction("➕ Add Interface");
    auto addNoteAct = menu.addAction("➕ Add Note");
    menu.addSeparator();
    auto linkAct = menu.addAction("🔗 Link Elements Mode");
    menu.addSeparator();
    auto autoLayoutAct = menu.addAction("⚡ Auto-Layout (Spring / Force-directed)");
    auto saveAct = menu.addAction("💾 Save Diagram Layout");

    auto chosen = menu.exec(event->globalPos());
    if (chosen == addClassAct) {
        addNewElement(ea::ObjectType::Class, "NewClass");
    } else if (chosen == addInterfaceAct) {
        addNewElement(ea::ObjectType::Interface, "NewInterface");
    } else if (chosen == addNoteAct) {
        addNewElement(ea::ObjectType::Note, "Note");
    } else if (chosen == linkAct) {
        startLinkMode();
    } else if (chosen == autoLayoutAct) {
        if (m_diagram) {
            ea::DiagramLayout::Layout(*m_diagram, m_repo, ea::LayoutAlgorithm::ForceDirected);
            reloadDiagram();
            emit diagramModified();
        }
    } else if (chosen == saveAct) {
        saveDiagramLayout();
    }
}

void DiagramView::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        // Delete selected items
        auto selected = m_scene->selectedItems();
        for (auto* item : selected) {
            if (auto* elemItem = qgraphicsitem_cast<DiagramElementItem*>(item)) {
                onRemoveElementRequested(elemItem->getElementId());
            } else if (auto* connItem = qgraphicsitem_cast<DiagramConnectorItem*>(item)) {
                onRemoveConnectorRequested(connItem->getConnectorId());
            }
        }
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
    // Fondo oscuro completo estilo proto2.jpeg (#161c28)
    painter->fillRect(rect, QColor(22, 28, 40));

    // Subtle dark studio CAD dot grid pattern (#242e42)
    painter->setPen(QPen(QColor(36, 46, 66), 1.2));
    const int gridSize = 20;

    qreal left = int(rect.left()) - (int(rect.left()) % gridSize);
    qreal top = int(rect.top()) - (int(rect.top()) % gridSize);

    for (qreal x = left; x < rect.right(); x += gridSize) {
        for (qreal y = top; y < rect.bottom(); y += gridSize) {
            painter->drawPoint(QPointF(x, y));
        }
    }

    // Título prominente del Diagrama en el Canvas (como en proto2.jpeg)
    if (!m_diagramName.isEmpty()) {
        painter->save();
        QFont titleFont("Segoe UI, Inter, sans-serif", 17, QFont::Normal);
        painter->setFont(titleFont);
        painter->setPen(QColor(241, 245, 249));
        painter->drawText(QPointF(40, 50), m_diagramName);
        painter->restore();
    }
}

void DiagramView::exportToSvg(const QString& filePath) {
    if (!m_diagram) return;
    ea::DiagramRenderer renderer;
    renderer.RenderToSVGFile(*m_diagram, m_repo, filePath.toStdString());
}

void DiagramView::exportToPng(const QString& filePath) {
    if (!m_diagram) return;
    ea::DiagramRenderer renderer;
    renderer.RenderToPNGFile(*m_diagram, m_repo, filePath.toStdString());
}
