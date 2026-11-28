#include "StartPageWidget.hpp"
#include "DarkTheme.hpp"
#include "M3Icons.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QFrame>
#include <QFile>
#include <QFileInfo>
#include <QScrollArea>
#include <QSettings>

namespace ea::gui {

namespace {

QWidget* createFeatureCard(QWidget* parent,
                           const QString& title,
                           const QString& subtitle,
                           const QString& symbol,
                           const QColor& accent,
                           const std::function<void()>& onClick) {
    auto* card = new QPushButton(parent);
    card->setCursor(Qt::PointingHandCursor);
    card->setMinimumHeight(74);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    auto* row = new QHBoxLayout(card);
    row->setContentsMargins(14, 10, 14, 10);
    row->setSpacing(12);

    auto* iconBadge = new QLabel(card);
    iconBadge->setFixedSize(36, 36);
    iconBadge->setAlignment(Qt::AlignCenter);
    iconBadge->setPixmap(M3Icons::icon(symbol, accent).pixmap(22, 22));
    iconBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
    iconBadge->setStyleSheet(QString("background-color: rgba(%1,%2,%3,38); border-radius: 10px;")
                                 .arg(accent.red())
                                 .arg(accent.green())
                                 .arg(accent.blue()));
    row->addWidget(iconBadge);

    auto* textCol = new QVBoxLayout();
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->setSpacing(2);

    auto* titleLbl = new QLabel(title, card);
    titleLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
    setThemedStyleSheet(titleLbl, "color: @onSurface; font-size: 13px; font-weight: 600; background: transparent;");

    auto* subLbl = new QLabel(subtitle, card);
    subLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
    subLbl->setWordWrap(true);
    setThemedStyleSheet(subLbl, "color: @onSurfaceMuted; font-size: 11px; background: transparent;");

    textCol->addWidget(titleLbl);
    textCol->addWidget(subLbl);
    row->addLayout(textCol, 1);

    setThemedStyleSheet(card,
        "QPushButton { background-color: @surfaceContainer; border: 1px solid @outlineVariant; border-radius: 12px; text-align: left; padding: 0px; }"
        "QPushButton:hover { background-color: @surfaceHigh; border-color: @primary; }"
        "QPushButton:pressed { background-color: @primaryContainer; }"
    );

    if (onClick) {
        QObject::connect(card, &QPushButton::clicked, onClick);
    }
    return card;
}

} // namespace

StartPageWidget::StartPageWidget(QWidget* parent)
    : QWidget(parent) {
    setupUi();
    refreshRecentProjects();
}

void StartPageWidget::setupUi() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto bgWidget = new QWidget(this);
    ea::gui::setThemedStyleSheet(bgWidget, "background-color: @surface;");
    auto bgLayout = new QVBoxLayout(bgWidget);
    bgLayout->setContentsMargins(28, 22, 28, 24);
    bgLayout->setSpacing(18);

    // Encabezado limpio con título y subtítulo
    auto headerCol = new QVBoxLayout();
    headerCol->setSpacing(4);
    auto lblBrand = new QLabel("onlyArchitect Studio", bgWidget);
    ea::gui::setThemedStyleSheet(lblBrand, "color: @onSurface; font-size: 22px; font-weight: 700;");
    auto lblSubtitle = new QLabel("Suite libre de modelado UML 2.5.1 en C++20 con interoperabilidad .qea / .eapx / XMI 2.5.1 y colaboración en tiempo real.", bgWidget);
    lblSubtitle->setWordWrap(true);
    ea::gui::setThemedStyleSheet(lblSubtitle, "color: @onSurfaceMuted; font-size: 12.5px;");
    headerCol->addWidget(lblBrand);
    headerCol->addWidget(lblSubtitle);
    bgLayout->addLayout(headerCol);

    // Tres columnas claras: Acciones | Recientes | Fijados
    auto colsLayout = new QHBoxLayout();
    colsLayout->setSpacing(22);

    // ==========================================
    // Columna 1: EMPEZAR (Tarjetas visuales claras)
    // ==========================================
    auto colOpen = new QVBoxLayout();
    colOpen->setSpacing(12);

    auto lblAbrir = new QLabel("Empezar", bgWidget);
    ea::gui::setThemedStyleSheet(lblAbrir, "color: @primary; font-size: 16px; font-weight: 700; margin-bottom: 4px;");
    colOpen->addWidget(lblAbrir);

    colOpen->addWidget(createFeatureCard(
        bgWidget, "Abrir Proyecto", "Abre archivos .qea, .eapx o .eap desde tu equipo",
        "folder_open", m3().primary, [this]() { emit openFileBrowseRequested(); }));

    colOpen->addWidget(createFeatureCard(
        bgWidget, "Nuevo Proyecto", "Crea un repositorio UML vacío listo para diagramar",
        "note_add", m3().secondary, [this]() { emit newProjectRequested(); }));

    colOpen->addWidget(createFeatureCard(
        bgWidget, "Multijugador (LAN / VPN)", "Hospeda un proyecto o únete en tiempo real con tu equipo",
        "dns", m3().success, [this]() { emit serverConnectionRequested(); }));

    colOpen->addWidget(createFeatureCard(
        bgWidget, "Migrar .EAP / .EAPX → .QEA", "Convierte repositorios MS Access (.eap / .eapx) a SQLite (.qea)",
        "swap_horiz", m3().tertiary, [this]() { emit manageProjectsRequested(); }));

    colOpen->addStretch();
    colsLayout->addLayout(colOpen, 14);

    // ==========================================
    // Columna 2: RECIENTES
    // ==========================================
    auto colRecent = new QVBoxLayout();
    colRecent->setSpacing(12);

    auto lblRecentHeader = new QLabel("Recientes", bgWidget);
    ea::gui::setThemedStyleSheet(lblRecentHeader, "color: @primary; font-size: 16px; font-weight: 700; margin-bottom: 4px;");
    colRecent->addWidget(lblRecentHeader);

    m_recentsLayout = new QVBoxLayout();
    m_recentsLayout->setSpacing(8);
    colRecent->addLayout(m_recentsLayout);
    colRecent->addStretch();

    colsLayout->addLayout(colRecent, 11);

    // ==========================================
    // Columna 3: FIJADOS
    // ==========================================
    auto colPinned = new QVBoxLayout();
    colPinned->setSpacing(12);

    auto lblPinned = new QLabel("Fijados", bgWidget);
    ea::gui::setThemedStyleSheet(lblPinned, "color: @primary; font-size: 16px; font-weight: 700; margin-bottom: 4px;");
    colPinned->addWidget(lblPinned);

    m_pinnedLayout = new QVBoxLayout();
    m_pinnedLayout->setSpacing(8);
    colPinned->addLayout(m_pinnedLayout);
    colPinned->addStretch();

    colsLayout->addLayout(colPinned, 10);

    bgLayout->addLayout(colsLayout, 1);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMinimumSize(260, 200);
    scroll->setWidget(bgWidget);
    mainLayout->addWidget(scroll);
}

QWidget* StartPageWidget::createActionLink(const QString& title, const QString& symbol, const std::function<void()>& onClick) {
    return createFeatureCard(this, title, QString(), symbol, m3().primary, onClick);
}

void StartPageWidget::addRecentProject(const QString& path) {
    const QString absPath = QFileInfo(path).absoluteFilePath();
    if (absPath.isEmpty() || !QFile::exists(absPath)) return;
    // Never pollute recent projects with temporary LAN/VPN sync cache files
    if (absPath.contains("onlyarchitect_team_sync")) return;

    QSettings settings;
    QStringList recent = settings.value("startPage/recentProjects").toStringList();
    recent.removeAll(absPath);
    recent.prepend(absPath);
    while (recent.size() > 10) recent.removeLast();
    settings.setValue("startPage/recentProjects", recent);
}

void StartPageWidget::refreshRecentProjects() {
    auto clearLayout = [](QVBoxLayout* layout) {
        QLayoutItem* item;
        while ((item = layout->takeAt(0)) != nullptr) {
            if (auto* w = item->widget()) {
                w->hide();
                w->setParent(nullptr);
                w->deleteLater();
            }
            delete item;
        }
    };
    clearLayout(m_recentsLayout);
    clearLayout(m_pinnedLayout);

    QWidget* listParent = m_recentsLayout->parentWidget() ? m_recentsLayout->parentWidget() : this;
    QSettings settings;
    const QStringList recent = settings.value("startPage/recentProjects").toStringList();
    const QStringList pinned = settings.value("startPage/pinnedProjects").toStringList();

    auto makeProjectRow = [this, listParent, pinned](const QString& path, bool isPinned) -> QWidget* {
        const QFileInfo fi(path);
        auto* rowFrame = new QFrame(listParent);
        ea::gui::setThemedStyleSheet(rowFrame,
            "QFrame { background-color: @surfaceLow; border: 1px solid @outlineVariant; border-radius: 10px; }"
            "QFrame:hover { background-color: @surfaceContainer; border-color: @outline; }"
        );
        auto* h = new QHBoxLayout(rowFrame);
        h->setContentsMargins(10, 6, 8, 6);
        h->setSpacing(8);

        auto* openBtn = new QToolButton(rowFrame);
        openBtn->setText(fi.fileName());
        openBtn->setToolTip(path);
        openBtn->setCursor(Qt::PointingHandCursor);
        ea::gui::M3Icons::decorate(openBtn, isPinned ? "push_pin" : "description");
        openBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        openBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        ea::gui::setThemedStyleSheet(openBtn,
            "QToolButton { background: transparent; color: @onSurface; border: none; font-size: 12.5px; font-weight: 500; text-align: left; padding: 2px; }"
            "QToolButton:hover { color: @primary; }"
        );
        connect(openBtn, &QToolButton::clicked, this, [this, path]() { emit openProjectRequested(path); });
        h->addWidget(openBtn, 1);

        auto* pinBtn = new QToolButton(rowFrame);
        ea::gui::M3Icons::decorate(pinBtn, "push_pin", 15);
        pinBtn->setToolTip(isPinned ? "Quitar de Fijados" : "Fijar proyecto");
        pinBtn->setCursor(Qt::PointingHandCursor);
        ea::gui::setThemedStyleSheet(pinBtn,
            isPinned ? "QToolButton { background: transparent; color: @primary; border: none; padding: 4px; }"
                     : "QToolButton { background: transparent; color: @onSurfaceDisabled; border: none; padding: 4px; } QToolButton:hover { color: @primary; }"
        );
        connect(pinBtn, &QToolButton::clicked, this, [this, path, isPinned]() {
            QSettings s;
            QStringList list = s.value("startPage/pinnedProjects").toStringList();
            if (isPinned) list.removeAll(path);
            else if (!list.contains(path)) list.prepend(path);
            s.setValue("startPage/pinnedProjects", list);
            refreshRecentProjects();
        });
        h->addWidget(pinBtn);
        return rowFrame;
    };

    int shown = 0;
    for (const QString& path : recent) {
        if (!QFile::exists(path) || path.contains("onlyarchitect_team_sync")) continue;
        m_recentsLayout->addWidget(makeProjectRow(path, pinned.contains(path)));
        if (++shown >= 7) break;
    }

    if (shown == 0) {
        auto emptyLabel = new QLabel("Todavía no has abierto proyectos.\nUsa \"Abrir Proyecto\" o \"Nuevo Proyecto\".", listParent);
        emptyLabel->setWordWrap(true);
        ea::gui::setThemedStyleSheet(emptyLabel, "color: @onSurfaceMuted; font-size: 12px; padding: 8px;");
        m_recentsLayout->addWidget(emptyLabel);
    }

    int pinnedShown = 0;
    for (const QString& path : pinned) {
        if (!QFile::exists(path)) continue;
        m_pinnedLayout->addWidget(makeProjectRow(path, true));
        ++pinnedShown;
    }

    if (pinnedShown == 0) {
        auto emptyPin = new QLabel("Fija tus proyectos favoritos con el ícono de chincheta para abrirlos en un clic.", listParent);
        emptyPin->setWordWrap(true);
        ea::gui::setThemedStyleSheet(emptyPin, "color: @onSurfaceMuted; font-size: 12px; padding: 8px;");
        m_pinnedLayout->addWidget(emptyPin);
    }
}

} // namespace ea::gui
