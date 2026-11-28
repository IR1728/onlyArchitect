#include "WelcomeWidget.hpp"
#include "DarkTheme.hpp"
#include "M3Icons.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QFileInfo>
#include <QFile>

WelcomeWidget::WelcomeWidget(QWidget* parent)
    : QWidget(parent) {
    setupUi();
    refreshRecentProjects();
}

void WelcomeWidget::setupUi() {
    auto mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. Left Navigation Sidebar (matches prototype View 1 left bar)
    auto leftSidebar = new QFrame(this);
    leftSidebar->setFixedWidth(200);
    ea::gui::setThemedStyleSheet(leftSidebar,
        "QFrame { background-color: @surfaceLowest; border-right: 1px solid @outlineVariant; }"
        "QPushButton { text-align: left; padding: 10px 16px; border: none; border-radius: 6px; font-weight: bold; color: @onSurfaceMuted; background: transparent; }"
        "QPushButton:hover { background-color: @surfaceLow; color: @onSurface; }"
        "QPushButton:checked { background-color: @surfaceContainer; color: @primary; border-left: 3px solid @primaryStrong; }"
    );

    auto sideLayout = new QVBoxLayout(leftSidebar);
    sideLayout->setContentsMargins(12, 20, 12, 20);
    sideLayout->setSpacing(6);

    auto logoLabel = new QLabel("<b>onlyArchitect</b> Studio", leftSidebar);
    ea::gui::setThemedStyleSheet(logoLabel, "font-size: 16px; color: @primary; padding-bottom: 15px; border: none;");
    sideLayout->addWidget(logoLabel);

    auto btnHome = new QPushButton("Bienvenida", leftSidebar);
    ea::gui::M3Icons::decorate(btnHome, "home");
    btnHome->setCheckable(true);
    btnHome->setChecked(true);
    auto btnProjects = new QPushButton("Proyectos", leftSidebar);
    ea::gui::M3Icons::decorate(btnProjects, "folder");
    auto btnRecent = new QPushButton("Recientes", leftSidebar);
    ea::gui::M3Icons::decorate(btnRecent, "history");
    auto btnSettings = new QPushButton("Configuración", leftSidebar);
    ea::gui::M3Icons::decorate(btnSettings, "settings");

    sideLayout->addWidget(btnHome);
    sideLayout->addWidget(btnProjects);
    sideLayout->addWidget(btnRecent);
    sideLayout->addStretch();
    sideLayout->addWidget(btnSettings);

    mainLayout->addWidget(leftSidebar);

    // 2. Right Content Area (Scrollable cards)
    auto scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    ea::gui::setThemedStyleSheet(scrollArea, "QScrollArea { background-color: @surface; border: none; }");

    auto contentWidget = new QWidget(scrollArea);
    ea::gui::setThemedStyleSheet(contentWidget, "background-color: @surface;");
    auto contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(36, 28, 36, 28);
    contentLayout->setSpacing(24);

    // Header Title
    auto titleBox = new QVBoxLayout();
    auto titleLabel = new QLabel("Bienvenido a <b>onlyArchitect Studio</b>", contentWidget);
    ea::gui::setThemedStyleSheet(titleLabel, "font-size: 24px; color: @onSurface; font-weight: 600;");
    auto subtitleLabel = new QLabel("Plataforma nativa de modelado UML y arquitectura de software", contentWidget);
    ea::gui::setThemedStyleSheet(subtitleLabel, "font-size: 13px; color: @onSurfaceMuted;");
    titleBox->addWidget(titleLabel);
    titleBox->addWidget(subtitleLabel);
    contentLayout->addLayout(titleBox);

    // Quick Action Bar
    auto quickActionsLayout = new QHBoxLayout();
    quickActionsLayout->setSpacing(12);

    auto openBtn = new QPushButton("Abrir Repositorio (.qea, .eapx)...", contentWidget);
    ea::gui::M3Icons::decorate(openBtn, "folder_open");
    ea::gui::setThemedStyleSheet(openBtn, "QPushButton { background-color: @primaryStrong; color: white; font-weight: bold; padding: 10px 18px; border-radius: 6px; }"
                          "QPushButton:hover { background-color: @primaryStrong; }");
    connect(openBtn, &QPushButton::clicked, this, &WelcomeWidget::openFileBrowseRequested);

    auto newBtn = new QPushButton("Nuevo Repositorio", contentWidget);
    ea::gui::M3Icons::decorate(newBtn, "add");
    ea::gui::setThemedStyleSheet(newBtn, "QPushButton { background-color: @surfaceContainer; color: @onSurface; font-weight: bold; padding: 10px 18px; border: 1px solid @outline; border-radius: 6px; }"
                         "QPushButton:hover { background-color: @surfaceHighest; border: 1px solid @primary; }");
    connect(newBtn, &QPushButton::clicked, this, &WelcomeWidget::newProjectRequested);

    auto transferBtn = new QPushButton("Migrar EAP/EAPX a QEA", contentWidget);
    ea::gui::M3Icons::decorate(transferBtn, "sync_alt");
    ea::gui::setThemedStyleSheet(transferBtn, "QPushButton { background-color: @surfaceContainer; color: @onSurface; font-weight: bold; padding: 10px 18px; border: 1px solid @outline; border-radius: 6px; }"
                              "QPushButton:hover { background-color: @surfaceHighest; border: 1px solid @primary; }");
    connect(transferBtn, &QPushButton::clicked, this, &WelcomeWidget::transferRequested);

    quickActionsLayout->addWidget(openBtn);
    quickActionsLayout->addWidget(newBtn);
    quickActionsLayout->addWidget(transferBtn);
    quickActionsLayout->addStretch();
    contentLayout->addLayout(quickActionsLayout);

    // Section 1: Recent Projects (Cards Grid)
    auto recentSectionTitle = new QLabel("Proyectos Recientes", contentWidget);
    ea::gui::setThemedStyleSheet(recentSectionTitle, "font-size: 17px; font-weight: bold; color: @onSurface; margin-top: 10px;");
    contentLayout->addWidget(recentSectionTitle);

    m_projectsGrid = new QGridLayout();
    m_projectsGrid->setSpacing(16);
    contentLayout->addLayout(m_projectsGrid);

    // Section 2: Aprendizaje y Guías (matches prototype cards at bottom)
    auto learnSectionTitle = new QLabel("Aprendizaje y Recursos", contentWidget);
    ea::gui::setThemedStyleSheet(learnSectionTitle, "font-size: 17px; font-weight: bold; color: @onSurface; margin-top: 14px;");
    contentLayout->addWidget(learnSectionTitle);

    auto learnGrid = new QGridLayout();
    learnGrid->setSpacing(16);

    learnGrid->addWidget(createActionCard("Noticias y Actualizaciones", "Soporte C++20, renderizado vectorial Cairo y motor nativo de diagramas interactivos.", "info", 1), 0, 0);
    learnGrid->addWidget(createActionCard("Arquitectura y Modelado", "Modelo de objetos de alto rendimiento y almacenamiento relacional SQLite.", "account_tree", 2), 0, 1);
    learnGrid->addWidget(createActionCard("Guía de Modelado UML", "Documentación sobre diagramas de clases, casos de uso, secuencia, auto-layout y código.", "menu_book", 3), 0, 2);

    contentLayout->addLayout(learnGrid);
    contentLayout->addStretch();

    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea, 1);
}

QWidget* WelcomeWidget::createProjectCard(const QString& title, const QString& subtitle, const QString& path, const QString& symbol) {
    auto card = new QFrame(this);
    card->setObjectName("ProjectCard");
    card->setCursor(Qt::PointingHandCursor);
    ea::gui::setThemedStyleSheet(card,
        "QFrame#ProjectCard { background-color: @surfaceLow; border: 1px solid @outlineVariant; border-radius: 8px; padding: 14px; }"
        "QFrame#ProjectCard:hover { background-color: @surfaceContainer; border: 1px solid @primary; }"
    );
    card->setFixedSize(260, 130);

    auto layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(6);

    auto headerLayout = new QHBoxLayout();
    auto iconLabel = ea::gui::M3Icons::label(symbol, card, ea::gui::M3Icons::LargeSize, ea::gui::m3().primary);
    auto titleLabel = new QLabel(title, card);
    ea::gui::setThemedStyleSheet(titleLabel, "font-size: 13px; font-weight: bold; color: @onSurface;");
    titleLabel->setWordWrap(true);
    headerLayout->addWidget(iconLabel);
    headerLayout->addWidget(titleLabel, 1);
    layout->addLayout(headerLayout);

    auto subLabel = new QLabel(subtitle, card);
    ea::gui::setThemedStyleSheet(subLabel, "font-size: 11px; color: @onSurfaceMuted;");
    subLabel->setWordWrap(true);
    layout->addWidget(subLabel);
    layout->addStretch();

    auto openBtn = new QPushButton("Abrir Proyecto", card);
    ea::gui::setThemedStyleSheet(openBtn, "QPushButton { font-size: 11px; padding: 4px 8px; background-color: @surfaceHigh; color: @primary; border: none; border-radius: 4px; }"
                          "QPushButton:hover { background-color: @primaryStrong; color: white; }");
    layout->addWidget(openBtn, 0, Qt::AlignRight);

    connect(openBtn, &QPushButton::clicked, [this, path]() {
        emit openProjectRequested(path);
    });

    return card;
}

QWidget* WelcomeWidget::createActionCard(const QString& title, const QString& subtitle, const QString& symbol, int /*actionType*/) {
    auto card = new QFrame(this);
    card->setObjectName("ActionCard");
    ea::gui::setThemedStyleSheet(card,
        "QFrame#ActionCard { background-color: @surfaceLow; border: 1px solid @outlineVariant; border-radius: 8px; padding: 14px; }"
        "QFrame#ActionCard:hover { background-color: @surfaceContainer; border: 1px solid @onSurfaceDisabled; }"
    );
    card->setFixedSize(260, 110);

    auto layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(6);

    auto headerLayout = new QHBoxLayout();
    auto iconLabel = ea::gui::M3Icons::label(symbol, card, ea::gui::M3Icons::LargeSize, ea::gui::m3().tertiary);
    auto titleLabel = new QLabel(title, card);
    ea::gui::setThemedStyleSheet(titleLabel, "font-size: 12px; font-weight: bold; color: @onSurface;");
    headerLayout->addWidget(iconLabel);
    headerLayout->addWidget(titleLabel, 1);
    layout->addLayout(headerLayout);

    auto subLabel = new QLabel(subtitle, card);
    ea::gui::setThemedStyleSheet(subLabel, "font-size: 11px; color: @onSurfaceMuted;");
    subLabel->setWordWrap(true);
    layout->addWidget(subLabel);
    layout->addStretch();

    return card;
}

void WelcomeWidget::refreshRecentProjects() {
    // Clear previous items
    QLayoutItem* item;
    while ((item = m_projectsGrid->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    const QString exampleModelPath = QFile::exists("examples/my_model.qea")
                                         ? "examples/my_model.qea"
                                         : "../examples/my_model.qea";

    std::vector<std::pair<QString, QString>> candidateProjects = {
        {"my_model.qea", exampleModelPath},
        {"test_onlyarchitect.qea", "/tmp/test_onlyarchitect.qea"}
    };

    int col = 0;
    int row = 0;

    for (const auto& [name, path] : candidateProjects) {
        if (QFile::exists(path)) {
            QFileInfo fi(path);
            QString subtitle = QString("%1 • %2 KB").arg(fi.suffix().toUpper()).arg(fi.size() / 1024);
            QString icon = (fi.suffix().toLower() == "eapx" || fi.suffix().toLower() == "eap") ? "database" : "schema";

            m_projectsGrid->addWidget(createProjectCard(name, subtitle, path, icon), row, col);
            col++;
            if (col >= 3) {
                col = 0;
                row++;
            }
        }
    }

    // Add "New Project" card
    auto newCard = new QFrame(this);
    newCard->setObjectName("NewProjectCard");
    ea::gui::setThemedStyleSheet(newCard,
        "QFrame#NewProjectCard { background-color: @surfaceLow; border: 2px dashed @outlineVariant; border-radius: 8px; }"
        "QFrame#NewProjectCard:hover { border-color: @primary; background-color: @surfaceContainer; }"
    );
    newCard->setFixedSize(260, 130);

    auto ncLayout = new QVBoxLayout(newCard);
    ncLayout->setAlignment(Qt::AlignCenter);
    auto ncIcon = new QLabel(newCard);
    ncIcon->setPixmap(ea::gui::M3Icons::icon("add", ea::gui::m3().primary).pixmap(32, 32));
    ncIcon->setAlignment(Qt::AlignCenter);
    auto ncText = new QLabel("Crear Nuevo Proyecto", newCard);
    ea::gui::setThemedStyleSheet(ncText, "font-size: 13px; font-weight: bold; color: @onSurface;");
    ncText->setAlignment(Qt::AlignCenter);
    ncLayout->addWidget(ncIcon);
    ncLayout->addWidget(ncText);

    auto newCardBtn = new QPushButton("Iniciar", newCard);
    ea::gui::setThemedStyleSheet(newCardBtn, "QPushButton { font-size: 11px; padding: 4px 12px; background-color: @primaryStrong; color: white; border-radius: 4px; }");
    connect(newCardBtn, &QPushButton::clicked, this, &WelcomeWidget::newProjectRequested);
    ncLayout->addWidget(newCardBtn, 0, Qt::AlignCenter);

    m_projectsGrid->addWidget(newCard, row, col);
}
