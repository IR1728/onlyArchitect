#pragma once

#include <QWidget>
#include "ea/validation/ModelValidator.hpp"

class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

// "Validación UML" window: lists the issues of the last model validation.
class ValidationResultsWidget : public QWidget {
    Q_OBJECT
public:
    explicit ValidationResultsWidget(QWidget* parent = nullptr);

    void showReport(const ea::validation::ValidationReport& report);
    int issueCount() const;

signals:
    void elementActivated(int elementId);
    void validateRequested();

private:
    void onItemActivated(QTreeWidgetItem* item);

    QLabel* m_summary{nullptr};
    QTreeWidget* m_list{nullptr};
};

// One-line summary for the status bar, e.g. "Validación UML: 2 errores, 1 advertencia"
QString validation_summary(const ea::validation::ValidationReport& report);
