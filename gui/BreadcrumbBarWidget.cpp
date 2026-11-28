#include "BreadcrumbBarWidget.hpp"
#include "DarkTheme.hpp"
#include "M3Icons.hpp"

namespace ea::gui {

BreadcrumbBarWidget::BreadcrumbBarWidget(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

void BreadcrumbBarWidget::setupUi() {
    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(M3Spacing::Medium, M3Spacing::ExtraSmall, M3Spacing::Medium, M3Spacing::ExtraSmall);
    layout->setSpacing(M3Spacing::Small);

    ea::gui::setThemedStyleSheet(this, "background-color: @surface; border-bottom: 1px solid @outlineVariant;");

    // Navegación: atrás, adelante, inicio (Material Symbols)
    auto createNavBtn = [this](const QString& symbol) -> QToolButton* {
        auto btn = new QToolButton(this);
        ea::gui::M3Icons::decorate(btn, symbol);
        btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        ea::gui::setThemedStyleSheet(btn,
            "QToolButton { background: transparent; border: none; border-radius: 14px; padding: 4px; }"
            "QToolButton:hover { background: @surfaceHigh; }"
        );
        return btn;
    };

    m_btnBack = createNavBtn("arrow_back");
    m_btnBack->setToolTip("Back");
    connect(m_btnBack, &QToolButton::clicked, this, &BreadcrumbBarWidget::navigateBack);
    layout->addWidget(m_btnBack);

    m_btnForward = createNavBtn("arrow_forward");
    m_btnForward->setToolTip("Forward");
    connect(m_btnForward, &QToolButton::clicked, this, &BreadcrumbBarWidget::navigateForward);
    layout->addWidget(m_btnForward);

    m_btnHome = createNavBtn("home");
    m_btnHome->setToolTip("Home");
    connect(m_btnHome, &QToolButton::clicked, this, &BreadcrumbBarWidget::navigateHome);
    layout->addWidget(m_btnHome);

    layout->addWidget(ea::gui::M3Icons::label("chevron_right", this, ea::gui::M3Icons::SmallSize, m3().onSurfaceDisabled));

    // Migas de pan del modelo y diagrama activos
    m_modelLabel = new QLabel("Sin proyecto ▾", this);
    m_modelLabel->setFont(M3Typography::bodyMedium());
    ea::gui::setThemedStyleSheet(m_modelLabel, "color: @onSurfaceVariant;");
    layout->addWidget(m_modelLabel);

    layout->addWidget(ea::gui::M3Icons::label("chevron_right", this, ea::gui::M3Icons::SmallSize, m3().onSurfaceDisabled));

    m_packageLabel = new QLabel("Inicio ▾", this);
    m_packageLabel->setFont(M3Typography::labelLarge());
    ea::gui::setThemedStyleSheet(m_packageLabel, "color: @primary; background-color: @primaryContainer; border-radius: 12px; padding: 2px 12px;");
    layout->addWidget(m_packageLabel);

    layout->addStretch();

    // Buscador derecho: Find Package
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Find Package");
    ea::gui::M3Icons::addSearchIcon(m_searchEdit);
    m_searchEdit->setClearButtonEnabled(true);
    ea::gui::setThemedStyleSheet(m_searchEdit,
        "QLineEdit { background-color: @surfaceContainer; color: @onSurface; border: 1px solid @outline; border-radius: 14px; padding: 4px 12px; font-size: 12px; min-width: 180px; }"
        "QLineEdit:focus { border-color: @primary; }"
    );
    connect(m_searchEdit, &QLineEdit::textChanged, this, &BreadcrumbBarWidget::searchPackage);
    layout->addWidget(m_searchEdit);
}

void BreadcrumbBarWidget::setPath(const QString& modelName, const QString& packageName) {
    m_modelLabel->setText(QString("%1 ▾").arg(modelName.isEmpty() ? "Sin proyecto" : modelName));
    m_packageLabel->setText(QString("%1 ▾").arg(packageName.isEmpty() ? "Inicio" : packageName));
}

} // namespace ea::gui
