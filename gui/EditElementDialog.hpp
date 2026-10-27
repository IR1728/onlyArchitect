#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include "ea/model/Element.hpp"
#include "ea/repository/Repository.hpp"

class EditElementDialog : public QDialog {
    Q_OBJECT
public:
    EditElementDialog(std::shared_ptr<ea::Element> elem, ea::Repository& repo, QWidget* parent = nullptr);

    bool isModified() const { return m_modified; }

private slots:
    void onAddAttribute();
    void onRemoveAttribute();
    void onAddMethod();
    void onRemoveMethod();
    void onSave();

private:
    std::shared_ptr<ea::Element> m_elem;
    ea::Repository& m_repo;
    bool m_modified{false};

    QLineEdit* m_nameEdit{nullptr};
    QLineEdit* m_stereotypeEdit{nullptr};
    QComboBox* m_typeCombo{nullptr};
    QLineEdit* m_statusEdit{nullptr};
    QTextEdit* m_notesEdit{nullptr};

    QTableWidget* m_attrTable{nullptr};
    QTableWidget* m_methTable{nullptr};
};
