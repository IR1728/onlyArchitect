#pragma once

#include <QWidget>
#include <QTabBar>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QLabel>
#include <QMenu>

namespace ea::gui {

class RibbonGroupWidget : public QWidget {
    Q_OBJECT
public:
    explicit RibbonGroupWidget(const QString& title, QWidget* parent = nullptr);

    void addLargeButton(const QString& text, const QString& iconEmoji, const std::function<void()>& onClick = nullptr);
    void addSmallButton(const QString& text, const QString& iconEmoji, const std::function<void()>& onClick = nullptr);
    void addDropdownButton(const QString& text, const QString& iconEmoji, QMenu* menu = nullptr);

private:
    QVBoxLayout* m_mainLayout{nullptr};
    QHBoxLayout* m_contentLayout{nullptr};
    QVBoxLayout* m_smallButtonsLayout{nullptr};
    QLabel* m_titleLabel{nullptr};
};

class RibbonBarWidget : public QWidget {
    Q_OBJECT
public:
    explicit RibbonBarWidget(QWidget* parent = nullptr);

signals:
    void openFileClicked();
    void newFileClicked();
    void saveFileClicked();
    void showStartPageClicked();
    void showModelBuilderClicked();
    void showPropertiesClicked();
    void showDesignClicked();
    void showCollaborateClicked();
    void perspectiveChanged(const QString& perspective);

public slots:
    void setCurrentTab(int index);

private:
    void setupUi();
    QWidget* createInicioTab();
    QWidget* createDesignTab();
    QWidget* createSimulationTab();

    QTabBar* m_ribbonTabBar{nullptr};
    QStackedWidget* m_ribbonStack{nullptr};
    QLabel* m_perspectivesLabel{nullptr};
    QLabel* m_userLabel{nullptr};
};

} // namespace ea::gui
