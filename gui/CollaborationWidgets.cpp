#include "CollaborationWidgets.hpp"
#include "DarkTheme.hpp"
#include "M3Icons.hpp"
#include "ea/core/Utils.hpp"
#include "ea/desktop/CollaborationService.hpp"
#include <QDate>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QPainter>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <algorithm>

namespace ea::gui {

namespace {

const QStringList kKanbanStatuses = {"Proposed", "Approved", "Implemented", "Validated"};
const QStringList kKanbanTitles = {"Propuesto", "Aprobado", "Implementado", "Validado"};

QColor peerAccentColor(const QString& name) {
    static const QColor kPalette[] = {
        QColor("#88C0D0"), QColor("#A3BE8C"), QColor("#EBCB8B"),
        QColor("#D08770"), QColor("#B48EAD"), QColor("#81A1C1")
    };
    return kPalette[qHash(name) % (sizeof(kPalette) / sizeof(kPalette[0]))];
}

// Custom delegate that draws a clean M3 horizontal timeline bar for each Gantt task
class GanttBarDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyledItemDelegate::paint(painter, option, index);

        const int startDay = index.data(Qt::UserRole + 1).toInt();
        const int duration = std::max(1, index.data(Qt::UserRole + 2).toInt());
        const int progress = std::clamp(index.data(Qt::UserRole + 3).toInt(), 0, 100);
        const int totalSpan = std::max(14, index.data(Qt::UserRole + 4).toInt());

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        const QRectF r = option.rect.adjusted(6, 5, -6, -5);
        // Subtle day guide lines
        painter->setPen(QPen(withAlpha(m3().outline, 70), 1.0));
        const int step = totalSpan > 30 ? 5 : 2;
        for (int d = step; d < totalSpan; d += step) {
            const qreal x = r.left() + (r.width() * d) / totalSpan;
            painter->drawLine(QPointF(x, option.rect.top()), QPointF(x, option.rect.bottom()));
        }

        const qreal x0 = r.left() + (r.width() * std::max(0, startDay - 1)) / totalSpan;
        const qreal w = std::max<qreal>(14.0, (r.width() * duration) / totalSpan);
        const QRectF barRect(x0, r.top(), std::min(w, r.right() - x0), r.height());

        const QColor trackColor = progress >= 100 ? withAlpha(m3().success, 90) : withAlpha(m3().primary, 75);
        const QColor fillColor = progress >= 100 ? m3().success : m3().primary;

        painter->setPen(Qt::NoPen);
        painter->setBrush(trackColor);
        painter->drawRoundedRect(barRect, 6, 6);

        if (progress > 0) {
            QRectF progRect = barRect;
            progRect.setWidth(barRect.width() * (progress / 100.0));
            painter->setBrush(fillColor);
            painter->drawRoundedRect(progRect, 6, 6);
        }

        painter->setPen(m3().onSurface);
        QFont f = option.font;
        f.setPointSizeF(8.5);
        f.setBold(true);
        painter->setFont(f);
        painter->drawText(barRect.adjusted(4, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft,
                          QString("D%1-%2 (%3%)").arg(startDay).arg(startDay + duration - 1).arg(progress));
        painter->restore();
    }
};

} // namespace

// ============================================================================
// TeamChatWidget
// ============================================================================

TeamChatWidget::TeamChatWidget(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    auto* headerRow = new QHBoxLayout();
    m_statusLabel = new QLabel("Desconectado — Conéctate a LAN/VPN para chatear en tiempo real.", this);
    m_statusLabel->setWordWrap(true);
    setThemedStyleSheet(m_statusLabel, "color: @onSurfaceMuted; font-size: 11px;");
    headerRow->addWidget(m_statusLabel, 1);

    m_connectBtn = new QPushButton("Conectar LAN", this);
    M3Icons::decorate(m_connectBtn, "dns", 15);
    setThemedStyleSheet(m_connectBtn,
        "QPushButton { background-color: @primaryContainer; color: @onPrimaryContainer; border: 1px solid @primary; border-radius: 10px; padding: 4px 10px; font-size: 11px; font-weight: 600; }"
        "QPushButton:hover { background-color: @primary; color: @onPrimary; }"
    );
    connect(m_connectBtn, &QPushButton::clicked, this, &TeamChatWidget::connectServerRequested);
    headerRow->addWidget(m_connectBtn);
    root->addLayout(headerRow);

    m_transcript = new QTextBrowser(this);
    m_transcript->setOpenExternalLinks(false);
    setThemedStyleSheet(m_transcript,
        "QTextBrowser { background-color: @surfaceLow; color: @onSurface; border: 1px solid @outlineVariant; border-radius: 10px; padding: 6px; font-size: 12px; }"
    );
    root->addWidget(m_transcript, 1);

    auto* inputRow = new QHBoxLayout();
    inputRow->setSpacing(6);
    m_input = new QLineEdit(this);
    m_input->setPlaceholderText("Escribe un mensaje para el equipo...");
    connect(m_input, &QLineEdit::returnPressed, this, &TeamChatWidget::onSendClicked);
    inputRow->addWidget(m_input, 1);

    m_sendBtn = new QPushButton("Enviar", this);
    M3Icons::decorate(m_sendBtn, "send", 16);
    setThemedStyleSheet(m_sendBtn,
        "QPushButton { background-color: @primaryStrong; color: white; border: none; border-radius: 10px; padding: 6px 12px; font-size: 11.5px; font-weight: 600; }"
        "QPushButton:hover { background-color: @primary; color: @onPrimary; }"
        "QPushButton:disabled { background-color: @surfaceContainer; color: @onSurfaceDisabled; }"
    );
    connect(m_sendBtn, &QPushButton::clicked, this, &TeamChatWidget::onSendClicked);
    inputRow->addWidget(m_sendBtn);
    root->addLayout(inputRow);

    setConnectionState(false, {}, QString());
}

void TeamChatWidget::setConnectionState(bool connected, const QStringList& peers, const QString& localUser) {
    m_connected = connected;
    m_localUser = localUser;
    m_input->setEnabled(connected);
    m_sendBtn->setEnabled(connected);
    m_connectBtn->setVisible(!connected);

    if (connected) {
        m_statusLabel->setText(QString("En línea (%1): %2")
                                   .arg(std::max<qsizetype>(1, peers.size()))
                                   .arg(peers.isEmpty() ? localUser : peers.join(", ")));
        setThemedStyleSheet(m_statusLabel, "color: @success; font-size: 11px; font-weight: 600;");
    } else {
        m_statusLabel->setText("Chat offline — Conéctate a un servidor LAN/VPN para hablar con tu equipo.");
        setThemedStyleSheet(m_statusLabel, "color: @onSurfaceMuted; font-size: 11px;");
    }
}

void TeamChatWidget::appendChatMessage(const QString& author, const QString& text, qint64 timestampMs) {
    const QString timeStr = QDateTime::fromMSecsSinceEpoch(timestampMs).toString("HH:mm");
    const QColor accent = peerAccentColor(author);
    const QString safeAuthor = author.toHtmlEscaped();
    const QString safeText = text.toHtmlEscaped().replace('\n', "<br/>");
    m_transcript->append(QString("<div style='margin-bottom:6px;'>"
                                 "<span style='color:%1; font-weight:bold;'>%2</span> "
                                 "<span style='color:#7E8899; font-size:10px;'>%3</span><br/>"
                                 "<span>%4</span></div>")
                             .arg(accent.name(), safeAuthor, timeStr, safeText));
}

void TeamChatWidget::appendSystemNotice(const QString& text) {
    m_transcript->append(QString("<div style='color:#88C0D0; font-style:italic; font-size:11px; margin:4px 0;'>• %1</div>")
                             .arg(text.toHtmlEscaped()));
}

void TeamChatWidget::clearMessages() {
    m_transcript->clear();
}

void TeamChatWidget::onSendClicked() {
    if (!m_connected) return;
    const QString text = m_input->text().trimmed();
    if (text.isEmpty()) return;
    m_input->clear();
    emit sendMessageRequested(text);
}

// ============================================================================
// CollabHubWidget (Journal | Discuss | Review)
// ============================================================================

CollabHubWidget::CollabHubWidget(ea::Repository& repo, QWidget* parent)
    : QWidget(parent), m_repo(repo) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    auto* topRow = new QHBoxLayout();
    m_scopeLabel = new QLabel("Ámbito: Proyecto completo", this);
    setThemedStyleSheet(m_scopeLabel, "color: @primary; font-size: 11.5px; font-weight: 600;");
    topRow->addWidget(m_scopeLabel, 1);

    m_filterElementCheck = new QCheckBox("Solo elemento seleccionado", this);
    connect(m_filterElementCheck, &QCheckBox::toggled, this, [this]() { refresh(); });
    topRow->addWidget(m_filterElementCheck);
    root->addLayout(topRow);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildPane("journal", "Título de nota en bitácora...", "Decisiones de arquitectura, avances o acuerdos...", false, m_journalPane),
                   M3Icons::icon("book"), "Bitácora");
    m_tabs->addTab(buildPane("discuss", "Tema de discusión...", "Pregunta o propuesta para el equipo...", false, m_discussPane),
                   M3Icons::icon("forum"), "Discusiones");
    m_tabs->addTab(buildPane("review", "Punto de revisión...", "Observación o criterio de revisión UML...", true, m_reviewPane),
                   M3Icons::icon("rate_review"), "Revisiones");
    root->addWidget(m_tabs, 1);
}

QWidget* CollabHubWidget::buildPane(const QString& kind, const QString& titlePlaceholder, const QString& bodyPlaceholder,
                                    bool withReviewActions, Pane& outPane) {
    auto* page = new QWidget(this);
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(6, 6, 6, 6);
    lay->setSpacing(6);

    outPane.list = new QListWidget(page);
    outPane.list->setWordWrap(true);
    lay->addWidget(outPane.list, 1);

    auto* actionRow = new QHBoxLayout();
    if (withReviewActions) {
        outPane.approveBtn = new QPushButton("Aprobar", page);
        M3Icons::decorate(outPane.approveBtn, "check_circle", 15);
        setThemedStyleSheet(outPane.approveBtn, "QPushButton { padding: 4px 10px; font-size: 11px; color: @success; }");
        connect(outPane.approveBtn, &QPushButton::clicked, this, [this]() { updateSelectedReviewStatus("approved"); });
        actionRow->addWidget(outPane.approveBtn);

        outPane.changesBtn = new QPushButton("Pedir cambios", page);
        M3Icons::decorate(outPane.changesBtn, "warning", 15);
        setThemedStyleSheet(outPane.changesBtn, "QPushButton { padding: 4px 10px; font-size: 11px; color: @warning; }");
        connect(outPane.changesBtn, &QPushButton::clicked, this, [this]() { updateSelectedReviewStatus("changes_requested"); });
        actionRow->addWidget(outPane.changesBtn);
    }
    actionRow->addStretch();
    auto* delBtn = new QPushButton("Eliminar", page);
    setThemedStyleSheet(delBtn, "QPushButton { padding: 4px 10px; font-size: 11px; color: @error; }");
    connect(delBtn, &QPushButton::clicked, this, [this, kind, &outPane]() { deleteSelectedFrom(kind, outPane); });
    actionRow->addWidget(delBtn);
    lay->addLayout(actionRow);

    outPane.titleEdit = new QLineEdit(page);
    outPane.titleEdit->setPlaceholderText(titlePlaceholder);
    lay->addWidget(outPane.titleEdit);

    outPane.bodyEdit = new QPlainTextEdit(page);
    outPane.bodyEdit->setPlaceholderText(bodyPlaceholder);
    outPane.bodyEdit->setFixedHeight(64);
    lay->addWidget(outPane.bodyEdit);

    auto* addBtn = new QPushButton("Guardar en el proyecto (.qea)", page);
    M3Icons::decorate(addBtn, "add", 16);
    setThemedStyleSheet(addBtn,
        "QPushButton { background-color: @primaryContainer; color: @onPrimaryContainer; border: 1px solid @primary; border-radius: 10px; padding: 6px 12px; font-size: 11.5px; font-weight: 600; }"
        "QPushButton:hover { background-color: @primary; color: @onPrimary; }"
    );
    connect(addBtn, &QPushButton::clicked, this, [this, kind, &outPane]() { addEntryFromPane(kind, outPane); });
    lay->addWidget(addBtn);

    return page;
}

void CollabHubWidget::setCurrentElement(int elementId) {
    m_elementId = elementId;
    if (m_repo.IsOpen() && m_elementId > 0) {
        if (auto elem = m_repo.GetElementByID(m_elementId)) {
            m_scopeLabel->setText(QString("Elemento activo: %1").arg(QString::fromStdString(elem->GetName())));
        } else {
            m_scopeLabel->setText("Ámbito: Proyecto completo");
        }
    } else {
        m_scopeLabel->setText("Ámbito: Proyecto completo");
    }
    refresh();
}

void CollabHubWidget::showTab(int index) {
    if (m_tabs && index >= 0 && index < m_tabs->count()) {
        m_tabs->setCurrentIndex(index);
    }
    refresh();
}

void CollabHubWidget::refresh() {
    populatePane("journal", m_journalPane);
    populatePane("discuss", m_discussPane);
    populatePane("review", m_reviewPane);
}

void CollabHubWidget::populatePane(const QString& kind, Pane& pane) {
    if (!pane.list) return;
    pane.list->clear();
    if (!m_repo.IsOpen()) return;

    const int filterId = (m_filterElementCheck && m_filterElementCheck->isChecked() && m_elementId > 0) ? m_elementId : -1;
    const auto entries = ea::desktop::CollaborationService::listEntries(m_repo, kind.toStdString(), filterId);
    for (const auto& e : entries) {
        QString badge;
        if (kind == "review") {
            if (e.status == "approved") badge = "[APROBADO] ";
            else if (e.status == "changes_requested") badge = "[CAMBIOS] ";
            else badge = "[ABIERTA] ";
        }
        QString elemTag;
        if (e.elementId > 0) {
            if (auto elem = m_repo.GetElementByID(e.elementId)) {
                elemTag = QString(" · #%1").arg(QString::fromStdString(elem->GetName()));
            }
        }
        const QString header = QString("%1%2 (%3 · %4%5)")
                                   .arg(badge,
                                        QString::fromStdString(e.title.empty() ? "Sin título" : e.title),
                                        QString::fromStdString(e.author.empty() ? "Equipo" : e.author),
                                        QString::fromStdString(e.createdAt),
                                        elemTag);
        const QString text = e.body.empty() ? header : header + "\n" + QString::fromStdString(e.body);
        auto* item = new QListWidgetItem(text, pane.list);
        item->setData(Qt::UserRole, e.id);
        if (e.status == "approved") item->setForeground(QBrush(m3().success));
        else if (e.status == "changes_requested") item->setForeground(QBrush(m3().warning));
    }
}

void CollabHubWidget::addEntryFromPane(const QString& kind, Pane& pane) {
    if (!m_repo.IsOpen()) {
        QMessageBox::information(this, "Colaboración", "Abre o crea un proyecto .qea primero.");
        return;
    }
    const QString title = pane.titleEdit->text().trimmed();
    const QString body = pane.bodyEdit->toPlainText().trimmed();
    if (title.isEmpty() && body.isEmpty()) return;

    ea::desktop::CollabEntry entry;
    entry.kind = kind.toStdString();
    entry.elementId = m_elementId;
    entry.author = m_author.toStdString();
    entry.title = (title.isEmpty() ? body.left(48) : title).toStdString();
    entry.body = body.toStdString();
    entry.status = "open";

    if (ea::desktop::CollaborationService::addEntry(m_repo, entry)) {
        pane.titleEdit->clear();
        pane.bodyEdit->clear();
        refresh();
        emit collaborationDataChanged();
    }
}

void CollabHubWidget::updateSelectedReviewStatus(const QString& newStatus) {
    if (!m_repo.IsOpen() || !m_reviewPane.list) return;
    auto* item = m_reviewPane.list->currentItem();
    if (!item) return;
    const int entryId = item->data(Qt::UserRole).toInt();
    if (ea::desktop::CollaborationService::updateEntryStatus(m_repo, entryId, newStatus.toStdString())) {
        refresh();
        emit collaborationDataChanged();
    }
}

void CollabHubWidget::deleteSelectedFrom(const QString& /*kind*/, Pane& pane) {
    if (!m_repo.IsOpen() || !pane.list) return;
    auto* item = pane.list->currentItem();
    if (!item) return;
    const int entryId = item->data(Qt::UserRole).toInt();
    if (ea::desktop::CollaborationService::deleteEntry(m_repo, entryId)) {
        refresh();
        emit collaborationDataChanged();
    }
}

// ============================================================================
// ProjectPlannerWidget (Kanban | Gantt | Calendar)
// ============================================================================

ProjectPlannerWidget::ProjectPlannerWidget(ea::Repository& repo, QWidget* parent)
    : QWidget(parent), m_repo(repo) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(12);

    // Top pill switcher (compact so it never clips on 1280px or tiled windows)
    auto* topBar = new QHBoxLayout();
    topBar->setSpacing(8);
    auto* titleLbl = new QLabel("Planificación", this);
    setThemedStyleSheet(titleLbl, "color: @onSurface; font-size: 16px; font-weight: 700;");
    topBar->addWidget(titleLbl);
    topBar->addSpacing(8);

    const std::array<std::pair<QString, QString>, 3> navItems = {{
        {"Kanban", "view_kanban"},
        {"Gantt", "view_timeline"},
        {"Calendario", "calendar_month"}
    }};
    for (int i = 0; i < 3; ++i) {
        auto* btn = new QPushButton(navItems[i].first, this);
        btn->setCheckable(true);
        M3Icons::decorate(btn, navItems[i].second, 16);
        setThemedStyleSheet(btn,
            "QPushButton { background-color: @surfaceContainer; color: @onSurfaceMuted; border: 1px solid @outlineVariant; border-radius: 12px; padding: 5px 12px; font-size: 11.5px; font-weight: 600; }"
            "QPushButton:hover { background-color: @surfaceHigh; color: @onSurface; }"
            "QPushButton:checked { background-color: @primaryContainer; color: @onPrimaryContainer; border-color: @primary; }"
        );
        connect(btn, &QPushButton::clicked, this, [this, i]() { showView(i); });
        m_navButtons[i] = btn;
        topBar->addWidget(btn);
    }
    topBar->addStretch();

    auto* refreshBtn = new QPushButton("Actualizar", this);
    M3Icons::decorate(refreshBtn, "refresh", 15);
    setThemedStyleSheet(refreshBtn, "QPushButton { padding: 5px 11px; font-size: 11.5px; border-radius: 12px; }");
    connect(refreshBtn, &QPushButton::clicked, this, &ProjectPlannerWidget::refreshFromRepository);
    topBar->addWidget(refreshBtn);
    root->addLayout(topBar);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildKanbanPage());
    m_stack->addWidget(buildGanttPage());
    m_stack->addWidget(buildCalendarPage());
    root->addWidget(m_stack, 1);

    showView(ViewKanban);
}

void ProjectPlannerWidget::showView(int index) {
    if (index < 0 || index >= 3) index = 0;
    m_stack->setCurrentIndex(index);
    for (int i = 0; i < 3; ++i) {
        if (m_navButtons[i]) m_navButtons[i]->setChecked(i == index);
    }
    refreshFromRepository();
}

void ProjectPlannerWidget::refreshFromRepository() {
    refreshKanban();
    refreshGantt();
    refreshCalendar();
}

QWidget* ProjectPlannerWidget::buildKanbanPage() {
    auto* page = new QWidget(this);
    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(10);

    auto* subtitle = new QLabel("Cada tarjeta corresponde a un elemento UML real del repositorio .qea (campo Status).", page);
    setThemedStyleSheet(subtitle, "color: @onSurfaceMuted; font-size: 12px;");
    root->addWidget(subtitle);

    auto* boardContainer = new QWidget(page);
    auto* colsRow = new QHBoxLayout(boardContainer);
    colsRow->setContentsMargins(0, 0, 0, 0);
    colsRow->setSpacing(10);

    const QStringList colColors = {"@primary", "@secondary", "@warning", "@success"};

    for (int c = 0; c < kKanbanCols; ++c) {
        auto* colFrame = new QFrame(boardContainer);
        colFrame->setMinimumWidth(165);
        setThemedStyleSheet(colFrame,
            "QFrame { background-color: @surfaceLow; border: 1px solid @outlineVariant; border-radius: 12px; }"
        );
        auto* colLay = new QVBoxLayout(colFrame);
        colLay->setContentsMargins(8, 8, 8, 8);
        colLay->setSpacing(6);

        auto* header = new QLabel(kKanbanTitles[c], colFrame);
        setThemedStyleSheet(header, QString("color: %1; font-size: 13px; font-weight: 700; border: none;").arg(colColors[c]));
        m_kanbanHeaders[c] = header;
        colLay->addWidget(header);

        auto* list = new QListWidget(colFrame);
        list->setMinimumWidth(130);
        list->setSpacing(4);
        list->setWordWrap(true);
        connect(list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
            if (item) emit elementActivated(item->data(Qt::UserRole).toInt());
        });
        m_kanbanLists[c] = list;
        colLay->addWidget(list, 1);

        auto* btnRow = new QHBoxLayout();
        btnRow->setSpacing(4);
        if (c > 0) {
            auto* leftBtn = new QPushButton("◀", colFrame);
            leftBtn->setToolTip("Mover a la columna anterior");
            setThemedStyleSheet(leftBtn, "QPushButton { padding: 4px 8px; font-size: 11px; }");
            connect(leftBtn, &QPushButton::clicked, this, [this, c]() { moveSelectedCard(c, -1); });
            btnRow->addWidget(leftBtn);
        }
        auto* addBtn = new QPushButton("+ Tarjeta", colFrame);
        setThemedStyleSheet(addBtn, "QPushButton { padding: 4px 10px; font-size: 11px; }");
        connect(addBtn, &QPushButton::clicked, this, [this, c]() { addKanbanCard(c); });
        btnRow->addWidget(addBtn, 1);
        if (c + 1 < kKanbanCols) {
            auto* rightBtn = new QPushButton("▶", colFrame);
            rightBtn->setToolTip("Avanzar a la siguiente columna");
            setThemedStyleSheet(rightBtn, "QPushButton { padding: 4px 8px; font-size: 11px; }");
            connect(rightBtn, &QPushButton::clicked, this, [this, c]() { moveSelectedCard(c, +1); });
            btnRow->addWidget(rightBtn);
        }
        colLay->addLayout(btnRow);

        colsRow->addWidget(colFrame, 1);
    }

    auto* boardScroll = new QScrollArea(page);
    boardScroll->setWidgetResizable(true);
    boardScroll->setFrameShape(QFrame::NoFrame);
    boardScroll->setMinimumSize(220, 180);
    boardScroll->setWidget(boardContainer);
    root->addWidget(boardScroll, 1);
    return page;
}

void ProjectPlannerWidget::refreshKanban() {
    for (int c = 0; c < kKanbanCols; ++c) {
        if (m_kanbanLists[c]) m_kanbanLists[c]->clear();
    }
    if (!m_repo.IsOpen()) {
        for (int c = 0; c < kKanbanCols; ++c) {
            if (m_kanbanHeaders[c]) m_kanbanHeaders[c]->setText(QString("%1 (0)").arg(kKanbanTitles[c]));
        }
        return;
    }

    const auto cards = ea::desktop::CollaborationService::listKanbanCards(m_repo);
    std::array<int, kKanbanCols> counts{};
    for (const auto& card : cards) {
        int col = kKanbanStatuses.indexOf(QString::fromStdString(card.status));
        if (col < 0) col = 0;
        ++counts[col];

        QString label = QString("%1\n[%2]").arg(QString::fromStdString(card.name), QString::fromStdString(card.typeName));
        if (!card.author.empty()) label += QString(" · %1").arg(QString::fromStdString(card.author));

        auto* item = new QListWidgetItem(label, m_kanbanLists[col]);
        item->setData(Qt::UserRole, card.elementId);
        item->setToolTip("Doble clic para inspeccionar el elemento UML");
    }

    for (int c = 0; c < kKanbanCols; ++c) {
        m_kanbanHeaders[c]->setText(QString("%1 (%2)").arg(kKanbanTitles[c]).arg(counts[c]));
    }
}

void ProjectPlannerWidget::moveSelectedCard(int columnIndex, int delta) {
    const int targetCol = columnIndex + delta;
    if (!m_repo.IsOpen() || targetCol < 0 || targetCol >= kKanbanCols) return;
    auto* item = m_kanbanLists[columnIndex]->currentItem();
    if (!item) return;

    const int elemId = item->data(Qt::UserRole).toInt();
    if (ea::desktop::CollaborationService::moveKanbanCard(m_repo, elemId, kKanbanStatuses[targetCol].toStdString())) {
        refreshKanban();
        emit plannerDataChanged();
    }
}

void ProjectPlannerWidget::addKanbanCard(int columnIndex) {
    if (!m_repo.IsOpen()) {
        QMessageBox::information(this, "Kanban", "Abre o crea un proyecto .qea antes de crear tarjetas.");
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this, "Nueva Tarjeta / Elemento",
                                               QString("Nombre para columna \"%1\":").arg(kKanbanTitles[columnIndex]),
                                               QLineEdit::Normal, "", &ok).trimmed();
    if (!ok || name.isEmpty()) return;

    if (m_repo.GetModels().size() == 0) {
        m_repo.CreateModel("Model");
    }
    auto rootPkg = m_repo.GetModels().GetAt(0);
    if (!rootPkg) return;

    ea::Element elem;
    elem.SetName(name.toStdString());
    elem.SetType(ea::ObjectType::Requirement);
    elem.SetPackageID(rootPkg->GetPackageID());
    elem.SetStatus(kKanbanStatuses[columnIndex].toStdString());
    elem.SetAuthor(m_author.toStdString());
    elem.SetElementGUID(ea::utils::generate_guid());

    if (m_repo.AddElement(elem)) {
        refreshKanban();
        emit plannerDataChanged();
    }
}

QWidget* ProjectPlannerWidget::buildGanttPage() {
    auto* page = new QWidget(this);
    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(10);

    auto* toolsRow = new QHBoxLayout();
    auto* addBtn = new QPushButton("+ Nueva Tarea", page);
    M3Icons::decorate(addBtn, "add", 16);
    connect(addBtn, &QPushButton::clicked, this, [this]() { addGanttTask(); });
    toolsRow->addWidget(addBtn);

    auto* importBtn = new QPushButton("Importar Elementos del Modelo", page);
    M3Icons::decorate(importBtn, "download", 16);
    connect(importBtn, &QPushButton::clicked, this, &ProjectPlannerWidget::importElementsToGantt);
    toolsRow->addWidget(importBtn);

    auto* progBtn = new QPushButton("+25% Avance", page);
    M3Icons::decorate(progBtn, "check_circle", 16);
    connect(progBtn, &QPushButton::clicked, this, &ProjectPlannerWidget::advanceSelectedTaskProgress);
    toolsRow->addWidget(progBtn);

    auto* delBtn = new QPushButton("Eliminar Tarea", page);
    setThemedStyleSheet(delBtn, "QPushButton { color: @error; }");
    connect(delBtn, &QPushButton::clicked, this, &ProjectPlannerWidget::deleteSelectedTask);
    toolsRow->addWidget(delBtn);
    toolsRow->addStretch();
    root->addLayout(toolsRow);

    m_ganttTable = new QTableWidget(page);
    m_ganttTable->setColumnCount(5);
    m_ganttTable->setHorizontalHeaderLabels({"Tarea / Entregable", "Responsable", "Fecha límite", "Estado", "Cronograma (Gantt)"});
    m_ganttTable->verticalHeader()->setVisible(false);
    m_ganttTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ganttTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_ganttTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_ganttTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_ganttTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_ganttTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_ganttTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_ganttTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_ganttTable->setItemDelegateForColumn(4, new GanttBarDelegate(m_ganttTable));
    root->addWidget(m_ganttTable, 1);

    return page;
}

void ProjectPlannerWidget::refreshGantt() {
    if (!m_ganttTable) return;
    m_ganttTable->setRowCount(0);
    if (!m_repo.IsOpen()) return;

    const auto tasks = ea::desktop::CollaborationService::listTasks(m_repo);
    int maxDay = 14;
    for (const auto& t : tasks) {
        maxDay = std::max(maxDay, t.startDay + t.durationDays + 1);
    }

    m_ganttTable->setRowCount(static_cast<int>(tasks.size()));
    for (int i = 0; i < static_cast<int>(tasks.size()); ++i) {
        const auto& t = tasks[static_cast<size_t>(i)];
        m_ganttTable->setRowHeight(i, 34);

        auto* titleItem = new QTableWidgetItem(QString::fromStdString(t.title));
        titleItem->setData(Qt::UserRole, t.id);
        titleItem->setData(Qt::UserRole + 10, t.progress);
        m_ganttTable->setItem(i, 0, titleItem);
        m_ganttTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(t.assignee.empty() ? "-" : t.assignee)));
        m_ganttTable->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(t.dueDate.empty() ? "-" : t.dueDate)));
        m_ganttTable->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(t.status)));

        auto* barItem = new QTableWidgetItem();
        barItem->setData(Qt::UserRole + 1, t.startDay);
        barItem->setData(Qt::UserRole + 2, t.durationDays);
        barItem->setData(Qt::UserRole + 3, t.progress);
        barItem->setData(Qt::UserRole + 4, maxDay);
        m_ganttTable->setItem(i, 4, barItem);
    }
}

void ProjectPlannerWidget::addGanttTask(const QString& presetDueDate) {
    if (!m_repo.IsOpen()) {
        QMessageBox::information(this, "Cronograma", "Abre o crea un proyecto .qea primero.");
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Nueva Tarea / Hito del Proyecto");
    auto* form = new QFormLayout(&dlg);
    auto* titleEdit = new QLineEdit(&dlg);
    titleEdit->setPlaceholderText("Ej. Diseñar Diagrama de Clases");
    auto* assigneeEdit = new QLineEdit(m_author, &dlg);
    auto* startSpin = new QSpinBox(&dlg);
    startSpin->setRange(1, 365);
    startSpin->setValue(1);
    auto* durSpin = new QSpinBox(&dlg);
    durSpin->setRange(1, 180);
    durSpin->setValue(4);
    auto* dueEdit = new QLineEdit(presetDueDate.isEmpty() ? QDate::currentDate().addDays(4).toString("yyyy-MM-dd") : presetDueDate, &dlg);

    form->addRow("Título:", titleEdit);
    form->addRow("Responsable:", assigneeEdit);
    form->addRow("Día de inicio (Gantt):", startSpin);
    form->addRow("Duración (días):", durSpin);
    form->addRow("Fecha límite (YYYY-MM-DD):", dueEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);

    if (dlg.exec() != QDialog::Accepted || titleEdit->text().trimmed().isEmpty()) return;

    ea::desktop::ProjectTask task;
    task.title = titleEdit->text().trimmed().toStdString();
    task.assignee = assigneeEdit->text().trimmed().toStdString();
    task.startDay = startSpin->value();
    task.durationDays = durSpin->value();
    task.progress = 0;
    task.status = "Proposed";
    task.dueDate = dueEdit->text().trimmed().toStdString();

    if (ea::desktop::CollaborationService::addTask(m_repo, task)) {
        refreshGantt();
        refreshCalendar();
        emit plannerDataChanged();
    }
}

void ProjectPlannerWidget::importElementsToGantt() {
    if (!m_repo.IsOpen()) return;
    const auto cards = ea::desktop::CollaborationService::listKanbanCards(m_repo);
    const auto existing = ea::desktop::CollaborationService::listTasks(m_repo);

    int dayCursor = 1;
    int added = 0;
    for (const auto& card : cards) {
        const bool already = std::any_of(existing.begin(), existing.end(), [&](const auto& t) {
            return t.elementId == card.elementId && card.elementId > 0;
        });
        if (already) continue;

        ea::desktop::ProjectTask t;
        t.title = card.name + " (" + card.typeName + ")";
        t.assignee = card.author.empty() ? m_author.toStdString() : card.author;
        t.startDay = dayCursor;
        t.durationDays = 3;
        t.progress = (card.status == "Validated") ? 100 : (card.status == "Implemented" ? 75 : (card.status == "Approved" ? 25 : 0));
        t.status = card.status;
        t.elementId = card.elementId;
        t.dueDate = QDate::currentDate().addDays(dayCursor + 2).toString("yyyy-MM-dd").toStdString();
        if (ea::desktop::CollaborationService::addTask(m_repo, t)) {
            ++added;
            dayCursor += 2;
        }
        if (added >= 15) break;
    }
    if (added > 0) {
        refreshGantt();
        refreshCalendar();
        emit plannerDataChanged();
    }
}

void ProjectPlannerWidget::advanceSelectedTaskProgress() {
    if (!m_repo.IsOpen() || !m_ganttTable) return;
    const int row = m_ganttTable->currentRow();
    if (row < 0) return;
    auto* item = m_ganttTable->item(row, 0);
    if (!item) return;

    const int taskId = item->data(Qt::UserRole).toInt();
    const int cur = item->data(Qt::UserRole + 10).toInt();
    const int next = (cur >= 100) ? 0 : std::min(100, cur + 25);
    const std::string status = (next >= 100) ? "Validated" : (next >= 75 ? "Implemented" : (next >= 25 ? "Approved" : "Proposed"));
    if (ea::desktop::CollaborationService::updateTaskProgress(m_repo, taskId, next, status)) {
        refreshGantt();
        refreshCalendar();
        emit plannerDataChanged();
    }
}

void ProjectPlannerWidget::deleteSelectedTask() {
    if (!m_repo.IsOpen() || !m_ganttTable) return;
    const int row = m_ganttTable->currentRow();
    if (row < 0) return;
    auto* item = m_ganttTable->item(row, 0);
    if (!item) return;

    if (ea::desktop::CollaborationService::deleteTask(m_repo, item->data(Qt::UserRole).toInt())) {
        refreshGantt();
        refreshCalendar();
        emit plannerDataChanged();
    }
}

QWidget* ProjectPlannerWidget::buildCalendarPage() {
    auto* page = new QWidget(this);
    auto* split = new QSplitter(Qt::Horizontal, page);

    m_calendar = new QCalendarWidget(split);
    m_calendar->setGridVisible(true);
    connect(m_calendar, &QCalendarWidget::selectionChanged, this, &ProjectPlannerWidget::refreshCalendar);
    split->addWidget(m_calendar);

    auto* rightPanel = new QWidget(split);
    auto* rightLay = new QVBoxLayout(rightPanel);
    rightLay->setContentsMargins(12, 4, 4, 4);
    rightLay->setSpacing(8);

    m_calendarDateLabel = new QLabel(rightPanel);
    setThemedStyleSheet(m_calendarDateLabel, "color: @primary; font-size: 14px; font-weight: 700;");
    rightLay->addWidget(m_calendarDateLabel);

    m_calendarTasksList = new QListWidget(rightPanel);
    m_calendarTasksList->setWordWrap(true);
    rightLay->addWidget(m_calendarTasksList, 1);

    auto* addMilestoneBtn = new QPushButton("+ Agregar Hito en esta fecha", rightPanel);
    M3Icons::decorate(addMilestoneBtn, "event", 16);
    setThemedStyleSheet(addMilestoneBtn,
        "QPushButton { background-color: @primaryContainer; color: @onPrimaryContainer; border: 1px solid @primary; border-radius: 10px; padding: 7px 14px; font-weight: 600; }"
        "QPushButton:hover { background-color: @primary; color: @onPrimary; }"
    );
    connect(addMilestoneBtn, &QPushButton::clicked, this, [this]() {
        addGanttTask(m_calendar->selectedDate().toString("yyyy-MM-dd"));
    });
    rightLay->addWidget(addMilestoneBtn);

    split->addWidget(rightPanel);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);

    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(split);
    return page;
}

void ProjectPlannerWidget::refreshCalendar() {
    if (!m_calendar || !m_calendarTasksList || !m_calendarDateLabel) return;
    const QDate sel = m_calendar->selectedDate();
    const QString selIso = sel.toString("yyyy-MM-dd");
    m_calendarDateLabel->setText(QString("Hitos y entregas · %1").arg(sel.toString("dd MMM yyyy")));
    m_calendarTasksList->clear();

    if (!m_repo.IsOpen()) return;
    const auto tasks = ea::desktop::CollaborationService::listTasks(m_repo);

    int onDate = 0;
    for (const auto& t : tasks) {
        if (QString::fromStdString(t.dueDate) == selIso) {
            auto* item = new QListWidgetItem(
                QString("★ %1 (%2%)\nResponsable: %3 · Estado: %4")
                    .arg(QString::fromStdString(t.title))
                    .arg(t.progress)
                    .arg(QString::fromStdString(t.assignee.empty() ? "-" : t.assignee),
                         QString::fromStdString(t.status)),
                m_calendarTasksList);
            item->setForeground(QBrush(m3().primary));
            ++onDate;
        }
    }
    if (onDate == 0) {
        auto* empty = new QListWidgetItem("Sin entregas programadas para este día.\nUsa el botón de abajo para agendar un hito.", m_calendarTasksList);
        empty->setForeground(QBrush(m3().onSurfaceMuted));
    }

    if (!tasks.empty()) {
        auto* sep = new QListWidgetItem("── Todos los hitos del proyecto ──", m_calendarTasksList);
        sep->setFlags(Qt::NoItemFlags);
        sep->setForeground(QBrush(m3().onSurfaceDisabled));
        for (const auto& t : tasks) {
            new QListWidgetItem(
                QString("[%1] %2 — %3% (%4)")
                    .arg(QString::fromStdString(t.dueDate.empty() ? "Sin fecha" : t.dueDate),
                         QString::fromStdString(t.title))
                    .arg(t.progress)
                    .arg(QString::fromStdString(t.assignee.empty() ? "-" : t.assignee)),
                m_calendarTasksList);
        }
    }
}

} // namespace ea::gui
