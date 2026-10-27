#include "StartPageWidget.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QFrame>
#include <QFile>

namespace ea::gui {

StartPageWidget::StartPageWidget(QWidget* parent)
    : QWidget(parent) {
    setupUi();
    refreshRecentProjects();
}

void StartPageWidget::setupUi() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Contenedor principal con fondo oscuro
    auto bgWidget = new QWidget(this);
    bgWidget->setStyleSheet("background-color: #171c26;");
    auto bgLayout = new QVBoxLayout(bgWidget);
    bgLayout->setContentsMargins(40, 30, 40, 40);
    bgLayout->setSpacing(24);

    // Fila superior de navegación de la Start Page: flechas < >
    auto navHeader = new QHBoxLayout();
    auto btnBack = new QToolButton(bgWidget);
    btnBack->setText("❮");
    btnBack->setStyleSheet("color: #64748b; background: transparent; border: none; font-size: 14px;");
    auto btnForward = new QToolButton(bgWidget);
    btnForward->setText("❯");
    btnForward->setStyleSheet("color: #64748b; background: transparent; border: none; font-size: 14px;");
    navHeader->addWidget(btnBack);
    navHeader->addWidget(btnForward);
    navHeader->addStretch();
    bgLayout->addLayout(navHeader);

    // Tres columnas: Abrir | Recientes | Fijados
    auto colsLayout = new QHBoxLayout();
    colsLayout->setSpacing(48);

    // ==========================================
    // Columna 1: ABRIR
    // ==========================================
    auto colOpen = new QVBoxLayout();
    colOpen->setSpacing(14);

    auto lblAbrir = new QLabel("Abrir", bgWidget);
    lblAbrir->setStyleSheet("color: #93c5fd; font-size: 20px; font-weight: bold; margin-bottom: 6px;");
    colOpen->addWidget(lblAbrir);

    // Sección Personal Project
    auto lblPersonal = new QLabel("Personal Project", bgWidget);
    lblPersonal->setStyleSheet("color: #60a5fa; font-size: 14px; font-weight: 600;");
    colOpen->addWidget(lblPersonal);

    colOpen->addWidget(createActionLink("Open File", "📂", [this]() { emit openFileBrowseRequested(); }));
    colOpen->addWidget(createActionLink("Create New", "📄", [this]() { emit newProjectRequested(); }));

    colOpen->addSpacing(16);

    // Sección Team Repository
    auto lblTeam = new QLabel("Team Repository", bgWidget);
    lblTeam->setStyleSheet("color: #60a5fa; font-size: 14px; font-weight: 600;");
    colOpen->addWidget(lblTeam);

    colOpen->addWidget(createActionLink("Cloud Connection", "☁️", [this]() {}));
    colOpen->addWidget(createActionLink("Server Connection", "🖥️", [this]() {}));
    colOpen->addWidget(createActionLink("Custom Data Source", "🗄️", [this]() {}));
    colOpen->addWidget(createActionLink("URL", "🔗", [this]() {}));

    colOpen->addSpacing(16);

    auto btnManage = new QPushButton("Manage Projects...", bgWidget);
    btnManage->setStyleSheet(
        "QPushButton { background-color: #242c3d; color: #cbd5e1; border: 1px solid #38455e; border-radius: 4px; padding: 7px 16px; font-size: 12px; }"
        "QPushButton:hover { background-color: #2d374d; color: #ffffff; border-color: #38bdf8; }"
    );
    connect(btnManage, &QPushButton::clicked, this, &StartPageWidget::manageProjectsRequested);
    colOpen->addWidget(btnManage);

    colOpen->addStretch();
    colsLayout->addLayout(colOpen, 1);

    // ==========================================
    // Columna 2: RECIENTES ▾
    // ==========================================
    auto colRecent = new QVBoxLayout();
    colRecent->setSpacing(14);

    auto lblRecentHeader = new QLabel("Recientes ▾", bgWidget);
    lblRecentHeader->setStyleSheet("color: #93c5fd; font-size: 20px; font-weight: bold; margin-bottom: 6px;");
    colRecent->addWidget(lblRecentHeader);

    m_recentsLayout = new QVBoxLayout();
    m_recentsLayout->setSpacing(8);
    colRecent->addLayout(m_recentsLayout);
    colRecent->addStretch();

    colsLayout->addLayout(colRecent, 1);

    // ==========================================
    // Columna 3: FIJADOS
    // ==========================================
    auto colPinned = new QVBoxLayout();
    colPinned->setSpacing(14);

    auto lblPinned = new QLabel("Fijados", bgWidget);
    lblPinned->setStyleSheet("color: #93c5fd; font-size: 20px; font-weight: bold; margin-bottom: 6px;");
    colPinned->addWidget(lblPinned);

    m_pinnedLayout = new QVBoxLayout();
    m_pinnedLayout->setSpacing(8);
    colPinned->addLayout(m_pinnedLayout);
    colPinned->addStretch();

    colsLayout->addLayout(colPinned, 1);

    bgLayout->addLayout(colsLayout);
    mainLayout->addWidget(bgWidget);
}

QWidget* StartPageWidget::createActionLink(const QString& title, const QString& iconEmoji, const std::function<void()>& onClick) {
    auto btn = new QToolButton(this);
    btn->setText(QString("%1  %2").arg(iconEmoji, title));
    btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    btn->setStyleSheet(
        "QToolButton { background: transparent; color: #cbd5e1; border: none; font-size: 13px; text-align: left; padding: 3px 0px; }"
        "QToolButton:hover { color: #38bdf8; text-decoration: underline; }"
    );
    if (onClick) {
        connect(btn, &QToolButton::clicked, onClick);
    }
    return btn;
}

void StartPageWidget::refreshRecentProjects() {
    // Limpiar layout de recientes
    QLayoutItem* item;
    while ((item = m_recentsLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    struct ProjectItem {
        QString name;
        QString path;
    };

    std::vector<ProjectItem> candidates = {
        {"EAExample", "/home/iran/EA/re/bin/EA_Trial/EAExample.qea"},
        {"DiagramaCasosUsoExamen.qea", "/home/iran/CicloAgostoDiciembre2026/Diseño/DiagramaCasosUsoExamen.qea"},
        {"DiagramaCasosUsoExamen.eapx", "/home/iran/CicloAgostoDiciembre2026/Diseño/DiagramaCasosUsoExamen.eapx"}
    };

    for (const auto& p : candidates) {
        auto linkBtn = new QToolButton(this);
        linkBtn->setText(QString("📄  %1").arg(p.name));
        linkBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        linkBtn->setStyleSheet(
            "QToolButton { background: transparent; color: #cbd5e1; border: none; font-size: 13px; text-align: left; padding: 4px 0px; }"
            "QToolButton:hover { color: #38bdf8; text-decoration: underline; }"
        );
        connect(linkBtn, &QToolButton::clicked, this, [this, path = p.path]() {
            emit openProjectRequested(path);
        });
        m_recentsLayout->addWidget(linkBtn);
    }

    // Fijados
    while ((item = m_pinnedLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    auto pinBtn = new QToolButton(this);
    pinBtn->setText("📌  EAExample (Model Reference)");
    pinBtn->setStyleSheet(
        "QToolButton { background: transparent; color: #94a3b8; border: none; font-size: 13px; text-align: left; padding: 4px 0px; }"
        "QToolButton:hover { color: #38bdf8; text-decoration: underline; }"
    );
    connect(pinBtn, &QToolButton::clicked, this, [this]() {
        emit openProjectRequested("/home/iran/EA/re/bin/EA_Trial/EAExample.qea");
    });
    m_pinnedLayout->addWidget(pinBtn);
}

} // namespace ea::gui
