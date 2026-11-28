#include "DiagramElementItem.hpp"
#include "DarkTheme.hpp"
#include "M3Icons.hpp"
#include <QApplication>
#include <QGraphicsScene>
#include "SequenceSymbols.hpp"
#include "ea/diagram/SequenceModel.hpp"
#include "ea/core/Utils.hpp"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QStyleOptionGraphicsItem>
#include <QMenu>
#include <QCursor>
#include <QVariantAnimation>
#include "UmlSymbolPainter.hpp"
#include "ea/editor/FeatureText.hpp"

DiagramElementItem::DiagramElementItem(std::shared_ptr<ea::DiagramObject> dobj,
                                       std::shared_ptr<ea::Element> elem,
                                       ea::Repository& repo,
                                       QGraphicsItem* parent)
    : QGraphicsObject(parent), m_dobj(dobj), m_elem(elem), m_repo(repo) {

    // Initial position directly from EA coordinates:
    // EA's RectTop is negative, so on screen Y = -RectTop
    int initX = m_dobj ? m_dobj->GetLeft() : 100;
    int initY = m_dobj ? -m_dobj->GetTop() : 100;
    int initW = m_dobj ? m_dobj->GetWidth() : DefaultWidth;
    int initH = m_dobj ? m_dobj->GetHeight() : DefaultHeight;

    if (initW <= MinStoredSize) initW = DefaultWidth;
    if (initH <= MinStoredSize) initH = DefaultHeight;

    setPos(initX, initY);
    m_rect = QRectF(0, 0, initW, initH);

    // Respect EA's z-order convention (t_diagramobjects.Sequence: lower = frontmost, higher = further back)
    // and ensure containers (ActivityPartition, Boundary, large Package/Activity) always sit behind child nodes.
    const double seq = m_dobj ? static_cast<double>(m_dobj->GetSequence()) : 0.0;
    const auto elemType = m_elem ? m_elem->GetType() : ea::ObjectType::Unknown;
    if (elemType == ea::ObjectType::ActivityPartition) {
        setZValue(-200.0 - seq * 0.01);
    } else if (elemType == ea::ObjectType::Boundary) {
        setZValue(-150.0 - seq * 0.01);
    } else if (elemType == ea::ObjectType::Package && initW >= 180 && initH >= 130) {
        setZValue(-80.0 - seq * 0.01);
    } else if (elemType == ea::ObjectType::Activity && initW >= 110 && initH >= 65) {
        setZValue(-40.0 - seq * 0.01);
    } else {
        setZValue(-seq * 0.01);
    }

    setFlags(QGraphicsItem::ItemIsMovable |
             QGraphicsItem::ItemIsSelectable |
             QGraphicsItem::ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
}

int DiagramElementItem::getElementId() const {
    if (m_elem) return m_elem->GetElementID();
    if (m_dobj) return m_dobj->GetElementID();
    return 0;
}

QRectF DiagramElementItem::boundingRect() const {
    // a lifeline head may be wider than the symbol (long titles on narrow EA lifelines)
    constexpr double LifelineHeadOverflow = 60.0;
    if (m_style == SymbolStyle::Lifeline) return m_rect.adjusted(-LifelineHeadOverflow, -6, LifelineHeadOverflow, 10);
    if (m_elem) {
        const auto t = m_elem->GetType();
        if (t == ea::ObjectType::Actor) {
            return m_rect.adjusted(-55.0, -6, 55.0, 32);
        }
        if (t == ea::ObjectType::InitialNode || t == ea::ObjectType::FinalState ||
            t == ea::ObjectType::Decision || t == ea::ObjectType::MergeNode ||
            t == ea::ObjectType::ForkNode || t == ea::ObjectType::JoinNode ||
            t == ea::ObjectType::Port) {
            return m_rect.adjusted(-62.0, -6, 62.0, 62);
        }
        if (t == ea::ObjectType::Object || t == ea::ObjectType::Class) {
            const QString st = QString::fromStdString(m_elem->GetStereotype()).trimmed().toLower();
            if (st == "boundary" || st == "control" || st == "entity") {
                return m_rect.adjusted(-52.0, -8, 52.0, 62);
            }
        }
    }
    return m_rect.adjusted(-6, -6, 10, 10);
}

ea::editor::Box DiagramElementItem::sceneBox() const {
    return {pos().x(), pos().y(), pos().x() + m_rect.width(), pos().y() + m_rect.height()};
}

ea::editor::Handle DiagramElementItem::handleAt(const QPointF& p) const {
    if (!isSelected()) return ea::editor::Handle::None;
    const ea::editor::Box local{m_rect.left(), m_rect.top(), m_rect.right(), m_rect.bottom()};
    return ea::editor::handle_at(local, p.x(), p.y(), HandleSize + 2); // a little easier to grab than to see
}

bool DiagramElementItem::snapping() const {
    // the diagram decides; Alt held while dragging places freely (as in EA)
    return scene() && scene()->property("snapToGrid").toBool() && !(QApplication::keyboardModifiers() & Qt::AltModifier);
}

void DiagramElementItem::paintHandles(QPainter* painter) const {
    if (!isSelected()) return;
    const ea::editor::Box local{m_rect.left(), m_rect.top(), m_rect.right(), m_rect.bottom()};
    std::vector<QPointF> centers;
    for (const auto& [handle, b] : ea::editor::handle_boxes(local, HandleSize)) {
        centers.emplace_back((b.left + b.right) / 2.0, (b.top + b.bottom) / 2.0);
    }
    uml_symbols::paintHandles(painter, centers);
}

void DiagramElementItem::animateHover(qreal target) {
    if (!m_hoverAnim) {
        constexpr int HoverDurationMs = 200; // M3 short-4 with emphasized decelerate easing
        m_hoverAnim = new QVariantAnimation(this);
        m_hoverAnim->setDuration(HoverDurationMs);
        m_hoverAnim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_hoverAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            m_hoverScale = v.toReal();
            update();
        });
    }
    m_hoverAnim->stop();
    m_hoverAnim->setStartValue(m_hoverScale);
    m_hoverAnim->setEndValue(target);
    m_hoverAnim->start();
}

void DiagramElementItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event) {
    animateHover(1.0);
    QGraphicsObject::hoverEnterEvent(event);
}

void DiagramElementItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event) {
    animateHover(0.0);
    QGraphicsObject::hoverLeaveEvent(event);
}

QPointF DiagramElementItem::getCenter() const {
    return pos() + QPointF(m_rect.width() / 2.0, m_rect.height() / 2.0);
}

void DiagramElementItem::setRect(const QRectF& r) {
    prepareGeometryChange();
    m_rect = r;
    updateCoordinatesFromScene();
    update();
}

DiagramElementItem::~DiagramElementItem() {
    // Copy: endpointDestroyed() does not call back into removeConnector(), but stay safe
    auto connectors = m_connectors;
    m_connectors.clear();
    for (auto* conn : connectors) conn->endpointDestroyed(this);
}

void DiagramElementItem::addConnector(ConnectorEndpointObserver* conn) {
    if (conn) {
        m_connectors.push_back(conn);
    }
}

void DiagramElementItem::removeConnector(ConnectorEndpointObserver* conn) {
    auto it = std::find(m_connectors.begin(), m_connectors.end(), conn);
    if (it != m_connectors.end()) {
        m_connectors.erase(it);
    }
}

void DiagramElementItem::updateCoordinatesFromScene() {
    if (m_dobj) {
        int new_left = static_cast<int>(pos().x());
        int new_right = static_cast<int>(pos().x() + m_rect.width());
        int new_top = -static_cast<int>(pos().y());
        int new_bottom = -static_cast<int>(pos().y() + m_rect.height());

        m_dobj->SetLeft(new_left);
        m_dobj->SetRight(new_right);
        m_dobj->SetTop(new_top);
        m_dobj->SetBottom(new_bottom);

        emit positionModified();
    }
}

QVariant DiagramElementItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionChange && m_userMoving && snapping()) {
        const QPointF p = value.toPointF();
        const auto [x, y] = ea::editor::snap_position(p.x(), p.y());
        return QPointF(x, y);
    }
    if (change == ItemPositionHasChanged) {
        updateCoordinatesFromScene();
        for (auto* conn : m_connectors) {
            if (conn) conn->updatePosition();
        }
    }
    return QGraphicsObject::itemChange(change, value);
}

void DiagramElementItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event) {
    using H = ea::editor::Handle;
    switch (handleAt(event->pos())) {
        case H::TopLeft: case H::BottomRight: setCursor(Qt::SizeFDiagCursor); break;
        case H::TopRight: case H::BottomLeft: setCursor(Qt::SizeBDiagCursor); break;
        case H::Top: case H::Bottom: setCursor(Qt::SizeVerCursor); break;
        case H::Left: case H::Right: setCursor(Qt::SizeHorCursor); break;
        default: setCursor(Qt::ArrowCursor);
    }
    QGraphicsObject::hoverMoveEvent(event);
}

void DiagramElementItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        const auto handle = handleAt(event->pos());
        if (handle != ea::editor::Handle::None) {
            m_activeHandle = handle;
            m_resizeStartPos = event->scenePos();
            m_resizeStartBox = sceneBox();
            event->accept();
            return;
        }
        m_userMoving = true;
    }
    emit elementClicked(getElementId());
    QGraphicsObject::mousePressEvent(event);
}

void DiagramElementItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    if (m_activeHandle != ea::editor::Handle::None) {
        const QPointF delta = event->scenePos() - m_resizeStartPos;
        const auto box = ea::editor::resize_box(m_resizeStartBox, m_activeHandle, delta.x(), delta.y(),
                                                ea::editor::MinSymbolSize, snapping() ? ea::editor::GridSize : 0);
        prepareGeometryChange();
        m_rect = QRectF(0, 0, box.width(), box.height());
        setPos(box.left, box.top); // updates the DiagramObject and the connectors
        updateCoordinatesFromScene();
        for (auto* conn : m_connectors) {
            if (conn) conn->updatePosition();
        }
        update();
        event->accept();
        return;
    }
    QGraphicsObject::mouseMoveEvent(event);
}

void DiagramElementItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    m_activeHandle = ea::editor::Handle::None;
    m_userMoving = false;
    updateCoordinatesFromScene();
    QGraphicsObject::mouseReleaseEvent(event);
}

ea::editor::ElementGeometry DiagramElementItem::geometry() const {
    const int left = static_cast<int>(pos().x());
    const int top = -static_cast<int>(pos().y());
    return {left, top, left + static_cast<int>(m_rect.width()), top - static_cast<int>(m_rect.height())};
}

void DiagramElementItem::applyGeometry(const ea::editor::ElementGeometry& g) {
    // Size first: setPos() triggers itemChange(), which writes pos + m_rect back to the DiagramObject
    prepareGeometryChange();
    m_rect = QRectF(0, 0, g.width(), g.height());
    setPos(g.left, -g.top);
    updateCoordinatesFromScene();
    for (auto* conn : m_connectors) {
        if (conn) conn->updatePosition();
    }
    update();
}

void DiagramElementItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    if (m_elem) emit editRequested(getElementId());
    event->accept();
}

void DiagramElementItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event) {
    QMenu menu;
    auto editAct = menu.addAction(ea::gui::M3Icons::icon("edit"), "Editar elemento...");
    QAction* refineAct = nullptr;
    QAction* addAttrAct = nullptr;
    QAction* addMethAct = nullptr;

    const auto kind = m_elem ? m_elem->GetType() : ea::ObjectType::Unknown;
    if (kind == ea::ObjectType::UseCase) {
        refineAct = menu.addAction(ea::gui::M3Icons::icon("sync_alt"), "Refinar con Diagrama de Secuencia...");
    }
    if (kind == ea::ObjectType::Class || kind == ea::ObjectType::Interface ||
        kind == ea::ObjectType::Component || kind == ea::ObjectType::Enumeration ||
        kind == ea::ObjectType::DataType || kind == ea::ObjectType::Object ||
        kind == ea::ObjectType::Lifeline) {
        menu.addSeparator();
        addAttrAct = menu.addAction(ea::gui::M3Icons::icon("add"), "Añadir atributo...");
        addMethAct = menu.addAction(ea::gui::M3Icons::icon("add"), "Añadir operación / método...");
    }
    menu.addSeparator();
    auto removeAct = menu.addAction(ea::gui::M3Icons::icon("delete"), "Quitar del diagrama");

    auto selectedAct = menu.exec(event->screenPos());
    if (selectedAct == editAct) {
        if (m_elem) emit editRequested(getElementId());
    } else if (refineAct && selectedAct == refineAct) {
        if (m_elem) emit refineToSequenceRequested(getElementId());
    } else if (addAttrAct && selectedAct == addAttrAct) {
        if (m_elem) emit addAttributeRequested(getElementId());
    } else if (addMethAct && selectedAct == addMethAct) {
        if (m_elem) emit addOperationRequested(getElementId());
    } else if (selectedAct == removeAct) {
        emit removeRequested(getElementId());
    }
}

void DiagramElementItem::setSymbolStyle(SymbolStyle style) {
    if (m_style == style) return;
    prepareGeometryChange(); // the bounding rect depends on the style
    m_style = style;
    const double seq = m_dobj ? static_cast<double>(m_dobj->GetSequence()) : 0.0;
    if (style == SymbolStyle::Frame) {
        setZValue(-20.0 - seq * 0.01); // frames lie behind lifelines and messages
    } else if (style == SymbolStyle::Lifeline) {
        setZValue(-seq * 0.01);
    }
    update();
}

void DiagramElementItem::setActivations(std::vector<Activation> activations) {
    m_activations = std::move(activations);
    update();
}

bool DiagramElementItem::paintInteractionSymbol(QPainter* painter, bool selected) {
    if (m_style == SymbolStyle::Standard || !m_elem) return false;
    const QString name = QString::fromStdString(m_elem->GetName());
    const auto type = m_elem->GetType();
    if (m_style == SymbolStyle::Frame) {
        if (type == ea::ObjectType::InteractionOccurrence) {
            sequence_symbols::paintInteractionUse(painter, m_rect, name, selected);
        } else {
            sequence_symbols::paintCombinedFragment(
                painter, m_rect, QString::fromStdString(ea::sequence::fragment_operator(m_elem->GetNType())), name, selected);
        }
        return true;
    }
    // Lifeline title: "name : Classifier" style is EA's; the stereotype shows as «boundary» etc.
    QString title = name.isEmpty() ? QString(":%1").arg(QString::fromStdString(to_string(type))) : name;
    if (!m_elem->GetStereotype().empty()) title = QString("«%1»\n%2").arg(QString::fromStdString(m_elem->GetStereotype()), title);
    std::vector<sequence_symbols::ActivationBar> bars;
    bars.reserve(m_activations.size());
    for (const auto& a : m_activations) bars.push_back({a.top - pos().y(), a.bottom - pos().y(), a.depth});
    sequence_symbols::paintLifeline(painter, m_rect, title,
                                    type == ea::ObjectType::Actor ? sequence_symbols::LifelineHead::Actor
                                                                  : sequence_symbols::LifelineHead::Box,
                                    bars, selected);
    return true;
}

namespace {

std::vector<uml_symbols::FeatureLine> attributeLines(const ea::Element& elem) {
    std::vector<uml_symbols::FeatureLine> lines;
    const auto& attrs = elem.GetAttributes();
    lines.reserve(attrs.size());
    for (size_t i = 0; i < attrs.size(); ++i) {
        if (auto a = attrs.GetAt(static_cast<short>(i))) {
            lines.push_back({a->GetVisibility(), QString::fromStdString(a->GetName()), QString::fromStdString(a->GetType())});
        }
    }
    return lines;
}

std::vector<uml_symbols::FeatureLine> operationLines(const ea::Element& elem) {
    std::vector<uml_symbols::FeatureLine> lines;
    const auto& methods = elem.GetMethods();
    lines.reserve(methods.size());
    for (size_t i = 0; i < methods.size(); ++i) {
        if (auto m = methods.GetAt(static_cast<short>(i))) {
            const QString signature = QString("%1(%2)").arg(
                QString::fromStdString(m->GetName()),
                QString::fromStdString(ea::editor::format_parameters(m->GetParameters())));
            lines.push_back({m->GetVisibility(), signature, QString::fromStdString(m->GetReturnType())});
        }
    }
    return lines;
}

} // namespace

uml_symbols::SymbolContent DiagramElementItem::symbolContent() const {
    uml_symbols::SymbolContent c;
    if (!m_elem) {
        c.name = QString("ID: %1").arg(getElementId());
        return c;
    }
    c.type = m_elem->GetType();
    c.ntype = m_elem->GetNType();
    c.name = QString::fromStdString(m_elem->GetName());
    c.stereotype = QString::fromStdString(m_elem->GetStereotype());
    if (c.type == ea::ObjectType::Interface && c.stereotype.isEmpty()) c.stereotype = QStringLiteral("interface");
    if (c.type == ea::ObjectType::Note) {
        c.notes = QString::fromStdString(m_elem->GetNotes());
        return c;
    }
    c.attributes = attributeLines(*m_elem);
    c.operations = operationLines(*m_elem);
    return c;
}

void DiagramElementItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* /*widget*/) {
    painter->save();
    paintSymbol(painter, option);
    painter->restore();
    paintHandles(painter);
}

void DiagramElementItem::paintSymbol(QPainter* painter, const QStyleOptionGraphicsItem* option) {
    painter->setRenderHint(QPainter::Antialiasing);
    const bool selected = option->state & QStyle::State_Selected;
    if (paintInteractionSymbol(painter, selected)) return;

    const auto content = symbolContent();
    uml_symbols::paintSymbol(painter, m_rect, content);
    uml_symbols::paintSelectionGlow(painter, m_rect, content.type, selected, m_hoverScale);
}
