#include "BreadcrumbBarWidget.hpp"

namespace ea::gui {

BreadcrumbBarWidget::BreadcrumbBarWidget(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

void BreadcrumbBarWidget::setupUi() {
    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 3, 8, 3);
    layout->setSpacing(6);

    setStyleSheet("background-color: #171c26; border-bottom: 1px solid #283042;");

    // Flechas de navegación ◀ ▶ 🏠
    auto createNavBtn = [this](const QString& text) -> QToolButton* {
        auto btn = new QToolButton(this);
        btn->setText(text);
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        btn->setStyleSheet(
            "QToolButton { background: transparent; color: #94a3b8; border: none; font-size: 13px; padding: 2px 5px; }"
            "QToolButton:hover { color: #38bdf8; background: #242c3d; border-radius: 3px; }"
        );
        return btn;
    };

    m_btnBack = createNavBtn("◀");
    connect(m_btnBack, &QToolButton::clicked, this, &BreadcrumbBarWidget::navigateBack);
    layout->addWidget(m_btnBack);

    m_btnForward = createNavBtn("▶");
    connect(m_btnForward, &QToolButton::clicked, this, &BreadcrumbBarWidget::navigateForward);
    layout->addWidget(m_btnForward);

    m_btnHome = createNavBtn("🏠");
    connect(m_btnHome, &QToolButton::clicked, this, &BreadcrumbBarWidget::navigateHome);
    layout->addWidget(m_btnHome);

    auto sep1 = new QLabel("/", this);
    sep1->setStyleSheet("color: #475569; font-size: 12px;");
    layout->addWidget(sep1);

    // Migas de pan: Example Model ▾ / Getting Started ▾
    m_modelLabel = new QLabel("Example Model ▾", this);
    m_modelLabel->setStyleSheet("color: #cbd5e1; font-size: 11.5px; font-weight: 500;");
    layout->addWidget(m_modelLabel);

    auto sep2 = new QLabel("/", this);
    sep2->setStyleSheet("color: #475569; font-size: 12px;");
    layout->addWidget(sep2);

    m_packageLabel = new QLabel("Getting Started ▾", this);
    m_packageLabel->setStyleSheet("color: #38bdf8; font-size: 11.5px; font-weight: bold;");
    layout->addWidget(m_packageLabel);

    layout->addStretch();

    // Buscador derecho: 🔍 Find Package
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("🔍 Find Package");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setStyleSheet(
        "QLineEdit { background-color: #1a1f2b; color: #f1f5f9; border: 1px solid #283042; border-radius: 4px; padding: 3px 8px; font-size: 11px; min-width: 170px; }"
        "QLineEdit:focus { border-color: #38bdf8; }"
    );
    connect(m_searchEdit, &QLineEdit::textChanged, this, &BreadcrumbBarWidget::searchPackage);
    layout->addWidget(m_searchEdit);
}

void BreadcrumbBarWidget::setPath(const QString& modelName, const QString& packageName) {
    m_modelLabel->setText(QString("%1 ▾").arg(modelName.isEmpty() ? "Example Model" : modelName));
    m_packageLabel->setText(QString("%1 ▾").arg(packageName.isEmpty() ? "Getting Started" : packageName));
}

} // namespace ea::gui
