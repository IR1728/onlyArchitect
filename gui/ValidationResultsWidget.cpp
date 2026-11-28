#include "ValidationResultsWidget.hpp"
#include "DarkTheme.hpp"
#include "M3Icons.hpp"
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
constexpr int ElementIdRole = Qt::UserRole;

QString plural(size_t n, const char* one, const char* many) {
    return QString("%1 %2").arg(n).arg(n == 1 ? one : many);
}
} // namespace

QString validation_summary(const ea::validation::ValidationReport& report) {
    using ea::validation::Severity;
    if (report.clean()) {
        return QString("Validación UML: sin problemas (%1, %2)")
            .arg(plural(report.elementsChecked, "elemento", "elementos"),
                 plural(report.connectorsChecked, "relación", "relaciones"));
    }
    return QString("Validación UML: %1, %2")
        .arg(plural(report.count(Severity::Error), "error", "errores"),
             plural(report.count(Severity::Warning), "advertencia", "advertencias"));
}

ValidationResultsWidget::ValidationResultsWidget(QWidget* parent) : QWidget(parent) {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto header = new QHBoxLayout();
    m_summary = new QLabel("Pulsa Validar para comprobar el modelo.", this);
    auto validateBtn = new QPushButton("Validar modelo", this);
    ea::gui::M3Icons::decorate(validateBtn, "check_circle");
    header->addWidget(m_summary, 1);
    header->addWidget(validateBtn);
    layout->addLayout(header);

    m_list = new QTreeWidget(this);
    m_list->setRootIsDecorated(false);
    m_list->setAlternatingRowColors(true);
    m_list->setHeaderLabels({"Severidad", "Regla", "Mensaje"});
    m_list->header()->setStretchLastSection(true);
    layout->addWidget(m_list, 1);

    connect(validateBtn, &QPushButton::clicked, this, &ValidationResultsWidget::validateRequested);
    connect(m_list, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item) { onItemActivated(item); });
}

void ValidationResultsWidget::showReport(const ea::validation::ValidationReport& report) {
    m_summary->setText(validation_summary(report));
    m_list->clear();
    QList<QTreeWidgetItem*> rows;
    rows.reserve(static_cast<qsizetype>(report.issues.size()));
    for (const auto& issue : report.issues) {
        const bool error = issue.severity == ea::validation::Severity::Error;
        auto item = new QTreeWidgetItem({QString::fromStdString(to_string(issue.severity)),
                                         QString::fromStdString(issue.rule), QString::fromStdString(issue.message)});
        item->setIcon(0, error ? ea::gui::M3Icons::icon("error", ea::gui::m3().error) : ea::gui::M3Icons::icon("warning", ea::gui::m3().warning));
        item->setData(0, ElementIdRole, issue.elementId);
        item->setToolTip(2, QString::fromStdString(issue.message));
        rows.append(item);
    }
    m_list->addTopLevelItems(rows); // one insertion instead of one per issue
    m_list->resizeColumnToContents(0);
    m_list->resizeColumnToContents(1);
}

int ValidationResultsWidget::issueCount() const {
    return m_list->topLevelItemCount();
}

void ValidationResultsWidget::onItemActivated(QTreeWidgetItem* item) {
    const int elementId = item ? item->data(0, ElementIdRole).toInt() : 0;
    if (elementId > 0) emit elementActivated(elementId);
}
