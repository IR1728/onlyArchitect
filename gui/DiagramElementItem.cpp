#include "DiagramElementItem.hpp"
#include "DiagramConnectorItem.hpp"
#include "EditElementDialog.hpp"
#include "ea/core/Utils.hpp"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QStyleOptionGraphicsItem>
#include <QMenu>
#include <QCursor>
#include <QInputDialog>

DiagramElementItem::DiagramElementItem(std::shared_ptr<ea::DiagramObject> dobj,
                                       std::shared_ptr<ea::Element> elem,
                                       ea::Repository& repo,
                                       QGraphicsItem* parent)
    : QGraphicsObject(parent), m_dobj(dobj), m_elem(elem), m_repo(repo) {

    // Initial position directly from EA coordinates:
    // EA's RectTop is negative, so on screen Y = -RectTop
    int initX = m_dobj ? m_dobj->GetLeft() : 100;
    int initY = m_dobj ? -m_dobj->GetTop() : 100;
    int initW = m_dobj ? m_dobj->GetWidth() : 170;
    int initH = m_dobj ? m_dobj->GetHeight() : 100;

    if (initW <= 40) initW = 170;
    if (initH <= 40) initH = 100;

    setPos(initX, initY);
    m_rect = QRectF(0, 0, initW, initH);

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
    return m_rect.adjusted(-6, -6, 10, 10);
}

QRectF DiagramElementItem::resizeGripRect() const {
    return QRectF(m_rect.right() - 10, m_rect.bottom() - 10, 10, 10);
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

void DiagramElementItem::addConnector(DiagramConnectorItem* conn) {
    if (conn) {
        m_connectors.push_back(conn);
    }
}

void DiagramElementItem::removeConnector(DiagramConnectorItem* conn) {
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
    if (change == ItemPositionHasChanged) {
        updateCoordinatesFromScene();
        for (auto* conn : m_connectors) {
            if (conn) conn->updatePosition();
        }
    }
    return QGraphicsObject::itemChange(change, value);
}

void DiagramElementItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event) {
    if (resizeGripRect().contains(event->pos())) {
        setCursor(Qt::SizeFDiagCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
    QGraphicsObject::hoverMoveEvent(event);
}

void DiagramElementItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (resizeGripRect().contains(event->pos())) {
            m_isResizing = true;
            m_resizeStartPos = event->scenePos();
            m_resizeStartSize = m_rect.size();
            event->accept();
            return;
        }
    }
    emit elementClicked(getElementId());
    QGraphicsObject::mousePressEvent(event);
}

void DiagramElementItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    if (m_isResizing) {
        QPointF delta = event->scenePos() - m_resizeStartPos;
        double newW = std::max(80.0, m_resizeStartSize.width() + delta.x());
        double newH = std::max(50.0, m_resizeStartSize.height() + delta.y());

        prepareGeometryChange();
        m_rect.setSize(QSizeF(newW, newH));
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
    m_isResizing = false;
    updateCoordinatesFromScene();
    QGraphicsObject::mouseReleaseEvent(event);
}

void DiagramElementItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    if (m_elem) {
        EditElementDialog dlg(m_elem, m_repo);
        if (dlg.exec() == QDialog::Accepted && dlg.isModified()) {
            update();
            emit elementClicked(getElementId());
            emit positionModified();
        }
    }
    event->accept();
}

void DiagramElementItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event) {
    QMenu menu;
    auto editAct = menu.addAction("✏️ Edit Element...");
    menu.addSeparator();
    auto addAttrAct = menu.addAction("➕ Add Attribute...");
    auto addMethAct = menu.addAction("➕ Add Operation / Method...");
    menu.addSeparator();
    auto removeAct = menu.addAction("🗑️ Remove from Diagram");

    auto selectedAct = menu.exec(event->screenPos());
    if (selectedAct == editAct) {
        if (m_elem) {
            EditElementDialog dlg(m_elem, m_repo);
            if (dlg.exec() == QDialog::Accepted && dlg.isModified()) {
                update();
                emit elementClicked(getElementId());
                emit positionModified();
            }
        }
    } else if (selectedAct == addAttrAct) {
        if (m_elem) {
            bool ok = false;
            QString name = QInputDialog::getText(nullptr, "Add Attribute", "Attribute name:", QLineEdit::Normal, "newAttr", &ok);
            if (ok && !name.isEmpty()) {
                m_elem->GetAttributes().AddNew(name.toStdString(), "int");
                if (m_repo.GetStorageEngine()) m_repo.GetStorageEngine()->SaveElement(*m_elem);
                update();
                emit elementClicked(getElementId());
                emit positionModified();
            }
        }
    } else if (selectedAct == addMethAct) {
        if (m_elem) {
            bool ok = false;
            QString name = QInputDialog::getText(nullptr, "Add Operation", "Operation name:", QLineEdit::Normal, "newOperation", &ok);
            if (ok && !name.isEmpty()) {
                m_elem->GetMethods().AddNew(name.toStdString(), "void");
                if (m_repo.GetStorageEngine()) m_repo.GetStorageEngine()->SaveElement(*m_elem);
                update();
                emit elementClicked(getElementId());
                emit positionModified();
            }
        }
    } else if (selectedAct == removeAct) {
        emit removeRequested(getElementId());
    }
}

void DiagramElementItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* /*widget*/) {
    painter->setRenderHint(QPainter::Antialiasing);

    bool isSelected = option->state & QStyle::State_Selected;

    // Sombra suave oscura
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0, 0, 0, 70));
    painter->drawRoundedRect(m_rect.translated(4, 5), 6.0, 6.0);

    // Paleta oscura moderna según tipo de elemento (basada en el prototipo)
    QColor bg(34, 39, 51);        // #222733
    QColor headerBg(26, 30, 40);  // #1a1e28
    QColor border(56, 66, 82);     // #384252
    QColor accent(56, 189, 248);   // #38bdf8

    ea::ObjectType elemType = ea::ObjectType::Class;
    if (m_elem) {
        elemType = m_elem->GetType();
        if (elemType == ea::ObjectType::Interface) {
            bg = QColor(28, 38, 54);
            border = QColor(30, 144, 255);
        } else if (elemType == ea::ObjectType::Enumeration) {
            bg = QColor(36, 35, 48);
            border = QColor(168, 85, 247);
        } else if (elemType == ea::ObjectType::Package) {
            bg = QColor(38, 36, 32);
            border = QColor(245, 158, 11);
        } else if (elemType == ea::ObjectType::Note) {
            bg = QColor(44, 40, 26);
            border = QColor(234, 179, 8);
        } else if (elemType == ea::ObjectType::UseCase) {
            bg = QColor(28, 36, 48);
            border = QColor(56, 189, 248);
        } else if (elemType == ea::ObjectType::Actor) {
            bg = QColor(24, 30, 42);
            border = QColor(16, 185, 129);
        } else if (elemType == ea::ObjectType::Component) {
            bg = QColor(36, 68, 92);       // #24445c Azul Componente proto2.jpeg
            border = QColor(56, 189, 248);  // #38bdf8
        } else if (elemType == ea::ObjectType::Port) {
            bg = QColor(40, 50, 68);
            border = QColor(56, 189, 248);
        }
    }

    if (isSelected) {
        border = QColor(56, 189, 248); // Borde cyan activo de selección
    }

    // Renderizado según tipo de elemento
    if (elemType == ea::ObjectType::Component) {
        // Componente UML (Fiel a proto2.jpeg)
        painter->setBrush(QBrush(bg));
        painter->setPen(QPen(border, isSelected ? 2.5 : 1.5));
        painter->drawRoundedRect(m_rect, 4.0, 4.0);

        // Icono canónico de Componente UML en la esquina superior derecha
        double iconRight = m_rect.right() - 8;
        double iconTop = m_rect.top() + 8;
        QRectF compIconRect(iconRight - 16, iconTop, 16, 20);
        painter->setBrush(QBrush(bg.lighter(120)));
        painter->setPen(QPen(border, 1.2));
        painter->drawRect(compIconRect);

        // Dos lengüetas salientes a la izquierda del icono
        painter->drawRect(QRectF(compIconRect.left() - 4, compIconRect.top() + 3, 7, 4));
        painter->drawRect(QRectF(compIconRect.left() - 4, compIconRect.top() + 11, 7, 4));

        // Estereotipo opcional
        std::string st = m_elem ? m_elem->GetStereotype() : "";
        if (!st.empty()) {
            QFont stFont("sans-serif", 8);
            stFont.setItalic(true);
            painter->setFont(stFont);
            painter->setPen(QColor(148, 163, 184));
            painter->drawText(QRectF(m_rect.left(), m_rect.top() + 8, m_rect.width() - 28, 14),
                              Qt::AlignCenter, QString("«%1»").arg(QString::fromStdString(st)));
        }

        // Nombre del componente centrado en blanco
        std::string name = m_elem ? m_elem->GetName() : ("Component " + std::to_string(getElementId()));
        QFont nameFont("sans-serif", 10, QFont::Bold);
        painter->setFont(nameFont);
        painter->setPen(QColor(241, 245, 249));
        QRectF nameRect = m_rect.adjusted(10, st.empty() ? 10 : 22, -26, -10);
        painter->drawText(nameRect, Qt::AlignCenter | Qt::TextWordWrap, QString::fromStdString(name));

    } else if (elemType == ea::ObjectType::Port) {
        // Puerto UML (pequeño cuadro con etiqueta)
        painter->setBrush(QBrush(bg));
        painter->setPen(QPen(border, isSelected ? 2.5 : 1.5));
        painter->drawRect(m_rect);

        std::string name = m_elem ? m_elem->GetName() : "";
        if (!name.empty()) {
            painter->setFont(QFont("sans-serif", 8));
            painter->setPen(QColor(241, 245, 249));
            painter->drawText(QRectF(m_rect.left() - 20, m_rect.bottom() + 2, m_rect.width() + 40, 16),
                              Qt::AlignCenter, QString::fromStdString(name));
        }

    } else if (elemType == ea::ObjectType::UseCase) {
        // Forma Elipse de Caso de Uso
        painter->setBrush(QBrush(bg));
        painter->setPen(QPen(border, isSelected ? 2.5 : 1.5));
        painter->drawEllipse(m_rect);

        // Estereotipo
        std::string st = m_elem ? m_elem->GetStereotype() : "";
        std::string name = m_elem ? m_elem->GetName() : ("UC " + std::to_string(getElementId()));

        QFont stFont("sans-serif", 8);
        stFont.setItalic(true);
        painter->setFont(stFont);
        painter->setPen(QColor(148, 163, 184));
        if (!st.empty()) {
            painter->drawText(QRectF(m_rect.left(), m_rect.top() + 10, m_rect.width(), 16),
                              Qt::AlignCenter, QString("«%1»").arg(QString::fromStdString(st)));
        }

        QFont nameFont("sans-serif", 9, QFont::Bold);
        painter->setFont(nameFont);
        painter->setPen(QColor(241, 245, 249));
        QRectF nameRect = m_rect.adjusted(12, st.empty() ? 12 : 24, -12, -12);
        painter->drawText(nameRect, Qt::AlignCenter | Qt::TextWordWrap, QString::fromStdString(name));

    } else if (elemType == ea::ObjectType::Actor) {
        // Renderizado de Actor UML
        painter->setBrush(QBrush(bg));
        painter->setPen(QPen(border, isSelected ? 2.5 : 1.5));
        painter->drawRoundedRect(m_rect, 8.0, 8.0);

        // Stickman decorativo centrado arriba
        double centerX = m_rect.center().x();
        double headY = m_rect.top() + 18;
        painter->setPen(QPen(QColor(56, 189, 248), 2.0));
        painter->setBrush(QBrush(QColor(24, 30, 42)));
        painter->drawEllipse(QPointF(centerX, headY), 9, 9); // Cabeza
        painter->drawLine(QPointF(centerX, headY + 9), QPointF(centerX, headY + 28)); // Cuerpo
        painter->drawLine(QPointF(centerX - 12, headY + 16), QPointF(centerX + 12, headY + 16)); // Brazos
        painter->drawLine(QPointF(centerX, headY + 28), QPointF(centerX - 10, headY + 42)); // Pierna izq
        painter->drawLine(QPointF(centerX, headY + 28), QPointF(centerX + 10, headY + 42)); // Pierna der

        // Nombre del actor
        std::string name = m_elem ? m_elem->GetName() : ("Actor " + std::to_string(getElementId()));
        QFont nameFont("sans-serif", 9, QFont::Bold);
        painter->setFont(nameFont);
        painter->setPen(QColor(241, 245, 249));
        QRectF nameRect(m_rect.left() + 4, headY + 46, m_rect.width() - 8, m_rect.height() - (headY + 48 - m_rect.top()));
        painter->drawText(nameRect, Qt::AlignCenter | Qt::TextWordWrap, QString::fromStdString(name));

    } else {
        // Clase, Interfaz, Enumeración, Componente o Nota
        // Fondo general
        painter->setBrush(bg);
        painter->setPen(QPen(border, isSelected ? 2.5 : 1.2));
        painter->drawRoundedRect(m_rect, 6.0, 6.0);

        // Cabecera superior oscura contrastada
        double headerHeight = 36.0;
        std::string stereotype = m_elem ? m_elem->GetStereotype() : "";
        if (!stereotype.empty() || elemType == ea::ObjectType::Interface) {
            headerHeight = 44.0;
        }

        QRectF headerRect(m_rect.left(), m_rect.top(), m_rect.width(), headerHeight);
        QPainterPath headerPath;
        headerPath.addRoundedRect(headerRect, 6.0, 6.0);
        // Recortar la parte inferior para que sea recta
        painter->save();
        painter->setClipRect(headerRect);
        painter->setBrush(headerBg);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(headerRect.adjusted(0, 0, 0, 8), 6.0, 6.0);
        painter->restore();

        // Línea divisoria de cabecera
        painter->setPen(QPen(border, 1.0));
        painter->drawLine(QPointF(m_rect.left(), m_rect.top() + headerHeight),
                          QPointF(m_rect.right(), m_rect.top() + headerHeight));

        // Texto cabecera: estereotipo
        double textY = m_rect.top() + 6;
        if (elemType == ea::ObjectType::Interface && stereotype.empty()) {
            stereotype = "interface";
        }

        if (!stereotype.empty()) {
            QFont stFont("sans-serif", 8);
            stFont.setItalic(true);
            painter->setFont(stFont);
            painter->setPen(QColor(56, 189, 248)); // Cian estereotipo
            painter->drawText(QRectF(m_rect.left(), textY, m_rect.width(), 14),
                              Qt::AlignCenter, QString("«%1»").arg(QString::fromStdString(stereotype)));
            textY += 15.0;
        }

        // Nombre de la clase / elemento
        std::string name = m_elem ? m_elem->GetName() : ("ID: " + std::to_string(getElementId()));
        QFont nameFont("sans-serif", 9, QFont::Bold);
        painter->setFont(nameFont);
        painter->setPen(QColor(241, 245, 249)); // Blanco plateado
        painter->drawText(QRectF(m_rect.left() + 6, textY, m_rect.width() - 12, 16),
                          Qt::AlignCenter, QString::fromStdString(name));

        double curY = m_rect.top() + headerHeight + 14.0;

        // Compartimento de Atributos
        if (m_elem && elemType != ea::ObjectType::Note) {
            QFont codeFont("monospace", 8.5);
            painter->setFont(codeFont);

            for (size_t i = 0; i < m_elem->GetAttributes().size(); ++i) {
                if (curY > m_rect.bottom() - 22) break;
                auto attr = m_elem->GetAttributes().GetAt(static_cast<short>(i));
                if (!attr) continue;

                // Color según visibilidad (+ verde, - rojo, # ámbar, ~ violeta)
                QString vis = "+";
                QColor visColor(16, 185, 129); // + verde
                if (attr->GetVisibility() == ea::Scope::Private) {
                    vis = "-";
                    visColor = QColor(244, 63, 94); // - rojo
                } else if (attr->GetVisibility() == ea::Scope::Protected) {
                    vis = "#";
                    visColor = QColor(245, 158, 11); // # ámbar
                } else if (attr->GetVisibility() == ea::Scope::Package) {
                    vis = "~";
                    visColor = QColor(168, 85, 247); // ~ violeta
                }

                // Dibujar visibilidad
                painter->setPen(visColor);
                painter->drawText(QPointF(m_rect.left() + 8, curY), vis);

                // Dibujar nombre de atributo en blanco
                painter->setPen(QColor(226, 232, 240));
                QString attrName = QString::fromStdString(attr->GetName());
                painter->drawText(QPointF(m_rect.left() + 20, curY), attrName);

                // Dibujar tipo en cian
                QFontMetrics fm(codeFont);
                int nameWidth = fm.horizontalAdvance(attrName);
                painter->setPen(QColor(148, 163, 184));
                painter->drawText(QPointF(m_rect.left() + 22 + nameWidth, curY), ":");

                painter->setPen(QColor(56, 189, 248));
                painter->drawText(QPointF(m_rect.left() + 30 + nameWidth, curY), QString::fromStdString(attr->GetType()));

                curY += 15.0;
            }

            // Compartimento de Métodos / Operaciones
            if (!m_elem->GetMethods().empty() && curY < m_rect.bottom() - 18) {
                // Separador entre atributos y métodos
                painter->setPen(QPen(border, 1.0));
                painter->drawLine(QPointF(m_rect.left(), curY - 3), QPointF(m_rect.right(), curY - 3));
                curY += 12.0;

                for (size_t i = 0; i < m_elem->GetMethods().size(); ++i) {
                    if (curY > m_rect.bottom() - 10) break;
                    auto meth = m_elem->GetMethods().GetAt(static_cast<short>(i));
                    if (!meth) continue;

                    QString vis = "+";
                    QColor visColor(16, 185, 129);
                    if (meth->GetVisibility() == ea::Scope::Private) {
                        vis = "-";
                        visColor = QColor(244, 63, 94);
                    } else if (meth->GetVisibility() == ea::Scope::Protected) {
                        vis = "#";
                        visColor = QColor(245, 158, 11);
                    } else if (meth->GetVisibility() == ea::Scope::Package) {
                        vis = "~";
                        visColor = QColor(168, 85, 247);
                    }

                    painter->setPen(visColor);
                    painter->drawText(QPointF(m_rect.left() + 8, curY), vis);

                    painter->setPen(QColor(226, 232, 240));
                    QString methSignature = QString("%1()").arg(QString::fromStdString(meth->GetName()));
                    painter->drawText(QPointF(m_rect.left() + 20, curY), methSignature);

                    QFontMetrics fm(codeFont);
                    int sigWidth = fm.horizontalAdvance(methSignature);
                    painter->setPen(QColor(148, 163, 184));
                    painter->drawText(QPointF(m_rect.left() + 22 + sigWidth, curY), ":");

                    painter->setPen(QColor(56, 189, 248));
                    painter->drawText(QPointF(m_rect.left() + 30 + sigWidth, curY), QString::fromStdString(meth->GetReturnType()));

                    curY += 15.0;
                }
            }
        } else if (m_elem && elemType == ea::ObjectType::Note) {
            // Nota de texto explicativo
            painter->setPen(QColor(241, 245, 249));
            QFont noteFont("sans-serif", 8.5);
            painter->setFont(noteFont);
            QRectF noteRect(m_rect.left() + 8, m_rect.top() + 8, m_rect.width() - 16, m_rect.height() - 16);
            painter->drawText(noteRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                              QString::fromStdString(m_elem->GetNotes()));
        }
    }

    // Handles de selección activos
    if (isSelected) {
        painter->setBrush(QColor(56, 189, 248));
        painter->setPen(Qt::white);
        double s = 6.0;
        painter->drawRect(QRectF(m_rect.left() - s/2, m_rect.top() - s/2, s, s));
        painter->drawRect(QRectF(m_rect.right() - s/2, m_rect.top() - s/2, s, s));
        painter->drawRect(QRectF(m_rect.left() - s/2, m_rect.bottom() - s/2, s, s));
        painter->drawRect(QRectF(m_rect.right() - s/2, m_rect.bottom() - s/2, s, s));

        // Resize grip dots
        painter->setPen(QPen(QColor(148, 163, 184), 1.5));
        painter->drawLine(m_rect.right() - 8, m_rect.bottom() - 2, m_rect.right() - 2, m_rect.bottom() - 8);
        painter->drawLine(m_rect.right() - 5, m_rect.bottom() - 2, m_rect.right() - 2, m_rect.bottom() - 5);
    }
}
