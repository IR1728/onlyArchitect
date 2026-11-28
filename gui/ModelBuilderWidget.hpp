#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QTextBrowser>
#include <QTabWidget>
#include <memory>
#include "ea/repository/Repository.hpp"

namespace ea::gui {

class ModelBuilderWidget : public QWidget {
    Q_OBJECT
public:
    explicit ModelBuilderWidget(ea::Repository& repo, QWidget* parent = nullptr);

signals:
    void modelCreated(int newPackageId, int newDiagramId);

private slots:
    void onItemSelectionChanged();
    void onCreateModelClicked();
    void onFilterChanged(const QString& text);

private:
    void setupUi();
    void populatePerspectivesTree();

    ea::Repository& m_repo;

    QLineEdit* m_parentEdit{nullptr};
    QCheckBox* m_combineCheck{nullptr};
    QCheckBox* m_customizeCheck{nullptr};
    QPushButton* m_createBtn{nullptr};
    QPushButton* m_helpBtn{nullptr};

    QLineEdit* m_filterEdit{nullptr};
    QTreeWidget* m_tree{nullptr};
    QTextBrowser* m_previewBrowser{nullptr};
    QTabWidget* m_bottomTabs{nullptr};
};

} // namespace ea::gui
