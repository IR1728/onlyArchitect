#include "WelcomeWidget.hpp"
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
    leftSidebar->setStyleSheet(
        "QFrame { background-color: #13161f; border-right: 1px solid #242936; }"
        "QPushButton { text-align: left; padding: 10px 16px; border: none; border-radius: 6px; font-weight: bold; color: #9ca3af; background: transparent; }"
        "QPushButton:hover { background-color: #1e222d; color: #ffffff; }"
        "QPushButton:checked { background-color: #242936; color: #38bdf8; border-left: 3px solid #007acc; }"
    );

    auto sideLayout = new QVBoxLayout(leftSidebar);
    sideLayout->setContentsMargins(12, 20, 12, 20);
    sideLayout->setSpacing(6);

    auto logoLabel = new QLabel("<b>onlyArchitect</b> Studio", leftSidebar);
    logoLabel->setStyleSheet("font-size: 16px; color: #38bdf8; padding-bottom: 15px; border: none;");
    sideLayout->addWidget(logoLabel);

    auto btnHome = new QPushButton("🏠 Bienvenida", leftSidebar);
    btnHome->setCheckable(true);
    btnHome->setChecked(true);
    auto btnProjects = new QPushButton("📁 Proyectos", leftSidebar);
    auto btnRecent = new QPushButton("🕒 Recientes", leftSidebar);
    auto btnSettings = new QPushButton("⚙️ Configuración", leftSidebar);

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
    scrollArea->setStyleSheet("QScrollArea { background-color: #161922; border: none; }");

    auto contentWidget = new QWidget(scrollArea);
    contentWidget->setStyleSheet("background-color: #161922;");
    auto contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(36, 28, 36, 28);
    contentLayout->setSpacing(24);

    // Header Title
    auto titleBox = new QVBoxLayout();
    auto titleLabel = new QLabel("Bienvenido a <b>onlyArchitect Studio</b>", contentWidget);
    titleLabel->setStyleSheet("font-size: 24px; color: #f3f4f6; font-weight: 600;");
    auto subtitleLabel = new QLabel("Plataforma nativa de modelado UML y arquitectura de software", contentWidget);
    subtitleLabel->setStyleSheet("font-size: 13px; color: #9ca3af;");
    titleBox->addWidget(titleLabel);
    titleBox->addWidget(subtitleLabel);
    contentLayout->addLayout(titleBox);

    // Quick Action Bar
    auto quickActionsLayout = new QHBoxLayout();
    quickActionsLayout->setSpacing(12);

    auto openBtn = new QPushButton("📂 Abrir Repositorio (.qea, .eapx)...", contentWidget);
    openBtn->setStyleSheet("QPushButton { background-color: #007acc; color: white; font-weight: bold; padding: 10px 18px; border-radius: 6px; }"
                          "QPushButton:hover { background-color: #0088e2; }");
    connect(openBtn, &QPushButton::clicked, this, &WelcomeWidget::openFileBrowseRequested);

    auto newBtn = new QPushButton("➕ Nuevo Repositorio", contentWidget);
    newBtn->setStyleSheet("QPushButton { background-color: #242936; color: #f3f4f6; font-weight: bold; padding: 10px 18px; border: 1px solid #343c4e; border-radius: 6px; }"
                         "QPushButton:hover { background-color: #2e3546; border: 1px solid #38bdf8; }");
    connect(newBtn, &QPushButton::clicked, this, &WelcomeWidget::newProjectRequested);

    auto transferBtn = new QPushButton("🔄 Migrar EAP/EAPX a QEA", contentWidget);
    transferBtn->setStyleSheet("QPushButton { background-color: #242936; color: #f3f4f6; font-weight: bold; padding: 10px 18px; border: 1px solid #343c4e; border-radius: 6px; }"
                              "QPushButton:hover { background-color: #2e3546; border: 1px solid #38bdf8; }");
    connect(transferBtn, &QPushButton::clicked, this, &WelcomeWidget::transferRequested);

    quickActionsLayout->addWidget(openBtn);
    quickActionsLayout->addWidget(newBtn);
    quickActionsLayout->addWidget(transferBtn);
    quickActionsLayout->addStretch();
    contentLayout->addLayout(quickActionsLayout);

    // Section 1: Recent Projects (Cards Grid)
    auto recentSectionTitle = new QLabel("Proyectos Recientes", contentWidget);
    recentSectionTitle->setStyleSheet("font-size: 17px; font-weight: bold; color: #f3f4f6; margin-top: 10px;");
    contentLayout->addWidget(recentSectionTitle);

    m_projectsGrid = new QGridLayout();
    m_projectsGrid->setSpacing(16);
    contentLayout->addLayout(m_projectsGrid);

    // Section 2: Aprendizaje y Guías (matches prototype cards at bottom)
    auto learnSectionTitle = new QLabel("Aprendizaje y Recursos", contentWidget);
    learnSectionTitle->setStyleSheet("font-size: 17px; font-weight: bold; color: #f3f4f6; margin-top: 14px;");
    contentLayout->addWidget(learnSectionTitle);

    auto learnGrid = new QGridLayout();
    learnGrid->setSpacing(16);

    learnGrid->addWidget(createActionCard("Noticias y Actualizaciones", "Soporte C++20, renderizado vectorial Cairo y motor nativo de diagramas interactivos.", "📰", 1), 0, 0);
    learnGrid->addWidget(createActionCard("Arquitectura y Modelado", "Modelo de objetos de alto rendimiento y almacenamiento relacional SQLite.", "🔬", 2), 0, 1);
    learnGrid->addWidget(createActionCard("Guía de Modelado UML", "Documentación sobre diagramas de clases, casos de uso, secuencia, auto-layout y código.", "📚", 3), 0, 2);

    contentLayout->addLayout(learnGrid);
    contentLayout->addStretch();

    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea, 1);
}

QWidget* WelcomeWidget::createProjectCard(const QString& title, const QString& subtitle, const QString& path, const QString& iconEmoji) {
    auto card = new QFrame(this);
    card->setObjectName("ProjectCard");
    card->setCursor(Qt::PointingHandCursor);
    card->setStyleSheet(
        "QFrame#ProjectCard { background-color: #1e222d; border: 1px solid #2b3140; border-radius: 8px; padding: 14px; }"
        "QFrame#ProjectCard:hover { background-color: #242936; border: 1px solid #38bdf8; }"
    );
    card->setFixedSize(260, 130);

    auto layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(6);

    auto headerLayout = new QHBoxLayout();
    auto iconLabel = new QLabel(iconEmoji, card);
    iconLabel->setStyleSheet("font-size: 24px;");
    auto titleLabel = new QLabel(title, card);
    titleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #f3f4f6;");
    titleLabel->setWordWrap(true);
    headerLayout->addWidget(iconLabel);
    headerLayout->addWidget(titleLabel, 1);
    layout->addLayout(headerLayout);

    auto subLabel = new QLabel(subtitle, card);
    subLabel->setStyleSheet("font-size: 11px; color: #9ca3af;");
    subLabel->setWordWrap(true);
    layout->addWidget(subLabel);
    layout->addStretch();

    auto openBtn = new QPushButton("Abrir Proyecto", card);
    openBtn->setStyleSheet("QPushButton { font-size: 11px; padding: 4px 8px; background-color: #2b3140; color: #38bdf8; border: none; border-radius: 4px; }"
                          "QPushButton:hover { background-color: #007acc; color: white; }");
    layout->addWidget(openBtn, 0, Qt::AlignRight);

    connect(openBtn, &QPushButton::clicked, [this, path]() {
        emit openProjectRequested(path);
    });

    return card;
}

QWidget* WelcomeWidget::createActionCard(const QString& title, const QString& subtitle, const QString& iconEmoji, int /*actionType*/) {
    auto card = new QFrame(this);
    card->setObjectName("ActionCard");
    card->setStyleSheet(
        "QFrame#ActionCard { background-color: #1a1e28; border: 1px solid #2b3140; border-radius: 8px; padding: 14px; }"
        "QFrame#ActionCard:hover { background-color: #222734; border: 1px solid #4f586c; }"
    );
    card->setFixedSize(260, 110);

    auto layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(6);

    auto headerLayout = new QHBoxLayout();
    auto iconLabel = new QLabel(iconEmoji, card);
    iconLabel->setStyleSheet("font-size: 20px;");
    auto titleLabel = new QLabel(title, card);
    titleLabel->setStyleSheet("font-size: 12px; font-weight: bold; color: #f3f4f6;");
    headerLayout->addWidget(iconLabel);
    headerLayout->addWidget(titleLabel, 1);
    layout->addLayout(headerLayout);

    auto subLabel = new QLabel(subtitle, card);
    subLabel->setStyleSheet("font-size: 11px; color: #9ca3af;");
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

    std::vector<std::pair<QString, QString>> candidateProjects = {
        {"my_model.qea", "my_model.qea"},
        {"test_onlyarchitect.qea", "/tmp/test_onlyarchitect.qea"}
    };

    int col = 0;
    int row = 0;

    for (const auto& [name, path] : candidateProjects) {
        if (QFile::exists(path)) {
            QFileInfo fi(path);
            QString subtitle = QString("%1 • %2 KB").arg(fi.suffix().toUpper()).arg(fi.size() / 1024);
            QString icon = (fi.suffix().toLower() == "eapx" || fi.suffix().toLower() == "eap") ? "🗄️" : "📊";

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
    newCard->setStyleSheet(
        "QFrame#NewProjectCard { background-color: #1a1e28; border: 2px dashed #2b3140; border-radius: 8px; }"
        "QFrame#NewProjectCard:hover { border-color: #38bdf8; background-color: #202430; }"
    );
    newCard->setFixedSize(260, 130);

    auto ncLayout = new QVBoxLayout(newCard);
    ncLayout->setAlignment(Qt::AlignCenter);
    auto ncIcon = new QLabel("➕", newCard);
    ncIcon->setStyleSheet("font-size: 26px; color: #38bdf8;");
    ncIcon->setAlignment(Qt::AlignCenter);
    auto ncText = new QLabel("Crear Nuevo Proyecto", newCard);
    ncText->setStyleSheet("font-size: 13px; font-weight: bold; color: #f3f4f6;");
    ncText->setAlignment(Qt::AlignCenter);
    ncLayout->addWidget(ncIcon);
    ncLayout->addWidget(ncText);

    auto newCardBtn = new QPushButton("Iniciar", newCard);
    newCardBtn->setStyleSheet("QPushButton { font-size: 11px; padding: 4px 12px; background-color: #007acc; color: white; border-radius: 4px; }");
    connect(newCardBtn, &QPushButton::clicked, this, &WelcomeWidget::newProjectRequested);
    ncLayout->addWidget(newCardBtn, 0, Qt::AlignCenter);

    m_projectsGrid->addWidget(newCard, row, col);
}
