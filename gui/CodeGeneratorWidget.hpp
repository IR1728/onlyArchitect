#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QPushButton>
#include "ea/repository/Repository.hpp"

class CodeGeneratorWidget : public QWidget {
    Q_OBJECT
public:
    explicit CodeGeneratorWidget(ea::Repository& repo, QWidget* parent = nullptr);

    void refreshFromRepository();
    void generateForElement(int elementId);

private slots:
    void onTreeItemSelectionChanged();
    void onCopyCodeClicked();
    void onExportDocsClicked();

private:
    void setupUi();
    QString generateCpp(std::shared_ptr<ea::Element> elem);
    QString generatePython(std::shared_ptr<ea::Element> elem);
    QString generateJava(std::shared_ptr<ea::Element> elem);
    QString generateSql(std::shared_ptr<ea::Element> elem);
    QString generateDocumentation(std::shared_ptr<ea::Element> elem);

    ea::Repository& m_repo;

    QTreeWidget* m_elementTree{nullptr};
    QTabWidget* m_codeTabs{nullptr};
    QPlainTextEdit* m_cppEditor{nullptr};
    QPlainTextEdit* m_pythonEditor{nullptr};
    QPlainTextEdit* m_javaEditor{nullptr};
    QPlainTextEdit* m_sqlEditor{nullptr};
    QTextBrowser* m_docBrowser{nullptr};
    QPushButton* m_copyBtn{nullptr};
    QPushButton* m_exportDocBtn{nullptr};
};
