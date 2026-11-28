#pragma once

#include <QDialog>
#include <QSettings>
#include <QStringList>
#include <vector>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QListWidgetItem;

namespace ea::gui {

// Predefined workspace layouts (Alt+7)
struct SystemWorkspaceLayout {
    QString name;
    QString set;          // "Design and Construct" | "Software & Simulation"
    bool implemented;     // false: needs windows onlyArchitect does not have yet
};

const std::vector<SystemWorkspaceLayout>& systemWorkspaceLayouts();
bool isImplementedSystemLayout(const QString& name);

// Per-user persistence of custom layouts and Workspace Layout dialog options
class WorkspaceLayoutStore {
public:
    QStringList userLayoutNames() const;
    bool hasUserLayout(const QString& name) const;
    void saveUserLayout(const QString& name, const QByteArray& dockState, int centralWorkspace);
    bool loadUserLayout(const QString& name, QByteArray& dockState, int& centralWorkspace) const;
    void removeUserLayout(const QString& name);

    QString lastApplied() const;
    void setLastApplied(const QString& name);
    bool loadOnStartup() const;
    void setLoadOnStartup(bool enabled);
    bool closeOnApply() const;
    void setCloseOnApply(bool enabled);

private:
    QVariantMap userLayouts() const;

    mutable QSettings m_settings{"onlyArchitect", "onlyArchitect"};
};

// 'Save Custom Workspace Layout' dialog
class SaveWorkspaceLayoutDialog : public QDialog {
    Q_OBJECT
public:
    SaveWorkspaceLayoutDialog(const QStringList& existingUserLayouts, QWidget* parent = nullptr);
    QString layoutName() const;

private:
    QComboBox* m_nameCombo{nullptr};
};

// 'Workspace Layout' dialog, 'Workspaces' page
class WorkspaceLayoutDialog : public QDialog {
    Q_OBJECT
public:
    explicit WorkspaceLayoutDialog(WorkspaceLayoutStore& store, QWidget* parent = nullptr);
    void refresh();

signals:
    void applyRequested(const QString& name);
    void saveCurrentLayoutRequested(const QString& name);

private:
    QString selectedLayout() const;
    void applySelected();
    void copySelectedAsNew();
    void deleteSelected();
    void showContextMenu(const QPoint& pos);

    WorkspaceLayoutStore& m_store;
    QListWidget* m_list{nullptr};
    QLineEdit* m_lastAppliedEdit{nullptr};
    QCheckBox* m_closeOnApply{nullptr};
    QCheckBox* m_loadOnStartup{nullptr};
};

} // namespace ea::gui
