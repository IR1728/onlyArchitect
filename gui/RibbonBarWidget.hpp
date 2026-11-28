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

    void addLargeButton(const QString& text, const QString& symbol, const std::function<void()>& onClick = nullptr);
    void addSmallButton(const QString& text, const QString& symbol, const std::function<void()>& onClick = nullptr);
    void addDropdownButton(const QString& text, const QString& symbol, QMenu* menu = nullptr);

private:
    static constexpr int SmallButtonsPerColumn = 3;
    QVBoxLayout* smallButtonColumn();

    QVBoxLayout* m_mainLayout{nullptr};
    QHBoxLayout* m_contentLayout{nullptr};
    QVBoxLayout* m_smallButtonsLayout{nullptr};
    QLabel* m_titleLabel{nullptr};
};

class RibbonBarWidget : public QWidget {
    Q_OBJECT
public:
    enum TabIndex {
        TabStart = 0, TabDesign, TabLayout, TabDevelop, TabSimulate,
        TabExecute, TabConstruct, TabSpecialize, TabPublish, TabSettings
    };

    explicit RibbonBarWidget(QWidget* parent = nullptr);
    int currentTab() const;
    QMenu* workspaceMenu() const { return m_workspaceMenu; }
    void setConnectedUser(const QString& username);

signals:
    void openFileClicked();
    void newFileClicked();
    void saveFileClicked();
    void showStartPageClicked();
    void serverConnectionClicked();
    void showWhiteboardClicked();
    void showChatClicked();
    void showJournalClicked();
    void showDiscussClicked();
    void showReviewClicked();
    void showKanbanClicked();
    void showGanttClicked();
    void showCalendarClicked();
    void showSqlConsoleClicked();
    void visualStyleClicked();
    void fullScreenClicked();
    void helpClicked();
    void showModelBuilderClicked();
    void windowSelectorRequested(int selector);
    void perspectiveChanged(const QString& perspective);

    void showSearchClicked();
    void showTraceabilityClicked();
    void showRelationshipMatrixClicked();
    void showSpecificationClicked();
    void showToolboxClicked();

    void addClassClicked();
    void addInterfaceClicked();
    void addComponentClicked();
    void addNoteClicked();
    void connectClicked();

    void newDiagramClicked();
    void validateModelClicked();
    void undoClicked();
    void redoClicked();

    void zoomInClicked();
    void zoomOutClicked();
    void zoomResetClicked();
    void fitDiagramClicked();
    void autoLayoutClicked();

    void generateCodeClicked();

    void simulationStartClicked();
    void simulationStepClicked();
    void simulationPauseClicked();
    void simulationStopClicked();

    void exportSvgClicked();
    void exportPngClicked();
    void exportPlantUmlClicked();
    void exportJsonClicked();
    void exportXmiClicked();
    void importXmiClicked();

public slots:
    void setCurrentTab(int index);

private:
    void setupUi();
    QWidget* createInicioTab();
    QWidget* createDesignTab();
    QWidget* createLayoutTab();
    QWidget* createDevelopTab();
    QWidget* createSimulationTab();
    QWidget* createPublishTab();
    QWidget* createSettingsTab();
    QWidget* createPlaceholderTab();

    QTabBar* m_ribbonTabBar{nullptr};
    QStackedWidget* m_ribbonStack{nullptr};
    QLabel* m_perspectivesLabel{nullptr};
    QLabel* m_userLabel{nullptr};
    QMenu* m_workspaceMenu{nullptr};
};

} // namespace ea::gui
