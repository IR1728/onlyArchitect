#include "RibbonBarWidget.hpp"
#include "DarkTheme.hpp"
#include "M3Icons.hpp"
#include <QFrame>
#include <QScrollArea>

namespace ea::gui {

RibbonGroupWidget::RibbonGroupWidget(const QString& title, QWidget* parent)
    : QWidget(parent) {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(6, 2, 6, 2);
    m_mainLayout->setSpacing(1);

    m_contentLayout = new QHBoxLayout();
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(2);
    m_mainLayout->addLayout(m_contentLayout, 1);

    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    ea::gui::setThemedStyleSheet(m_titleLabel, "color: @onSurfaceDisabled; font-size: 9.5px; font-weight: 600; letter-spacing: 0.4px; margin-top: 1px;");
    m_mainLayout->addWidget(m_titleLabel);

    ea::gui::setThemedStyleSheet(this,
        "RibbonGroupWidget { border-right: 1px solid @outlineVariant; background: transparent; }"
    );
}

namespace {

constexpr int RibbonContentHeight = 98; // Fits large icon + label + group title without vertical clipping
constexpr int LargeIconSize = 22;
constexpr int SmallIconSize = 15;

QWidget* wrapScrollableRibbonPage(QWidget* content, QWidget* parent) {
    auto* scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setMinimumWidth(0);
    scroll->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    content->setStyleSheet("background: transparent;");
    scroll->setWidget(content);
    return scroll;
}

QToolButton* makeRibbonButton(QWidget* parent, const QString& text, const QString& symbol, int iconSize) {
    auto btn = new QToolButton(parent);
    btn->setText(text);
    btn->setToolTip(text);
    btn->setIcon(M3Icons::icon(symbol));
    btn->setIconSize(QSize(iconSize, iconSize));
    btn->setAutoRaise(true);
    return btn;
}

} // namespace

void RibbonGroupWidget::addLargeButton(const QString& text, const QString& symbol, const std::function<void()>& onClick) {
    auto btn = makeRibbonButton(this, text, symbol, LargeIconSize);
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    ea::gui::setThemedStyleSheet(btn,
        "QToolButton { background-color: transparent; color: @onSurfaceVariant; border: 1px solid transparent; border-radius: 10px; padding: 4px 8px; font-size: 11px; font-weight: 500; }"
        "QToolButton:hover { background-color: @surfaceHighest; color: @onSurface; }"
        "QToolButton:pressed { background-color: @primaryContainer; color: @onPrimaryContainer; border: 1px solid @primary; }"
    );
    if (onClick) {
        connect(btn, &QToolButton::clicked, onClick);
    } else {
        btn->setEnabled(false);
        btn->setToolTip(QString("%1 — Próximamente en onlyArchitect").arg(text));
    }
    m_contentLayout->addWidget(btn);
}

// Small buttons stack in columns of SmallButtonsPerColumn, like the Office / EA ribbon, so a
// group with many of them grows sideways instead of overlapping inside the fixed-height bar
QVBoxLayout* RibbonGroupWidget::smallButtonColumn() {
    if (!m_smallButtonsLayout || m_smallButtonsLayout->count() >= SmallButtonsPerColumn) {
        m_smallButtonsLayout = new QVBoxLayout();
        m_smallButtonsLayout->setContentsMargins(0, 0, 0, 0);
        m_smallButtonsLayout->setSpacing(1);
        m_smallButtonsLayout->setAlignment(Qt::AlignTop);
        m_contentLayout->addLayout(m_smallButtonsLayout);
    }
    return m_smallButtonsLayout;
}

void RibbonGroupWidget::addSmallButton(const QString& text, const QString& symbol, const std::function<void()>& onClick) {
    auto btn = makeRibbonButton(this, text, symbol, SmallIconSize);
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ea::gui::setThemedStyleSheet(btn,
        "QToolButton { background-color: transparent; color: @onSurfaceMuted; border: none; border-radius: 10px; padding: 2px 8px; font-size: 11px; text-align: left; }"
        "QToolButton:hover { background-color: @surfaceHighest; color: @onSurface; }"
        "QToolButton:pressed { background-color: @primaryContainer; color: @onPrimaryContainer; }"
    );
    if (onClick) {
        connect(btn, &QToolButton::clicked, onClick);
    } else {
        btn->setEnabled(false);
        btn->setToolTip(QString("%1 — Próximamente en onlyArchitect").arg(text));
    }
    smallButtonColumn()->addWidget(btn);
}

void RibbonGroupWidget::addDropdownButton(const QString& text, const QString& symbol, QMenu* menu) {
    auto btn = makeRibbonButton(this, text, symbol, SmallIconSize);
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ea::gui::setThemedStyleSheet(btn,
        "QToolButton { background-color: transparent; color: @onSurfaceVariant; border: 1px solid transparent; border-radius: 10px; padding: 2px 8px; font-size: 11px; }"
        "QToolButton:hover { background-color: @surfaceHighest; color: @onSurface; }"
        "QToolButton::menu-indicator { subcontrol-position: right center; }"
    );
    if (menu) {
        btn->setMenu(menu);
        btn->setPopupMode(QToolButton::InstantPopup);
    } else {
        btn->setEnabled(false);
        btn->setToolTip(QString("%1 — Próximamente en onlyArchitect").arg(text));
    }
    if (m_smallButtonsLayout) {
        smallButtonColumn()->addWidget(btn);
    } else {
        m_contentLayout->addWidget(btn);
    }
}

// ---------------------------------------------------------------------------
// RibbonBarWidget
// ---------------------------------------------------------------------------

RibbonBarWidget::RibbonBarWidget(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

void RibbonBarWidget::setConnectedUser(const QString& username) {
    if (!m_userLabel) return;
    const QString clean = username.trimmed();
    m_userLabel->setText(clean.isEmpty() ? QStringLiteral("Local") : clean);
}

void RibbonBarWidget::setupUi() {
    auto topLayout = new QVBoxLayout(this);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(0);

    // Barra superior de Pestañas Ribbon
    auto headerWidget = new QWidget(this);
    ea::gui::setThemedStyleSheet(headerWidget, "background-color: @surfaceContainer;");
    auto headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(M3Spacing::Medium, M3Spacing::ExtraSmall, M3Spacing::Medium, 0);
    headerLayout->setSpacing(M3Spacing::Small);

    m_ribbonTabBar = new QTabBar(headerWidget);
    m_ribbonTabBar->setDrawBase(false);
    m_ribbonTabBar->setUsesScrollButtons(true);
    m_ribbonTabBar->setExpanding(false);
    m_ribbonTabBar->setMinimumWidth(0);
    m_ribbonTabBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ea::gui::setThemedStyleSheet(m_ribbonTabBar,
        "QTabBar::tab { background-color: transparent; color: @onSurfaceMuted; padding: 6px 12px; font-size: 12px; font-weight: 500; margin: 2px 2px 0px 0px; border: none; border-top-left-radius: 10px; border-top-right-radius: 10px; }"
        "QTabBar::tab:selected { color: @primary; font-weight: 600; border-bottom: 2px solid @primary; background-color: @surfaceHigh; }"
        "QTabBar::tab:hover:!selected { color: @onSurface; background-color: @surfaceHigh; }"
    );

    QStringList tabs = {
        "Inicio", "Diseño", "Maquetación", "Código", "Simulación",
        "Ejecución", "Construcción", "Especialización", "Exportar", "Ajustes"
    };
    for (const auto& t : tabs) {
        m_ribbonTabBar->addTab(t);
    }

    // Hide empty unimplemented tabs from the visual bar so the user only sees real, working tools
    m_ribbonTabBar->setTabVisible(TabExecute, false);
    m_ribbonTabBar->setTabVisible(TabConstruct, false);
    m_ribbonTabBar->setTabVisible(TabSpecialize, false);

    headerLayout->addWidget(m_ribbonTabBar, 1);

    auto addIconLabel = [&](const QString& symbol, const QColor& tint) {
        auto icon = new QLabel(headerWidget);
        icon->setPixmap(M3Icons::icon(symbol, tint).pixmap(M3Icons::SmallSize, M3Icons::SmallSize));
        headerLayout->addWidget(icon);
    };
    addIconLabel("person", m3().primary);
    m_userLabel = new QLabel("Local", headerWidget);
    ea::gui::setThemedStyleSheet(m_userLabel, "color: @onSurfaceVariant; font-size: 11px; font-weight: 500; padding-right: 4px;");
    headerLayout->addWidget(m_userLabel);

    topLayout->addWidget(headerWidget);

    // Contenedor de contenido Ribbon según la pestaña seleccionada
    m_ribbonStack = new QStackedWidget(this);
    m_ribbonStack->setMinimumWidth(0);
    m_ribbonStack->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    ea::gui::setThemedStyleSheet(m_ribbonStack, "background-color: @surfaceHigh; border-bottom: 1px solid @outlineVariant;");
    m_ribbonStack->setFixedHeight(RibbonContentHeight);

    // One scrollable page per tab so the ribbon adapts cleanly to any screen width (1024px..4K)
    m_ribbonStack->addWidget(wrapScrollableRibbonPage(createInicioTab(), m_ribbonStack));
    m_ribbonStack->addWidget(wrapScrollableRibbonPage(createDesignTab(), m_ribbonStack));
    m_ribbonStack->addWidget(wrapScrollableRibbonPage(createLayoutTab(), m_ribbonStack));
    m_ribbonStack->addWidget(wrapScrollableRibbonPage(createDevelopTab(), m_ribbonStack));
    m_ribbonStack->addWidget(wrapScrollableRibbonPage(createSimulationTab(), m_ribbonStack));
    m_ribbonStack->addWidget(wrapScrollableRibbonPage(createPlaceholderTab(), m_ribbonStack));
    m_ribbonStack->addWidget(wrapScrollableRibbonPage(createPlaceholderTab(), m_ribbonStack));
    m_ribbonStack->addWidget(wrapScrollableRibbonPage(createPlaceholderTab(), m_ribbonStack));
    m_ribbonStack->addWidget(wrapScrollableRibbonPage(createPublishTab(), m_ribbonStack));
    m_ribbonStack->addWidget(wrapScrollableRibbonPage(createSettingsTab(), m_ribbonStack));

    topLayout->addWidget(m_ribbonStack);

    connect(m_ribbonTabBar, &QTabBar::currentChanged, this, &RibbonBarWidget::setCurrentTab);
}

void RibbonBarWidget::setCurrentTab(int index) {
    if (index < 0 || index >= m_ribbonStack->count()) index = TabStart;
    m_ribbonStack->setCurrentIndex(index);
    if (m_ribbonTabBar->currentIndex() != index) {
        m_ribbonTabBar->setCurrentIndex(index);
    }
}

int RibbonBarWidget::currentTab() const {
    return m_ribbonStack->currentIndex();
}

QWidget* RibbonBarWidget::createInicioTab() {
    auto tabWidget = new QWidget(this);
    auto tabLayout = new QHBoxLayout(tabWidget);
    tabLayout->setContentsMargins(M3Spacing::Medium, M3Spacing::ExtraSmall, M3Spacing::Medium, M3Spacing::ExtraSmall);
    tabLayout->setSpacing(M3Spacing::Small);

    // 1. Proyecto (las acciones básicas al frente y con iconos claros)
    auto grpProject = new RibbonGroupWidget("Proyecto", tabWidget);
    grpProject->addLargeButton("Abrir", "folder_open", [this]() { emit openFileClicked(); });
    grpProject->addLargeButton("Nuevo", "note_add", [this]() { emit newFileClicked(); });
    grpProject->addLargeButton("Guardar", "save", [this]() { emit saveFileClicked(); });
    grpProject->addSmallButton("Inicio", "home", [this]() { emit showStartPageClicked(); });
    grpProject->addSmallButton("Nuevo Diagrama", "schema", [this]() { emit newDiagramClicked(); });
    grpProject->addSmallButton("Model Builder", "construction", [this]() { emit showModelBuilderClicked(); });
    tabLayout->addWidget(grpProject);

    // 2. Equipo y Colaboración en Tiempo Real (LAN/VPN + Chat + Bitácora/Review + Pizarra)
    auto grpTeam = new RibbonGroupWidget("Equipo", tabWidget);
    grpTeam->addLargeButton("LAN / VPN", "dns", [this]() { emit serverConnectionClicked(); });
    grpTeam->addLargeButton("Chat", "chat", [this]() { emit showChatClicked(); });
    grpTeam->addSmallButton("Bitácora", "book", [this]() { emit showJournalClicked(); });
    grpTeam->addSmallButton("Discusión", "forum", [this]() { emit showDiscussClicked(); });
    grpTeam->addSmallButton("Revisión", "rate_review", [this]() { emit showReviewClicked(); });
    tabLayout->addWidget(grpTeam);

    // 3. Planificación (Kanban, Gantt, Calendario y Pizarra)
    auto grpPlan = new RibbonGroupWidget("Planificación", tabWidget);
    grpPlan->addLargeButton("Kanban", "view_kanban", [this]() { emit showKanbanClicked(); });
    grpPlan->addSmallButton("Gantt", "view_timeline", [this]() { emit showGanttClicked(); });
    grpPlan->addSmallButton("Calendario", "calendar_month", [this]() { emit showCalendarClicked(); });
    grpPlan->addSmallButton("Pizarra", "draw", [this]() { emit showWhiteboardClicked(); });
    tabLayout->addWidget(grpPlan);

    // 4. Explorar el Modelo
    auto grpExplore = new RibbonGroupWidget("Explorar", tabWidget);
    grpExplore->addSmallButton("Find in Project", "search", [this]() { emit showSearchClicked(); });
    grpExplore->addSmallButton("Traceability", "explore", [this]() { emit showTraceabilityClicked(); });
    grpExplore->addSmallButton("Consola SQL", "database", [this]() { emit showSqlConsoleClicked(); });
    tabLayout->addWidget(grpExplore);

    // 5. Ventanas y Paneles
    auto grpWindows = new RibbonGroupWidget("Ventanas", tabWidget);
    grpWindows->addLargeButton("Design", "dashboard", [this]() { emit windowSelectorRequested(0); });
    grpWindows->addSmallButton("Properties", "description", [this]() { emit windowSelectorRequested(1); });
    grpWindows->addSmallButton("Collaborate", "group", [this]() { emit windowSelectorRequested(2); });
    m_workspaceMenu = new QMenu(this);
    grpWindows->addDropdownButton("Workspace", "grid_view", m_workspaceMenu);
    tabLayout->addWidget(grpWindows);

    // 6. Vista Rápida
    auto grpAppearance = new RibbonGroupWidget("Vista", tabWidget);
    grpAppearance->addLargeButton("Tema", "palette", [this]() { emit visualStyleClicked(); });
    grpAppearance->addSmallButton("Pantalla Completa", "fullscreen", [this]() { emit fullScreenClicked(); });
    grpAppearance->addSmallButton("Acerca de", "help", [this]() { emit helpClicked(); });
    tabLayout->addWidget(grpAppearance);

    tabLayout->addStretch();
    return tabWidget;
}

QWidget* RibbonBarWidget::createDesignTab() {
    auto tabWidget = new QWidget(this);
    auto tabLayout = new QHBoxLayout(tabWidget);
    tabLayout->setContentsMargins(M3Spacing::Medium, M3Spacing::ExtraSmall, M3Spacing::Medium, M3Spacing::ExtraSmall);
    tabLayout->setSpacing(M3Spacing::Small);

    auto grpEdit = new RibbonGroupWidget("Editar", tabWidget);
    grpEdit->addSmallButton("Deshacer (Ctrl+Z)", "undo", [this]() { emit undoClicked(); });
    grpEdit->addSmallButton("Rehacer (Ctrl+Y)", "redo", [this]() { emit redoClicked(); });
    tabLayout->addWidget(grpEdit);

    auto grpDiagram = new RibbonGroupWidget("Diagrama", tabWidget);
    grpDiagram->addLargeButton("Nuevo Diagrama", "schema", [this]() { emit newDiagramClicked(); });
    grpDiagram->addLargeButton("Toolbox", "home_repair_service", [this]() { emit showToolboxClicked(); });
    grpDiagram->addLargeButton("Model Builder", "construction", [this]() { emit showModelBuilderClicked(); });
    tabLayout->addWidget(grpDiagram);

    auto grpElement = new RibbonGroupWidget("Agregar Elemento", tabWidget);
    grpElement->addLargeButton("Clase", "view_agenda", [this]() { emit addClassClicked(); });
    grpElement->addLargeButton("Componente", "extension", [this]() { emit addComponentClicked(); });
    grpElement->addSmallButton("Interfaz", "radio_button_unchecked", [this]() { emit addInterfaceClicked(); });
    grpElement->addSmallButton("Nota", "sticky_note_2", [this]() { emit addNoteClicked(); });
    grpElement->addSmallButton("Conectar", "link", [this]() { emit connectClicked(); });
    tabLayout->addWidget(grpElement);

    auto grpPackage = new RibbonGroupWidget("Análisis", tabWidget);
    grpPackage->addLargeButton("Matrix", "grid_on", [this]() { emit showRelationshipMatrixClicked(); });
    grpPackage->addLargeButton("Specification", "article", [this]() { emit showSpecificationClicked(); });
    grpPackage->addLargeButton("Validar UML", "check_circle", [this]() { emit validateModelClicked(); });
    tabLayout->addWidget(grpPackage);

    tabLayout->addStretch();
    return tabWidget;
}

QWidget* RibbonBarWidget::createLayoutTab() {
    auto tabWidget = new QWidget(this);
    auto tabLayout = new QHBoxLayout(tabWidget);
    tabLayout->setContentsMargins(M3Spacing::Medium, M3Spacing::ExtraSmall, M3Spacing::Medium, M3Spacing::ExtraSmall);
    tabLayout->setSpacing(M3Spacing::Small);

    auto grpDiagram = new RibbonGroupWidget("Zoom y Cámara", tabWidget);
    grpDiagram->addLargeButton("Acercar", "zoom_in", [this]() { emit zoomInClicked(); });
    grpDiagram->addLargeButton("Alejar", "zoom_out", [this]() { emit zoomOutClicked(); });
    grpDiagram->addSmallButton("Zoom 100%", "search", [this]() { emit zoomResetClicked(); });
    grpDiagram->addSmallButton("Ajustar a Pantalla", "fit_screen", [this]() { emit fitDiagramClicked(); });
    tabLayout->addWidget(grpDiagram);

    auto grpTools = new RibbonGroupWidget("Organizar", tabWidget);
    grpTools->addLargeButton("Diagram Layout", "auto_awesome", [this]() { emit autoLayoutClicked(); });
    tabLayout->addWidget(grpTools);

    tabLayout->addStretch();
    return tabWidget;
}

QWidget* RibbonBarWidget::createDevelopTab() {
    auto tabWidget = new QWidget(this);
    auto tabLayout = new QHBoxLayout(tabWidget);
    tabLayout->setContentsMargins(M3Spacing::Medium, M3Spacing::ExtraSmall, M3Spacing::Medium, M3Spacing::ExtraSmall);
    tabLayout->setSpacing(M3Spacing::Small);

    auto grpSource = new RibbonGroupWidget("Código y Documentación", tabWidget);
    grpSource->addLargeButton("Generar Código", "code", [this]() { emit generateCodeClicked(); });
    tabLayout->addWidget(grpSource);

    tabLayout->addStretch();
    return tabWidget;
}

QWidget* RibbonBarWidget::createSimulationTab() {
    auto tabWidget = new QWidget(this);
    auto tabLayout = new QHBoxLayout(tabWidget);
    tabLayout->setContentsMargins(M3Spacing::Medium, M3Spacing::ExtraSmall, M3Spacing::Medium, M3Spacing::ExtraSmall);
    tabLayout->setSpacing(M3Spacing::Small);

    auto grpSim = new RibbonGroupWidget("Simulación", tabWidget);
    grpSim->addLargeButton("Iniciar", "play_arrow", [this]() { emit simulationStartClicked(); });
    grpSim->addLargeButton("Paso", "skip_next", [this]() { emit simulationStepClicked(); });
    grpSim->addLargeButton("Pausar", "pause", [this]() { emit simulationPauseClicked(); });
    grpSim->addLargeButton("Detener", "stop", [this]() { emit simulationStopClicked(); });
    tabLayout->addWidget(grpSim);

    tabLayout->addStretch();
    return tabWidget;
}

QWidget* RibbonBarWidget::createPublishTab() {
    auto tabWidget = new QWidget(this);
    auto tabLayout = new QHBoxLayout(tabWidget);
    tabLayout->setContentsMargins(M3Spacing::Medium, M3Spacing::ExtraSmall, M3Spacing::Medium, M3Spacing::ExtraSmall);
    tabLayout->setSpacing(M3Spacing::Small);

    auto grpImage = new RibbonGroupWidget("Imagen del Diagrama", tabWidget);
    grpImage->addLargeButton("SVG", "image", [this]() { emit exportSvgClicked(); });
    grpImage->addLargeButton("PNG", "photo", [this]() { emit exportPngClicked(); });
    tabLayout->addWidget(grpImage);

    auto grpExchange = new RibbonGroupWidget("Intercambio", tabWidget);
    grpExchange->addLargeButton("PlantUML", "eco", [this]() { emit exportPlantUmlClicked(); });
    grpExchange->addLargeButton("JSON", "data_object", [this]() { emit exportJsonClicked(); });
    grpExchange->addLargeButton("Exportar XMI", "upload_file", [this]() { emit exportXmiClicked(); });
    grpExchange->addLargeButton("Importar XMI", "download", [this]() { emit importXmiClicked(); });
    tabLayout->addWidget(grpExchange);

    tabLayout->addStretch();
    return tabWidget;
}

QWidget* RibbonBarWidget::createSettingsTab() {
    auto tabWidget = new QWidget(this);
    auto tabLayout = new QHBoxLayout(tabWidget);
    tabLayout->setContentsMargins(M3Spacing::Medium, M3Spacing::ExtraSmall, M3Spacing::Medium, M3Spacing::ExtraSmall);
    tabLayout->setSpacing(M3Spacing::Small);

    auto grpLook = new RibbonGroupWidget("Apariencia", tabWidget);
    grpLook->addLargeButton("Color / Tema", "palette", [this]() { emit visualStyleClicked(); });
    grpLook->addLargeButton("Pantalla Completa", "fullscreen", [this]() { emit fullScreenClicked(); });
    tabLayout->addWidget(grpLook);

    auto grpTools = new RibbonGroupWidget("Herramientas", tabWidget);
    grpTools->addLargeButton("Multijugador", "dns", [this]() { emit serverConnectionClicked(); });
    grpTools->addLargeButton("Consola SQL", "database", [this]() { emit showSqlConsoleClicked(); });
    grpTools->addLargeButton("Acerca de", "help", [this]() { emit helpClicked(); });
    tabLayout->addWidget(grpTools);

    tabLayout->addStretch();
    return tabWidget;
}

QWidget* RibbonBarWidget::createPlaceholderTab() {
    auto tabWidget = new QWidget(this);
    auto tabLayout = new QHBoxLayout(tabWidget);
    tabLayout->setContentsMargins(12, 4, 6, 4);
    auto label = new QLabel("Esta cinta todavía no tiene funciones implementadas.", tabWidget);
    ea::gui::setThemedStyleSheet(label, "color: @onSurfaceDisabled; font-size: 11px; font-style: italic;");
    tabLayout->addWidget(label);
    tabLayout->addStretch();
    return tabWidget;
}

} // namespace ea::gui
