#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QVBoxLayout>
#include "ea/core/Types.hpp"

namespace ea::gui {

class ToolboxWidget : public QWidget {
    Q_OBJECT
public:
    explicit ToolboxWidget(QWidget* parent = nullptr);

signals:
    void elementToolSelected(ea::ObjectType type, const QString& stereotype);
    void connectorToolSelected(ea::ConnectorType type, const QString& stereotype);

private slots:
    void onItemClicked(QTreeWidgetItem* item, int column);
    void onSearchChanged(const QString& text);

private:
    void setupUi();
    void populateToolbox();

    QLineEdit* m_searchEdit{nullptr};
    QTreeWidget* m_tree{nullptr};
};

} // namespace ea::gui
