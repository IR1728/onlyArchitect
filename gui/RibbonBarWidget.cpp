#include "RibbonBarWidget.hpp"
#include <QFrame>

namespace ea::gui {

RibbonGroupWidget::RibbonGroupWidget(const QString& title, QWidget* parent)
    : QWidget(parent) {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(6, 4, 6, 2);
    m_mainLayout->setSpacing(2);

    m_contentLayout = new QHBoxLayout();
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(4);
    m_mainLayout->addLayout(m_contentLayout, 1);

    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet("color: #64748b; font-size: 10px; font-weight: normal; margin-top: 2px;");
    m_mainLayout->addWidget(m_titleLabel);

    setStyleSheet(
        "RibbonGroupWidget { border-right: 1px solid #283042; background: transparent; }"
    );
}

void RibbonGroupWidget::addLargeButton(const QString& text, const QString& iconEmoji, const std::function<void()>& onClick) {
    auto btn = new QToolButton(this);
    btn->setText(QString("%1\n%2").arg(iconEmoji, text));
    btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    btn->setStyleSheet(
        "QToolButton { background-color: transparent; color: #cbd5e1; border: 1px solid transparent; border-radius: 4px; padding: 4px 6px; font-size: 11px; text-align: center; min-width: 52px; }"
        "QToolButton:hover { background-color: #242c3d; color: #ffffff; border: 1px solid #38455e; }"
        "QToolButton:pressed { background-color: #1e2534; }"
    );
    if (onClick) {
        connect(btn, &QToolButton::clicked, onClick);
    }
    m_contentLayout->addWidget(btn);
}

void RibbonGroupWidget::addSmallButton(const QString& text, const QString& iconEmoji, const std::function<void()>& onClick) {
    if (!m_smallButtonsLayout) {
        m_smallButtonsLayout = new QVBoxLayout();
        m_smallButtonsLayout->setContentsMargins(0, 0, 0, 0);
        m_smallButtonsLayout->setSpacing(2);
        m_contentLayout->addLayout(m_smallButtonsLayout);
    }

    auto btn = new QToolButton(this);
    btn->setText(QString("%1 %2").arg(iconEmoji, text));
    btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    btn->setStyleSheet(
        "QToolButton { background-color: transparent; color: #94a3b8; border: none; border-radius: 3px; padding: 2px 6px; font-size: 11px; text-align: left; }"
        "QToolButton:hover { background-color: #242c3d; color: #ffffff; }"
    );
    if (onClick) {
        connect(btn, &QToolButton::clicked, onClick);
    }
    m_smallButtonsLayout->addWidget(btn);
}

void RibbonGroupWidget::addDropdownButton(const QString& text, const QString& iconEmoji, QMenu* menu) {
    auto btn = new QToolButton(this);
    btn->setText(QString("%1 %2 ▾").arg(iconEmoji, text));
    btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    btn->setStyleSheet(
        "QToolButton { background-color: transparent; color: #cbd5e1; border: 1px solid transparent; border-radius: 4px; padding: 3px 6px; font-size: 11px; }"
        "QToolButton:hover { background-color: #242c3d; color: #ffffff; border: 1px solid #38455e; }"
    );
    if (menu) {
        btn->setMenu(menu);
        btn->setPopupMode(QToolButton::InstantPopup);
    }
    if (m_smallButtonsLayout) {
        m_smallButtonsLayout->addWidget(btn);
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

void RibbonBarWidget::setupUi() {
    auto topLayout = new QVBoxLayout(this);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(0);

    // Barra superior de Pestañas Ribbon
    auto headerWidget = new QWidget(this);
    headerWidget->setStyleSheet("background-color: #1a1f2b; border-bottom: 1px solid #283042;");
    auto headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(6, 2, 8, 0);
    headerLayout->setSpacing(8);

    m_ribbonTabBar = new QTabBar(headerWidget);
    m_ribbonTabBar->setDrawBase(false);
    m_ribbonTabBar->setStyleSheet(
        "QTabBar::tab { background-color: transparent; color: #94a3b8; padding: 6px 14px; font-size: 12px; font-weight: normal; margin-right: 2px; border: none; }"
        "QTabBar::tab:selected { color: #ffffff; font-weight: bold; border-bottom: 2px solid #38bdf8; background-color: #222938; }"
        "QTabBar::tab:hover:!selected { color: #f1f5f9; background-color: #1e2433; }"
    );

    QStringList tabs = {
        "Inicio", "Diseño", "Maquetación", "Desarrollo", "Simulación",
        "Ejecución", "Construcción", "Especialización", "Publicar", "Ajustes"
    };
    for (const auto& t : tabs) {
        m_ribbonTabBar->addTab(t);
    }

    headerLayout->addWidget(m_ribbonTabBar);
    headerLayout->addStretch();

    // Perspectiva y Usuario a la derecha
    m_perspectivesLabel = new QLabel("🌐 All Perspectives", headerWidget);
    m_perspectivesLabel->setStyleSheet("color: #38bdf8; font-size: 11px; padding-right: 12px;");
    headerLayout->addWidget(m_perspectivesLabel);

    m_userLabel = new QLabel("👤 User", headerWidget);
    m_userLabel->setStyleSheet("color: #cbd5e1; font-size: 11px; padding-right: 6px;");
    headerLayout->addWidget(m_userLabel);

    topLayout->addWidget(headerWidget);

    // Contenedor de contenido Ribbon según la pestaña seleccionada
    m_ribbonStack = new QStackedWidget(this);
    m_ribbonStack->setStyleSheet("background-color: #1a1f2b; border-bottom: 2px solid #283042;");
    m_ribbonStack->setFixedHeight(92);

    m_ribbonStack->addWidget(createInicioTab());
    m_ribbonStack->addWidget(createDesignTab());
    m_ribbonStack->addWidget(createSimulationTab());

    topLayout->addWidget(m_ribbonStack);

    connect(m_ribbonTabBar, &QTabBar::currentChanged, this, &RibbonBarWidget::setCurrentTab);
}

void RibbonBarWidget::setCurrentTab(int index) {
    if (index >= 0 && index < m_ribbonStack->count()) {
        m_ribbonStack->setCurrentIndex(index);
    } else {
        m_ribbonStack->setCurrentIndex(0);
    }
}

QWidget* RibbonBarWidget::createInicioTab() {
    auto tabWidget = new QWidget(this);
    auto tabLayout = new QHBoxLayout(tabWidget);
    tabLayout->setContentsMargins(6, 4, 6, 4);
    tabLayout->setSpacing(2);

    // 1. All Windows
    auto grpWindows = new RibbonGroupWidget("All Windows", tabWidget);
    grpWindows->addLargeButton("Design", "📐", [this]() { emit showDesignClicked(); });
    grpWindows->addLargeButton("Properties", "📑", [this]() { emit showPropertiesClicked(); });
    grpWindows->addLargeButton("Collaborate", "👥", [this]() { emit showCollaborateClicked(); });
    grpWindows->addLargeButton("Publish", "📤", [this]() { emit saveFileClicked(); });
    grpWindows->addDropdownButton("Workspace", "🔲");
    grpWindows->addDropdownButton("Perspective", "🎯");
    tabLayout->addWidget(grpWindows);

    // 2. Explore
    auto grpExplore = new RibbonGroupWidget("Explore", tabWidget);
    grpExplore->addLargeButton("Search Portals", "🔍", [this]() {});
    tabLayout->addWidget(grpExplore);

    // 3. Appearance
    auto grpAppearance = new RibbonGroupWidget("Appearance", tabWidget);
    grpAppearance->addLargeButton("Preferences", "⚙️", [this]() {});
    grpAppearance->addSmallButton("Full Screen", "🖥️");
    grpAppearance->addSmallButton("Visual Style", "🎨");
    grpAppearance->addSmallButton("View", "👁️");
    tabLayout->addWidget(grpAppearance);

    // 4. Collaborate
    auto grpCollab = new RibbonGroupWidget("Collaborate", tabWidget);
    grpCollab->addLargeButton("Journal", "📔");
    grpCollab->addLargeButton("Discuss", "💬");
    grpCollab->addSmallButton("Review", "📋");
    grpCollab->addSmallButton("Chat", "🗨️");
    grpCollab->addSmallButton("Mail", "✉️");
    tabLayout->addWidget(grpCollab);

    // 5. Calendar
    auto grpCal = new RibbonGroupWidget("Calendar", tabWidget);
    grpCal->addSmallButton("Model Diagram", "📊");
    grpCal->addSmallButton("Model Library", "📚");
    grpCal->addSmallButton("Calendar", "📅");
    tabLayout->addWidget(grpCal);

    // 6. Personal
    auto grpPersonal = new RibbonGroupWidget("Personal", tabWidget);
    grpPersonal->addSmallButton("My Workdesk", "💻");
    grpPersonal->addSmallButton("Working Sets", "📂");
    grpPersonal->addSmallButton("My Diagram", "📈");
    grpPersonal->addSmallButton("Model Builder", "🏗️", [this]() { emit showModelBuilderClicked(); });
    grpPersonal->addSmallButton("My Kanban", "📋");
    grpPersonal->addSmallButton("My Gantt", "⏱️");
    tabLayout->addWidget(grpPersonal);

    // 7. Help
    auto grpHelp = new RibbonGroupWidget("Help", tabWidget);
    grpHelp->addLargeButton("Help", "🛟", [this]() {});
    grpHelp->addSmallButton("Home Page", "🏠", [this]() { emit showStartPageClicked(); });
    grpHelp->addSmallButton("Libraries", "📖");
    grpHelp->addSmallButton("Register", "🔑");
    tabLayout->addWidget(grpHelp);

    tabLayout->addStretch();
    return tabWidget;
}

QWidget* RibbonBarWidget::createDesignTab() {
    auto tabWidget = new QWidget(this);
    auto tabLayout = new QHBoxLayout(tabWidget);
    tabLayout->setContentsMargins(6, 4, 6, 4);
    tabLayout->setSpacing(2);

    auto grpDiagram = new RibbonGroupWidget("Diagram", tabWidget);
    grpDiagram->addLargeButton("Layout", "⚡");
    grpDiagram->addLargeButton("Insert Class", "📦");
    grpDiagram->addLargeButton("Insert Component", "🧩");
    grpDiagram->addLargeButton("Connect", "🔗");
    tabLayout->addWidget(grpDiagram);

    auto grpTools = new RibbonGroupWidget("Tools", tabWidget);
    grpTools->addLargeButton("Toolbox", "🧰");
    grpTools->addLargeButton("Model Builder", "🏗️", [this]() { emit showModelBuilderClicked(); });
    tabLayout->addWidget(grpTools);

    tabLayout->addStretch();
    return tabWidget;
}

QWidget* RibbonBarWidget::createSimulationTab() {
    auto tabWidget = new QWidget(this);
    auto tabLayout = new QHBoxLayout(tabWidget);
    tabLayout->setContentsMargins(6, 4, 6, 4);
    tabLayout->setSpacing(2);

    auto grpSim = new RibbonGroupWidget("Execution Controls", tabWidget);
    grpSim->addLargeButton("Start", "▶️");
    grpSim->addLargeButton("Step", "⏭️");
    grpSim->addLargeButton("Pause", "⏸️");
    grpSim->addLargeButton("Stop", "⏹️");
    tabLayout->addWidget(grpSim);

    tabLayout->addStretch();
    return tabWidget;
}

} // namespace ea::gui
