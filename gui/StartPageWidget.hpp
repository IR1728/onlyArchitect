#pragma once

#include <QWidget>
#include <QString>
#include <vector>

class QLabel;
class QPushButton;
class QVBoxLayout;

namespace ea::gui {

class StartPageWidget : public QWidget {
    Q_OBJECT
public:
    explicit StartPageWidget(QWidget* parent = nullptr);

    void refreshRecentProjects();

signals:
    void openProjectRequested(const QString& path);
    void openFileBrowseRequested();
    void newProjectRequested();
    void manageProjectsRequested();

private:
    void setupUi();
    QWidget* createActionLink(const QString& title, const QString& iconEmoji, const std::function<void()>& onClick);

    QVBoxLayout* m_recentsLayout{nullptr};
    QVBoxLayout* m_pinnedLayout{nullptr};
};

} // namespace ea::gui
