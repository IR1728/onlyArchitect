#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QToolButton>
#include <QLabel>
#include <QLineEdit>

namespace ea::gui {

class BreadcrumbBarWidget : public QWidget {
    Q_OBJECT
public:
    explicit BreadcrumbBarWidget(QWidget* parent = nullptr);

    void setPath(const QString& modelName, const QString& packageName);

signals:
    void navigateBack();
    void navigateForward();
    void navigateHome();
    void searchPackage(const QString& query);

private:
    void setupUi();

    QToolButton* m_btnBack{nullptr};
    QToolButton* m_btnForward{nullptr};
    QToolButton* m_btnHome{nullptr};
    QLabel* m_modelLabel{nullptr};
    QLabel* m_packageLabel{nullptr};
    QLineEdit* m_searchEdit{nullptr};
};

} // namespace ea::gui
