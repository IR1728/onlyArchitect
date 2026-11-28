#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QVBoxLayout>
#include <memory>
#include "ea/repository/Repository.hpp"

enum class TreeItemType {
    Model = 1,
    Package = 2,
    Element = 3,
    Diagram = 4
};

class ModelTreeWidget : public QWidget {
    Q_OBJECT
public:
    explicit ModelTreeWidget(ea::Repository& repo, QWidget* parent = nullptr);

    void populateTree();
    void clear();
    // Selects and reveals a diagram node (e.g. one just created)
    bool selectDiagram(int diagramId);

signals:
    void diagramDoubleClicked(int diagramId);
    void elementSelected(int elementId);
    void elementEditRequested(int elementId);
    void packageSelected(int packageId);
    void newDiagramRequested(int packageId);
    void newPackageRequested(int parentPackageId);
    void renamePackageRequested(int packageId);
    void renameElementRequested(int elementId);
    void deleteElementRequested(int elementId);
    void renameDiagramRequested(int diagramId);
    void refineUseCaseRequested(int elementId);

private slots:
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onItemClicked(QTreeWidgetItem* item, int column);
    void onFilterChanged(const QString& text);
    void onContextMenuRequested(const QPoint& pos);

private:
    void addPackageToTree(QTreeWidgetItem* parentItem, const ea::Package& pkg);
    bool filterTreeItem(QTreeWidgetItem* item, const QString& filter);

    ea::Repository& m_repo;
    QTreeWidget* m_tree{nullptr};
    QLineEdit* m_filterEdit{nullptr};
};
