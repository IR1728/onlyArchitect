#pragma once

#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <memory>
#include "ea/model/Connector.hpp"
#include "ea/repository/Repository.hpp"

class EditConnectorDialog : public QDialog {
    Q_OBJECT
public:
    EditConnectorDialog(std::shared_ptr<ea::Connector> conn, ea::Repository& repo, QWidget* parent = nullptr);

    bool isModified() const { return m_modified; }

private slots:
    void onSave();

private:
    std::shared_ptr<ea::Connector> m_conn;
    ea::Repository& m_repo;
    bool m_isMessage{false};
    bool m_modified{false};

    // Shared / Message fields
    QComboBox* m_nameCombo{nullptr};      // Editable combo: offers target class operations for Sequence Messages
    QComboBox* m_messageSortCombo{nullptr};
    QComboBox* m_typeCombo{nullptr};
    QComboBox* m_stereotypeCombo{nullptr};
    QComboBox* m_directionCombo{nullptr};

    // Domain / Relationship end fields (Roles & Multiplicities)
    QLineEdit* m_sourceRoleEdit{nullptr};
    QComboBox* m_sourceMultCombo{nullptr};
    QLineEdit* m_targetRoleEdit{nullptr};
    QComboBox* m_targetMultCombo{nullptr};

    QTextEdit* m_notesEdit{nullptr};
};
