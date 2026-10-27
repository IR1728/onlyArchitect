#pragma once

#include <QWidget>
#include <QString>
#include <vector>

class QLabel;
class QPushButton;
class QGridLayout;

class WelcomeWidget : public QWidget {
    Q_OBJECT
public:
    explicit WelcomeWidget(QWidget* parent = nullptr);

    void refreshRecentProjects();

signals:
    void openProjectRequested(const QString& path);
    void newProjectRequested();
    void transferRequested();
    void openFileBrowseRequested();

private:
    void setupUi();
    QWidget* createProjectCard(const QString& title, const QString& subtitle, const QString& path, const QString& iconEmoji);
    QWidget* createActionCard(const QString& title, const QString& subtitle, const QString& iconEmoji, int actionType);

    QGridLayout* m_projectsGrid{nullptr};
};
