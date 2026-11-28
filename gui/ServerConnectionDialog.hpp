#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QString>

namespace ea::gui {

class ServerConnectionDialog : public QDialog {
    Q_OBJECT
public:
    enum class Action {
        None,
        ConnectClient,
        HostServer,
        Disconnect
    };

    explicit ServerConnectionDialog(const QString& currentFilePath,
                                    bool isHosting,
                                    bool isConnected,
                                    quint16 activePort,
                                    const QStringList& peers,
                                    QWidget* parent = nullptr);

    Action chosenAction() const { return m_action; }
    QString hostAddress() const { return m_hostEdit ? m_hostEdit->text().trimmed() : QString(); }
    quint16 port() const;
    QString username() const { return m_userEdit ? m_userEdit->text().trimmed() : QString(); }
    QString hostFilePath() const { return m_hostFileEdit ? m_hostFileEdit->text().trimmed() : QString(); }
    QString roomToken() const;

private:
    static QString detectedLocalIps();

    Action m_action{Action::None};
    QTabWidget* m_tabs{nullptr};
    QLineEdit* m_hostEdit{nullptr};
    QSpinBox* m_clientPortSpin{nullptr};
    QLineEdit* m_userEdit{nullptr};
    QLineEdit* m_clientTokenEdit{nullptr};

    QLineEdit* m_hostFileEdit{nullptr};
    QSpinBox* m_hostPortSpin{nullptr};
    QLineEdit* m_hostUserEdit{nullptr};
    QLineEdit* m_hostTokenEdit{nullptr};
};

} // namespace ea::gui
