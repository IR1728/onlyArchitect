#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QLabel>
#include "ea/repository/Repository.hpp"

class SqlConsoleWidget : public QWidget {
    Q_OBJECT
public:
    explicit SqlConsoleWidget(ea::Repository& repo, QWidget* parent = nullptr);

private slots:
    void onExecuteClicked();

private:
    ea::Repository& m_repo;
    QTextEdit* m_queryEdit{nullptr};
    QPushButton* m_execButton{nullptr};
    QTableWidget* m_resultsTable{nullptr};
    QLabel* m_statusLabel{nullptr};
};
