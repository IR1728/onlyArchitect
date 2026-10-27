#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QFormLayout>
#include "ea/model/Element.hpp"
#include "ea/model/Diagram.hpp"
#include "ea/repository/Repository.hpp"

class PropertiesWidget : public QWidget {
    Q_OBJECT
public:
    explicit PropertiesWidget(ea::Repository& repo, QWidget* parent = nullptr);

    void showElement(int elementId);
    void showDiagram(int diagramId);
    void clear();

private:
    void setupUi();

    ea::Repository& m_repo;
    QTabWidget* m_tabs{nullptr};

    // General tab widgets
    QLineEdit* m_idEdit{nullptr};
    QLineEdit* m_guidEdit{nullptr};
    QLineEdit* m_nameEdit{nullptr};
    QLineEdit* m_typeEdit{nullptr};
    QLineEdit* m_stereotypeEdit{nullptr};
    QLineEdit* m_statusEdit{nullptr};
    QLineEdit* m_authorEdit{nullptr};
    QLineEdit* m_versionEdit{nullptr};
    QTextEdit* m_notesEdit{nullptr};

    // Compartment tables
    QTableWidget* m_attributesTable{nullptr};
    QTableWidget* m_operationsTable{nullptr};
    QTableWidget* m_connectorsTable{nullptr};
};
