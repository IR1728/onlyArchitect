#pragma once

#include <QCalendarWidget>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QWidget>
#include <array>
#include "ea/repository/Repository.hpp"

namespace ea::gui {

// ============================================================================
// 1. Live LAN/VPN Team Chat Dock
// ============================================================================
class TeamChatWidget : public QWidget {
    Q_OBJECT
public:
    explicit TeamChatWidget(QWidget* parent = nullptr);

    void setConnectionState(bool connected, const QStringList& peers, const QString& localUser);
    void appendChatMessage(const QString& author, const QString& text, qint64 timestampMs);
    void appendSystemNotice(const QString& text);
    void clearMessages();

signals:
    void sendMessageRequested(const QString& text);
    void connectServerRequested();

private:
    void onSendClicked();

    QLabel* m_statusLabel{nullptr};
    QPushButton* m_connectBtn{nullptr};
    QTextBrowser* m_transcript{nullptr};
    QLineEdit* m_input{nullptr};
    QPushButton* m_sendBtn{nullptr};
    bool m_connected{false};
    QString m_localUser;
};

// ============================================================================
// 2. Journal / Discussions / Reviews Dock (persisted in .qea)
// ============================================================================
class CollabHubWidget : public QWidget {
    Q_OBJECT
public:
    enum TabIndex { TabJournal = 0, TabDiscuss = 1, TabReview = 2 };

    explicit CollabHubWidget(ea::Repository& repo, QWidget* parent = nullptr);

    void setCurrentElement(int elementId);
    void setAuthorName(const QString& author) { m_author = author; }
    void showTab(int index);
    void refresh();

signals:
    void collaborationDataChanged();

private:
    struct Pane {
        QListWidget* list{nullptr};
        QLineEdit* titleEdit{nullptr};
        QPlainTextEdit* bodyEdit{nullptr};
        QPushButton* approveBtn{nullptr};
        QPushButton* changesBtn{nullptr};
    };

    QWidget* buildPane(const QString& kind, const QString& titlePlaceholder, const QString& bodyPlaceholder,
                       bool withReviewActions, Pane& outPane);
    void populatePane(const QString& kind, Pane& pane);
    void addEntryFromPane(const QString& kind, Pane& pane);
    void updateSelectedReviewStatus(const QString& newStatus);
    void deleteSelectedFrom(const QString& kind, Pane& pane);

    ea::Repository& m_repo;
    int m_elementId{0};
    QString m_author{"Arquitecto"};
    QLabel* m_scopeLabel{nullptr};
    QCheckBox* m_filterElementCheck{nullptr};
    QTabWidget* m_tabs{nullptr};
    Pane m_journalPane;
    Pane m_discussPane;
    Pane m_reviewPane;
};

// ============================================================================
// 3. Project Planner Workspace: Kanban | Gantt | Calendar (persisted in .qea)
// ============================================================================
class ProjectPlannerWidget : public QWidget {
    Q_OBJECT
public:
    enum ViewIndex { ViewKanban = 0, ViewGantt = 1, ViewCalendar = 2 };

    explicit ProjectPlannerWidget(ea::Repository& repo, QWidget* parent = nullptr);

    void showView(int index);
    void setAuthorName(const QString& author) { m_author = author; }
    void refreshFromRepository();

signals:
    void plannerDataChanged();
    void elementActivated(int elementId);

private:
    QWidget* buildKanbanPage();
    QWidget* buildGanttPage();
    QWidget* buildCalendarPage();

    void refreshKanban();
    void refreshGantt();
    void refreshCalendar();

    void moveSelectedCard(int columnIndex, int delta);
    void addKanbanCard(int columnIndex);
    void addGanttTask(const QString& presetDueDate = QString());
    void importElementsToGantt();
    void advanceSelectedTaskProgress();
    void deleteSelectedTask();

    ea::Repository& m_repo;
    QString m_author{"Arquitecto"};

    QStackedWidget* m_stack{nullptr};
    std::array<QPushButton*, 3> m_navButtons{};

    // Kanban columns: Proposed, Approved, Implemented, Validated
    static constexpr int kKanbanCols = 4;
    std::array<QListWidget*, kKanbanCols> m_kanbanLists{};
    std::array<QLabel*, kKanbanCols> m_kanbanHeaders{};

    // Gantt
    QTableWidget* m_ganttTable{nullptr};

    // Calendar
    QCalendarWidget* m_calendar{nullptr};
    QLabel* m_calendarDateLabel{nullptr};
    QListWidget* m_calendarTasksList{nullptr};
};

} // namespace ea::gui
