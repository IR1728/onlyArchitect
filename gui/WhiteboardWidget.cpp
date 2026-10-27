#include "WhiteboardWidget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <cmath>

namespace ea {

static QPair<QColor, QColor> getNoteColors(NoteColor c) {
    switch (c) {
    case NoteColor::Yellow:
        return {QColor(254, 240, 138), QColor(28, 25, 23)}; // Amarillo cálido, texto oscuro
    case NoteColor::Pink:
        return {QColor(251, 207, 232), QColor(131, 24, 67)}; // Rosa pastel
    case NoteColor::Green:
        return {QColor(187, 247, 208), QColor(6, 78, 59)};  // Menta / verde suave
    case NoteColor::Cyan:
        return {QColor(186, 230, 253), QColor(12, 74, 110)}; // Cian / cielo
    case NoteColor::Purple:
        return {QColor(233, 213, 255), QColor(76, 29, 149)}; // Lavanda / púrpura
    }
    return {QColor(254, 240, 138), QColor(28, 25, 23)};
}

StickyNoteItem::StickyNoteItem(const QString& title, const QString& content, const QString& author, NoteColor color, const QPointF& pos)
    : m_title(title), m_content(content), m_author(author), m_color(color), m_rect(0, 0, 170, 140) {
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
    painter->setBrush(QColor(0, 0, 0, 50));
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(m_rect.translated(4, 5), 6.0, 6.0);

    // Cuerpo nota
    painter->setBrush(bg);
    if (isSelected()) {
        painter->setPen(QPen(QColor(56, 189, 248), 2.5));
    } else {
        painter->setPen(QPen(bg.darker(115), 1));
    }
    painter->drawRoundedRect(m_rect, 6.0, 6.0);

    // Pin / Cinta superior decorativa
    painter->setBrush(QColor(255, 255, 255, 120));
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
    painter->drawText(authRect, Qt::AlignRight | Qt::AlignVCenter, QString("✍️ %1").arg(m_author));
}

void StickyNoteItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    bool ok = false;
    QString newText = QInputDialog::getMultiLineText(nullptr, QString::fromUtf8("Editar Nota"), QString::fromUtf8("Contenido:"), m_content, &ok);
    if (ok && !newText.isEmpty()) {
        setContent(newText);
    }
    event->accept();
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

    QPen pen(QColor(148, 163, 184, 180), 2, Qt::DashLine);
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
    QPointF a1 = p2 - QPointF(sin(angle + M_PI / 3) * arrowSize, cos(angle + M_PI / 3) * arrowSize);
    QPointF a2 = p2 - QPointF(sin(angle + M_PI - M_PI / 3) * arrowSize, cos(angle + M_PI - M_PI / 3) * arrowSize);
    QPolygonF head;
    head << p2 << a1 << a2;
    painter->setBrush(QColor(148, 163, 184));
    painter->drawPolygon(head);

    if (!m_label.isEmpty()) {
        QPointF mid = path.pointAtPercent(0.5);
        painter->setPen(QColor(226, 232, 240));
        painter->drawText(mid + QPointF(-20, -6), m_label);
    }
}

// ---------------------------------------------------------------------------
// WhiteboardWidget
// ---------------------------------------------------------------------------

WhiteboardWidget::WhiteboardWidget(ea::Repository& repo, QWidget* parent)
    : QWidget(parent), m_repo(repo) {
    setupUI();
    createSampleBoard();
}

void WhiteboardWidget::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // Barra de herramientas superior estilo FigJam / Miro
    auto* toolbar = new QWidget(this);
    toolbar->setObjectName("WhiteboardToolbar");
    toolbar->setStyleSheet(
        "#WhiteboardToolbar { background-color: #1e222d; border: 1px solid #2b3140; border-radius: 8px; padding: 6px; }"
    );
    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(8, 4, 8, 4);
    tbLayout->setSpacing(8);

    auto* titleLabel = new QLabel(QString::fromUtf8("<b>📝 MODO BOCETO & PIZARRA DE COLABORACIÓN</b>"), toolbar);
    titleLabel->setStyleSheet("color: #f59e0b; font-size: 13px;");
    tbLayout->addWidget(titleLabel);

    tbLayout->addSpacing(12);

    auto* btnYellow = new QPushButton(QString::fromUtf8("🟡 Post-it Amarillo"), toolbar);
    btnYellow->setStyleSheet("background-color: #fef08a; color: #1c1917; font-weight: bold; border-radius: 4px; padding: 6px 12px;");
    connect(btnYellow, &QPushButton::clicked, this, [this]() { addStickyNote(NoteColor::Yellow); });
    tbLayout->addWidget(btnYellow);

    auto* btnPink = new QPushButton(QString::fromUtf8("🌸 Rosa"), toolbar);
    btnPink->setStyleSheet("background-color: #fbcfe8; color: #831843; font-weight: bold; border-radius: 4px; padding: 6px 12px;");
    connect(btnPink, &QPushButton::clicked, this, [this]() { addStickyNote(NoteColor::Pink); });
    tbLayout->addWidget(btnPink);

    auto* btnGreen = new QPushButton(QString::fromUtf8("🌱 Menta"), toolbar);
    btnGreen->setStyleSheet("background-color: #bbf7d0; color: #064e3b; font-weight: bold; border-radius: 4px; padding: 6px 12px;");
    connect(btnGreen, &QPushButton::clicked, this, [this]() { addStickyNote(NoteColor::Green); });
    tbLayout->addWidget(btnGreen);

    auto* btnCyan = new QPushButton(QString::fromUtf8("🌊 Cian"), toolbar);
    btnCyan->setStyleSheet("background-color: #bae6fd; color: #0c4a6e; font-weight: bold; border-radius: 4px; padding: 6px 12px;");
    connect(btnCyan, &QPushButton::clicked, this, [this]() { addStickyNote(NoteColor::Cyan); });
    tbLayout->addWidget(btnCyan);

    auto* btnPurple = new QPushButton(QString::fromUtf8("🍇 Lavanda"), toolbar);
    btnPurple->setStyleSheet("background-color: #e9d5ff; color: #4c1d95; font-weight: bold; border-radius: 4px; padding: 6px 12px;");
    connect(btnPurple, &QPushButton::clicked, this, [this]() { addStickyNote(NoteColor::Purple); });
    tbLayout->addWidget(btnPurple);

    tbLayout->addSpacing(12);

    auto* btnIdea = new QPushButton(QString::fromUtf8("💡 Tarjeta de Idea"), toolbar);
    btnIdea->setStyleSheet("background-color: #242936; color: #38bdf8; border: 1px solid #384252; border-radius: 4px; padding: 6px 12px; font-weight: bold;");
    connect(btnIdea, &QPushButton::clicked, this, &WhiteboardWidget::addIdeaCard);
    tbLayout->addWidget(btnIdea);

    tbLayout->addStretch();

    auto* btnExport = new QPushButton(QString::fromUtf8("💾 Exportar Imagen"), toolbar);
    btnExport->setStyleSheet("background-color: #007acc; color: white; border-radius: 4px; padding: 6px 12px; font-weight: bold;");
    connect(btnExport, &QPushButton::clicked, this, &WhiteboardWidget::exportToImage);
    tbLayout->addWidget(btnExport);

    auto* btnClear = new QPushButton(QString::fromUtf8("🧹 Limpiar"), toolbar);
    btnClear->setStyleSheet("background-color: #242936; color: #f43f5e; border: 1px solid #384252; border-radius: 4px; padding: 6px 10px;");
    connect(btnClear, &QPushButton::clicked, this, &WhiteboardWidget::clearWhiteboard);
    tbLayout->addWidget(btnClear);

    mainLayout->addWidget(toolbar);

    // Canvas de la pizarra
    m_scene = new QGraphicsScene(this);
    m_scene->setSceneRect(0, 0, 2400, 1600);
    m_scene->setBackgroundBrush(QBrush(QColor(19, 22, 30)));

    m_graphicsView = new QGraphicsView(m_scene, this);
    m_graphicsView->setRenderHint(QPainter::Antialiasing);
    m_graphicsView->setDragMode(QGraphicsView::RubberBandDrag);
    m_graphicsView->setStyleSheet("background-color: #13161e; border: 1px solid #2b3140; border-radius: 8px;");
    mainLayout->addWidget(m_graphicsView);
}

void WhiteboardWidget::createSampleBoard() {
    m_scene->clear();

    // Notas de ejemplo tipo lluvia de ideas de equipo
    auto* n1 = new StickyNoteItem(
        QString::fromUtf8("Arquitectura v2"),
        QString::fromUtf8("Migrar de motor COM legado a arquitectura desacoplada C++20 con backend SQLite/DuckDB."),
        "Iran (Lead Dev)",
        NoteColor::Yellow,
        QPointF(80, 80)
    );

    auto* n2 = new StickyNoteItem(
        QString::fromUtf8("Compatibilidad EAPX"),
        QString::fromUtf8("Inyectar exportador MDB a SQLite usando mdbtools compilado estáticamente. Funciona sin sudo!"),
        "Backend Team",
        NoteColor::Green,
        QPointF(320, 80)
    );

    auto* n3 = new StickyNoteItem(
        QString::fromUtf8("Diseño UI Prototipo"),
        QString::fromUtf8("8 vistas temáticas oscuras con navegación centralizada y soporte de simulación interactiva."),
        "UI/UX Designer",
        NoteColor::Cyan,
        QPointF(560, 80)
    );

    auto* n4 = new StickyNoteItem(
        QString::fromUtf8("Pruebas Unitarias"),
        QString::fromUtf8("Añadir suite automatizada para validación de cardinalidad y renderizado de conectores UML."),
        "QA Specialist",
        NoteColor::Pink,
        QPointF(320, 280)
    );

    auto* n5 = new StickyNoteItem(
        QString::fromUtf8("Generación Código"),
        QString::fromUtf8("Soportar exportación en tiempo real para C++, Python, Java y esquemas SQL DDL."),
        "Iran (Lead Dev)",
        NoteColor::Purple,
        QPointF(80, 280)
    );

    m_scene->addItem(n1);
    m_scene->addItem(n2);
    m_scene->addItem(n3);
    m_scene->addItem(n4);
    m_scene->addItem(n5);

    // Conexiones de ejemplo
    m_scene->addItem(new WhiteboardConnectionItem(n1, n2, QString::fromUtf8("motor datos")));
    m_scene->addItem(new WhiteboardConnectionItem(n1, n5, QString::fromUtf8("ast export")));
    m_scene->addItem(new WhiteboardConnectionItem(n2, n3, QString::fromUtf8("modelo")));
    m_scene->addItem(new WhiteboardConnectionItem(n2, n4, QString::fromUtf8("regresión")));
}

void WhiteboardWidget::addStickyNote(NoteColor color) {
    bool ok = false;
    QString title = QInputDialog::getText(this, QString::fromUtf8("Nueva Nota"), QString::fromUtf8("Título:"), QLineEdit::Normal, QString::fromUtf8("Nueva Idea"), &ok);
    if (!ok || title.isEmpty()) return;

    QString content = QInputDialog::getMultiLineText(this, QString::fromUtf8("Nueva Nota"), QString::fromUtf8("Contenido:"), QString::fromUtf8("Escribe tu nota aquí... (Doble clic para editar)"), &ok);
    if (!ok) return;

    // Colocar en el centro visible
    QPointF center = m_graphicsView->mapToScene(m_graphicsView->viewport()->rect().center());
    auto* note = new StickyNoteItem(title, content, "Usuario", color, center - QPointF(85, 70));
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
    image.fill(QColor(19, 22, 30));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    m_scene->render(&painter, QRectF(), sceneRect);

    if (image.save(fileName)) {
        QMessageBox::information(this, QString::fromUtf8("Exportación Exitosa"),
                                 QString::fromUtf8("La pizarra se ha exportado correctamente a:\n%1").arg(fileName));
    }
}

} // namespace ea
