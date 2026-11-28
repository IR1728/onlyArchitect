#include "WhiteboardWidget.hpp"
#include "DarkTheme.hpp"
#include "M3Icons.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <cmath>
#include <numbers>

namespace ea {

static QPair<QColor, QColor> getNoteColors(NoteColor c) {
    switch (c) {
    case NoteColor::Yellow:
        return {ea::gui::m3().warning, ea::gui::m3().surfaceLowest}; // Amarillo cálido, texto oscuro
    case NoteColor::Pink:
        return {ea::gui::m3().error, ea::gui::m3().errorContainer}; // Rosa pastel
    case NoteColor::Green:
        return {ea::gui::m3().success, ea::gui::m3().successContainer};  // Menta / verde suave
    case NoteColor::Cyan:
        return {ea::gui::m3().primary, ea::gui::m3().primaryContainer}; // Cian / cielo
    case NoteColor::Purple:
        return {ea::gui::m3().tertiary, ea::gui::m3().tertiaryContainer}; // Lavanda / púrpura
    }
    return {ea::gui::m3().warning, ea::gui::m3().surfaceLowest};
}

static std::string colorToStereotype(NoteColor c) {
    switch (c) {
        case NoteColor::Pink: return "wb_pink";
        case NoteColor::Green: return "wb_green";
        case NoteColor::Cyan: return "wb_cyan";
        case NoteColor::Purple: return "wb_purple";
        default: return "wb_yellow";
    }
}

static NoteColor stereotypeToColor(const std::string& st) {
    if (st == "wb_pink") return NoteColor::Pink;
    if (st == "wb_green") return NoteColor::Green;
    if (st == "wb_cyan") return NoteColor::Cyan;
    if (st == "wb_purple") return NoteColor::Purple;
    return NoteColor::Yellow;
}

StickyNoteItem::StickyNoteItem(const QString& title, const QString& content, const QString& author, NoteColor color,
                               const QPointF& pos, int elementId,
                               std::function<void(int, const QString&, const QPointF&)> onChanged)
    : m_elementId(elementId), m_title(title), m_content(content), m_author(author), m_color(color),
      m_rect(0, 0, 170, 140), m_onChanged(std::move(onChanged)) {
    setPos(pos);
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges);
}

QRectF StickyNoteItem::boundingRect() const {
    return m_rect.adjusted(-6, -6, 12, 12);
}

void StickyNoteItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/) {
    painter->setRenderHint(QPainter::Antialiasing);

    auto [bg, fg] = getNoteColors(m_color);

    // Sombra suave
    painter->setBrush(ea::gui::withAlpha(ea::gui::m3().surfaceLowest, 50));
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(m_rect.translated(4, 5), 6.0, 6.0);

    // Cuerpo nota
    painter->setBrush(bg);
    if (isSelected()) {
        painter->setPen(QPen(ea::gui::m3().primary, 2.5));
    } else {
        painter->setPen(QPen(bg.darker(115), 1));
    }
    painter->drawRoundedRect(m_rect, 6.0, 6.0);

    // Pin / Cinta superior decorativa
    painter->setBrush(ea::gui::withAlpha(ea::gui::m3().onSurface, 120));
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(QRectF(m_rect.left() + (m_rect.width() - 40) / 2.0, m_rect.top() - 4, 40, 8), 2.0, 2.0);

    // Título y Autor
    painter->setPen(fg);
    QFont titleFont = painter->font();
    titleFont.setBold(true);
    titleFont.setPointSize(10);
    painter->setFont(titleFont);
    painter->drawText(QRectF(m_rect.left() + 10, m_rect.top() + 10, m_rect.width() - 20, 20), Qt::AlignLeft | Qt::AlignVCenter, m_title);

    // Contenido
    QFont bodyFont = painter->font();
    bodyFont.setBold(false);
    bodyFont.setPointSize(9);
    painter->setFont(bodyFont);
    QRectF bodyRect(m_rect.left() + 10, m_rect.top() + 32, m_rect.width() - 20, m_rect.height() - 58);
    painter->drawText(bodyRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, m_content);

    // Autor en la esquina inferior
    QFont authFont = painter->font();
    authFont.setItalic(true);
    authFont.setPointSize(7.5);
    painter->setFont(authFont);
    painter->setPen(fg.lighter(130));
    QRectF authRect(m_rect.left() + 10, m_rect.bottom() - 20, m_rect.width() - 20, 16);
    painter->drawText(authRect, Qt::AlignRight | Qt::AlignVCenter, m_author);
}

void StickyNoteItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    bool ok = false;
    QString newText = QInputDialog::getMultiLineText(nullptr, QString::fromUtf8("Editar Nota"), QString::fromUtf8("Contenido:"), m_content, &ok);
    if (ok && !newText.isEmpty()) {
        setContent(newText);
        if (m_onChanged && m_elementId > 0) m_onChanged(m_elementId, m_content, pos());
    }
    event->accept();
}

QVariant StickyNoteItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionHasChanged && m_onChanged && m_elementId > 0) {
        m_onChanged(m_elementId, m_content, value.toPointF());
    }
    return QGraphicsItem::itemChange(change, value);
}

// ---------------------------------------------------------------------------
// WhiteboardConnectionItem
// ---------------------------------------------------------------------------

WhiteboardConnectionItem::WhiteboardConnectionItem(QGraphicsItem* startItem, QGraphicsItem* endItem, const QString& label)
    : m_start(startItem), m_end(endItem), m_label(label) {
    setZValue(-1);
}

QRectF WhiteboardConnectionItem::boundingRect() const {
    if (!m_start || !m_end) return QRectF();
    QPointF p1 = m_start->scenePos() + QPointF(85, 70);
    QPointF p2 = m_end->scenePos() + QPointF(85, 70);
    return QRectF(p1, p2).normalized().adjusted(-30, -30, 30, 30);
}

void WhiteboardConnectionItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/) {
    if (!m_start || !m_end) return;

    painter->setRenderHint(QPainter::Antialiasing);

    QPointF p1 = m_start->scenePos() + QPointF(85, 70);
    QPointF p2 = m_end->scenePos() + QPointF(85, 70);

    QPen pen(ea::gui::withAlpha(ea::gui::m3().onSurfaceMuted, 180), 2, Qt::DashLine);
    painter->setPen(pen);

    QPainterPath path;
    path.moveTo(p1);
    QPointF ctrl1 = p1 + QPointF((p2.x() - p1.x()) / 2.0, 0);
    QPointF ctrl2 = p2 - QPointF((p2.x() - p1.x()) / 2.0, 0);
    path.cubicTo(ctrl1, ctrl2, p2);
    painter->drawPath(path);

    // Flecha
    qreal angle = std::atan2(p2.y() - ctrl2.y(), p2.x() - ctrl2.x());
    qreal arrowSize = 10;
    QPointF a1 = p2 - QPointF(std::sin(angle + std::numbers::pi / 3.0) * arrowSize, std::cos(angle + std::numbers::pi / 3.0) * arrowSize);
    QPointF a2 = p2 - QPointF(std::sin(angle + std::numbers::pi - std::numbers::pi / 3.0) * arrowSize, std::cos(angle + std::numbers::pi - std::numbers::pi / 3.0) * arrowSize);
    QPolygonF head;
    head << p2 << a1 << a2;
    painter->setBrush(ea::gui::m3().onSurfaceMuted);
    painter->drawPolygon(head);

    if (!m_label.isEmpty()) {
        QPointF mid = path.pointAtPercent(0.5);
        painter->setPen(ea::gui::m3().onSurface);
        painter->drawText(mid + QPointF(-20, -6), m_label);
    }
}

// ---------------------------------------------------------------------------
// WhiteboardWidget
// ---------------------------------------------------------------------------

WhiteboardWidget::WhiteboardWidget(ea::Repository& repo, QWidget* parent)
    : QWidget(parent), m_repo(repo) {
    setupUI();
    refreshFromRepository();
}

void WhiteboardWidget::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // Barra de herramientas superior estilo FigJam / Miro
    auto* toolbar = new QWidget(this);
    toolbar->setObjectName("WhiteboardToolbar");
    ea::gui::setThemedStyleSheet(toolbar,
        "#WhiteboardToolbar { background-color: @surfaceLow; border: 1px solid @outlineVariant; border-radius: 8px; padding: 6px; }"
    );
    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(8, 4, 8, 4);
    tbLayout->setSpacing(8);

    auto* titleLabel = new QLabel(QString::fromUtf8("<b>MODO BOCETO & PIZARRA DE COLABORACIÓN</b>"), toolbar);
    ea::gui::setThemedStyleSheet(titleLabel, "color: @warning; font-size: 13px;");
    tbLayout->addWidget(titleLabel);

    tbLayout->addSpacing(12);

    auto* btnYellow = new QPushButton(QString::fromUtf8("Post-it Amarillo"), toolbar);
    ea::gui::setThemedStyleSheet(btnYellow, "background-color: @warning; color: @onPrimary; font-weight: 600; border-radius: 16px; padding: 6px 16px;");
    btnYellow->setIcon(ea::gui::M3Icons::icon("sticky_note_2", ea::gui::m3().onPrimary));
    connect(btnYellow, &QPushButton::clicked, this, [this]() { addStickyNote(NoteColor::Yellow); });
    tbLayout->addWidget(btnYellow);

    auto* btnPink = new QPushButton(QString::fromUtf8("Rosa"), toolbar);
    ea::gui::setThemedStyleSheet(btnPink, "background-color: @error; color: @onPrimary; font-weight: 600; border-radius: 16px; padding: 6px 16px;");
    btnPink->setIcon(ea::gui::M3Icons::icon("sticky_note_2", ea::gui::m3().onPrimary));
    connect(btnPink, &QPushButton::clicked, this, [this]() { addStickyNote(NoteColor::Pink); });
    tbLayout->addWidget(btnPink);

    auto* btnGreen = new QPushButton(QString::fromUtf8("Menta"), toolbar);
    ea::gui::setThemedStyleSheet(btnGreen, "background-color: @success; color: @onPrimary; font-weight: 600; border-radius: 16px; padding: 6px 16px;");
    btnGreen->setIcon(ea::gui::M3Icons::icon("sticky_note_2", ea::gui::m3().onPrimary));
    connect(btnGreen, &QPushButton::clicked, this, [this]() { addStickyNote(NoteColor::Green); });
    tbLayout->addWidget(btnGreen);

    auto* btnCyan = new QPushButton(QString::fromUtf8("Cian"), toolbar);
    ea::gui::setThemedStyleSheet(btnCyan, "background-color: @primary; color: @onPrimary; font-weight: 600; border-radius: 16px; padding: 6px 16px;");
    btnCyan->setIcon(ea::gui::M3Icons::icon("sticky_note_2", ea::gui::m3().onPrimary));
    connect(btnCyan, &QPushButton::clicked, this, [this]() { addStickyNote(NoteColor::Cyan); });
    tbLayout->addWidget(btnCyan);

    auto* btnPurple = new QPushButton(QString::fromUtf8("Lavanda"), toolbar);
    ea::gui::setThemedStyleSheet(btnPurple, "background-color: @tertiary; color: @onPrimary; font-weight: 600; border-radius: 16px; padding: 6px 16px;");
    btnPurple->setIcon(ea::gui::M3Icons::icon("sticky_note_2", ea::gui::m3().onPrimary));
    connect(btnPurple, &QPushButton::clicked, this, [this]() { addStickyNote(NoteColor::Purple); });
    tbLayout->addWidget(btnPurple);

    tbLayout->addSpacing(12);

    auto* btnIdea = new QPushButton(QString::fromUtf8("Tarjeta de Idea"), toolbar);
    ea::gui::setThemedStyleSheet(btnIdea, "background-color: @surfaceContainer; color: @primary; border: 1px solid @outline; border-radius: 16px; padding: 6px 16px; font-weight: 600;");
    ea::gui::M3Icons::decorate(btnIdea, "lightbulb");
    connect(btnIdea, &QPushButton::clicked, this, &WhiteboardWidget::addIdeaCard);
    tbLayout->addWidget(btnIdea);

    tbLayout->addStretch();

    auto* btnExport = new QPushButton(QString::fromUtf8("Exportar Imagen"), toolbar);
    ea::gui::setThemedStyleSheet(btnExport, "background-color: @primaryStrong; color: white; border-radius: 16px; padding: 6px 16px; font-weight: 600;");
    ea::gui::M3Icons::decorate(btnExport, "save");
    connect(btnExport, &QPushButton::clicked, this, &WhiteboardWidget::exportToImage);
    tbLayout->addWidget(btnExport);

    auto* btnClear = new QPushButton(QString::fromUtf8("Limpiar"), toolbar);
    ea::gui::setThemedStyleSheet(btnClear, "background-color: @surfaceContainer; color: @error; border: 1px solid @outline; border-radius: 16px; padding: 6px 16px;");
    ea::gui::M3Icons::decorate(btnClear, "cleaning_services");
    connect(btnClear, &QPushButton::clicked, this, &WhiteboardWidget::clearWhiteboard);
    tbLayout->addWidget(btnClear);

    mainLayout->addWidget(toolbar);

    // Canvas de la pizarra
    m_scene = new QGraphicsScene(this);
    m_scene->setSceneRect(0, 0, 2400, 1600);
    m_scene->setBackgroundBrush(QBrush(ea::gui::m3().surfaceLowest));

    m_graphicsView = new QGraphicsView(m_scene, this);
    m_graphicsView->setRenderHint(QPainter::Antialiasing);
    m_graphicsView->setDragMode(QGraphicsView::RubberBandDrag);
    ea::gui::setThemedStyleSheet(m_graphicsView, "background-color: @surfaceLowest; border: 1px solid @outlineVariant; border-radius: 8px;");
    mainLayout->addWidget(m_graphicsView);
}

std::shared_ptr<ea::Diagram> WhiteboardWidget::ensureWhiteboardDiagram(int& outPackageId) {
    outPackageId = 0;
    if (!m_repo.IsOpen()) return nullptr;

    std::shared_ptr<ea::Package> wbPkg;
    std::function<void(const ea::Package&)> findPkg = [&](const ea::Package& pkg) {
        if (wbPkg) return;
        if (pkg.GetName() == "Whiteboard") {
            wbPkg = m_repo.GetPackageByID(pkg.GetPackageID());
            return;
        }
        for (size_t i = 0; i < pkg.GetPackages().size(); ++i) {
            if (auto sub = pkg.GetPackages().GetAt(static_cast<short>(i))) findPkg(*sub);
        }
    };
    for (size_t i = 0; i < m_repo.GetModels().size(); ++i) {
        if (auto m = m_repo.GetModels().GetAt(static_cast<short>(i))) findPkg(*m);
    }

    if (!wbPkg) {
        if (m_repo.GetModels().size() > 0 && m_repo.GetModels().GetAt(0)) {
            wbPkg = m_repo.CreatePackage(m_repo.GetModels().GetAt(0)->GetPackageID(), "Whiteboard");
        } else {
            wbPkg = m_repo.CreateModel("Whiteboard");
        }
    }
    if (!wbPkg) return nullptr;
    outPackageId = wbPkg->GetPackageID();

    if (!wbPkg->GetDiagrams().empty()) {
        if (auto d = wbPkg->GetDiagrams().GetAt(0)) {
            m_whiteboardDiagramId = d->GetDiagramID();
            return d;
        }
    }
    auto created = m_repo.CreateDiagram(outPackageId, "Whiteboard", DiagramType::Custom);
    if (created) m_whiteboardDiagramId = created->GetDiagramID();
    return created;
}

void WhiteboardWidget::persistNoteChange(int elementId, const QString& content, const QPointF& pos) {
    if (!m_repo.IsOpen() || elementId <= 0) return;
    if (auto elem = m_repo.GetElementByID(elementId)) {
        elem->SetNotes(content.toStdString());
        m_repo.SaveElement(*elem);
    }
    if (m_whiteboardDiagramId > 0) {
        if (auto diag = m_repo.GetDiagramByID(m_whiteboardDiagramId)) {
            for (const auto& dobj : diag->GetDiagramObjects()) {
                if (dobj && dobj->GetElementID() == elementId) {
                    dobj->SetLeft(static_cast<int>(pos.x()));
                    dobj->SetTop(-static_cast<int>(pos.y()));
                    dobj->SetRight(static_cast<int>(pos.x()) + 170);
                    dobj->SetBottom(-(static_cast<int>(pos.y()) + 140));
                    m_repo.SaveDiagramObject(m_whiteboardDiagramId, *dobj);
                    break;
                }
            }
        }
    }
}

void WhiteboardWidget::refreshFromRepository() {
    m_scene->clear();
    m_whiteboardDiagramId = 0;

    if (!m_repo.IsOpen()) {
        auto* hint = m_scene->addText(QString::fromUtf8("Abre o crea un repositorio (.qea) para guardar tus notas de la pizarra."));
        hint->setDefaultTextColor(ea::gui::m3().onSurfaceMuted);
        hint->setPos(60, 60);
        return;
    }

    // Buscar si existe un diagrama "Whiteboard" en el repositorio sin crear paquetes innecesarios al solo ver
    std::shared_ptr<ea::Diagram> wbDiag;
    std::function<void(const ea::Package&)> findDiag = [&](const ea::Package& pkg) {
        if (wbDiag) return;
        if (pkg.GetName() == "Whiteboard" && !pkg.GetDiagrams().empty()) {
            wbDiag = pkg.GetDiagrams().GetAt(0);
            return;
        }
        for (size_t i = 0; i < pkg.GetPackages().size(); ++i) {
            if (auto sub = pkg.GetPackages().GetAt(static_cast<short>(i))) findDiag(*sub);
        }
    };
    for (size_t i = 0; i < m_repo.GetModels().size(); ++i) {
        if (auto m = m_repo.GetModels().GetAt(static_cast<short>(i))) findDiag(*m);
    }

    if (!wbDiag || wbDiag->GetDiagramObjects().empty()) {
        auto* hint = m_scene->addText(QString::fromUtf8("Pizarra vacía. Usa los botones superiores para añadir post-its (se guardan automáticamente en el .qea)."));
        hint->setDefaultTextColor(ea::gui::m3().onSurfaceMuted);
        hint->setPos(60, 60);
        return;
    }

    m_whiteboardDiagramId = wbDiag->GetDiagramID();
    for (const auto& dobj : wbDiag->GetDiagramObjects()) {
        if (!dobj) continue;
        auto elem = m_repo.GetElementByID(dobj->GetElementID());
        if (!elem) continue;
        auto* note = new StickyNoteItem(
            QString::fromStdString(elem->GetName()),
            QString::fromStdString(elem->GetNotes()),
            QString::fromStdString(elem->GetAuthor().empty() ? "Usuario" : elem->GetAuthor()),
            stereotypeToColor(elem->GetStereotype()),
            QPointF(dobj->GetLeft(), -dobj->GetTop()),
            elem->GetElementID(),
            [this](int id, const QString& text, const QPointF& p) { persistNoteChange(id, text, p); });
        m_scene->addItem(note);
    }
}

void WhiteboardWidget::addStickyNote(NoteColor color) {
    bool ok = false;
    QString title = QInputDialog::getText(this, QString::fromUtf8("Nueva Nota"), QString::fromUtf8("Título:"), QLineEdit::Normal, QString::fromUtf8("Nueva Idea"), &ok);
    if (!ok || title.isEmpty()) return;

    QString content = QInputDialog::getMultiLineText(this, QString::fromUtf8("Nueva Nota"), QString::fromUtf8("Contenido:"), QString::fromUtf8("Escribe tu nota aquí... (Doble clic para editar)"), &ok);
    if (!ok) return;

    // Colocar en el centro visible
    QPointF center = m_graphicsView->mapToScene(m_graphicsView->viewport()->rect().center()) - QPointF(85, 70);
    int elementId = 0;

    if (m_repo.IsOpen()) {
        int pkgId = 0;
        if (auto diag = ensureWhiteboardDiagram(pkgId)) {
            Element e;
            e.SetName(title.toStdString());
            e.SetNotes(content.toStdString());
            e.SetType(ObjectType::Note);
            e.SetPackageID(pkgId);
            e.SetStereotype(colorToStereotype(color));
            e.SetAuthor("Usuario");
            if (auto saved = m_repo.AddElement(e)) {
                elementId = saved->GetElementID();
                auto dobj = std::make_shared<DiagramObject>();
                dobj->SetDiagramID(diag->GetDiagramID());
                dobj->SetElementID(elementId);
                dobj->SetLeft(static_cast<int>(center.x()));
                dobj->SetTop(-static_cast<int>(center.y()));
                dobj->SetRight(static_cast<int>(center.x()) + 170);
                dobj->SetBottom(-(static_cast<int>(center.y()) + 140));
                m_repo.AddElementToDiagram(*diag, dobj);
            }
        }
    }

    auto* note = new StickyNoteItem(
        title, content, "Usuario", color, center, elementId,
        [this](int id, const QString& text, const QPointF& p) { persistNoteChange(id, text, p); });
    m_scene->addItem(note);
}

void WhiteboardWidget::addIdeaCard() {
    addStickyNote(NoteColor::Purple);
}

void WhiteboardWidget::clearWhiteboard() {
    auto reply = QMessageBox::question(this, QString::fromUtf8("Limpiar Pizarra"),
                                       QString::fromUtf8("¿Deseas vaciar todas las notas de la pizarra?"),
                                       QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        m_scene->clear();
    }
}

void WhiteboardWidget::exportToImage() {
    QString fileName = QFileDialog::getSaveFileName(this, QString::fromUtf8("Guardar Pizarra como Imagen"),
                                                    "pizarra_boceto.png",
                                                    "Imágenes PNG (*.png);;Imágenes JPEG (*.jpg)");
    if (fileName.isEmpty()) return;

    QRectF sceneRect = m_scene->itemsBoundingRect().adjusted(-40, -40, 40, 40);
    QImage image(sceneRect.size().toSize(), QImage::Format_ARGB32);
    image.fill(ea::gui::m3().surfaceLowest);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    m_scene->render(&painter, QRectF(), sceneRect);

    if (image.save(fileName)) {
        QMessageBox::information(this, QString::fromUtf8("Exportación Exitosa"),
                                 QString::fromUtf8("La pizarra se ha exportado correctamente a:\n%1").arg(fileName));
    }
}

} // namespace ea
