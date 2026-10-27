#include "CodeGeneratorWidget.hpp"
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QClipboard>
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>

CodeGeneratorWidget::CodeGeneratorWidget(ea::Repository& repo, QWidget* parent)
    : QWidget(parent), m_repo(repo) {
    setupUi();
    refreshFromRepository();
}

void CodeGeneratorWidget::setupUi() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    auto headerLayout = new QHBoxLayout();
    auto title = new QLabel("<b>Generador de Código y Documentación</b>", this);
    title->setStyleSheet("font-size: 15px; color: #38bdf8;");
    headerLayout->addWidget(title);
    headerLayout->addStretch();

    m_copyBtn = new QPushButton("📋 Copiar Código", this);
    m_exportDocBtn = new QPushButton("📑 Exportar Docs (HTML)...", this);
    headerLayout->addWidget(m_copyBtn);
    headerLayout->addWidget(m_exportDocBtn);
    mainLayout->addLayout(headerLayout);

    connect(m_copyBtn, &QPushButton::clicked, this, &CodeGeneratorWidget::onCopyCodeClicked);
    connect(m_exportDocBtn, &QPushButton::clicked, this, &CodeGeneratorWidget::onExportDocsClicked);

    // Main Splitter (Left: Elements Tree, Right: Code & Docs)
    auto splitter = new QSplitter(Qt::Horizontal, this);

    // Left Tree
    auto leftWidget = new QWidget(splitter);
    auto leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(new QLabel("Selecciona un elemento UML:", leftWidget));

    m_elementTree = new QTreeWidget(leftWidget);
    m_elementTree->setHeaderLabels({"Elemento", "Tipo"});
    m_elementTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_elementTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    leftLayout->addWidget(m_elementTree);
    splitter->addWidget(leftWidget);

    connect(m_elementTree, &QTreeWidget::itemSelectionChanged, this, &CodeGeneratorWidget::onTreeItemSelectionChanged);

    // Right Splitter (Top: Code Tabs, Bottom: Documentation Preview)
    auto rightSplitter = new QSplitter(Qt::Vertical, splitter);

    // Code Tabs
    m_codeTabs = new QTabWidget(rightSplitter);

    auto makeEditor = [](QWidget* parent) {
        auto ed = new QPlainTextEdit(parent);
        ed->setReadOnly(true);
        ed->setStyleSheet(
            "QPlainTextEdit { background-color: #11141c; color: #a5f3fc; font-family: 'Consolas', 'Fira Code', monospace; font-size: 12px; border: 1px solid #282e3d; border-radius: 4px; }"
        );
        return ed;
    };

    m_cppEditor = makeEditor(m_codeTabs);
    m_pythonEditor = makeEditor(m_codeTabs);
    m_javaEditor = makeEditor(m_codeTabs);
    m_sqlEditor = makeEditor(m_codeTabs);

    m_codeTabs->addTab(m_cppEditor, "C++ (.hpp / .cpp)");
    m_codeTabs->addTab(m_pythonEditor, "Python (.py)");
    m_codeTabs->addTab(m_javaEditor, "Java (.java)");
    m_codeTabs->addTab(m_sqlEditor, "SQL DDL (.sql)");

    rightSplitter->addWidget(m_codeTabs);

    // Documentation View
    auto docWidget = new QWidget(rightSplitter);
    auto docLayout = new QVBoxLayout(docWidget);
    docLayout->setContentsMargins(0, 4, 0, 0);
    auto docHeader = new QLabel("<b>Documentación generada automáticamente:</b>", docWidget);
    docHeader->setStyleSheet("color: #e2e8f0; font-size: 12px;");
    docLayout->addWidget(docHeader);

    m_docBrowser = new QTextBrowser(docWidget);
    m_docBrowser->setStyleSheet(
        "QTextBrowser { background-color: #171b24; color: #e2e8f0; border: 1px solid #282e3d; border-radius: 4px; padding: 12px; font-family: 'Segoe UI', sans-serif; font-size: 12px; }"
    );
    docLayout->addWidget(m_docBrowser);

    rightSplitter->addWidget(docWidget);
    rightSplitter->setStretchFactor(0, 3);
    rightSplitter->setStretchFactor(1, 2);

    splitter->addWidget(rightSplitter);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);

    mainLayout->addWidget(splitter, 1);
}

void CodeGeneratorWidget::refreshFromRepository() {
    m_elementTree->clear();
    if (!m_repo.IsOpen()) return;

    for (size_t i = 0; i < m_repo.GetModels().size(); ++i) {
        auto model = m_repo.GetModels().GetAt(static_cast<short>(i));
        if (!model) continue;

        auto modelItem = new QTreeWidgetItem(m_elementTree);
        modelItem->setText(0, QString("📁 %1").arg(QString::fromStdString(model->GetName())));
        modelItem->setText(1, "Model");

        // Recursively list packages and elements
        std::function<void(std::shared_ptr<ea::Package>, QTreeWidgetItem*)> addPkg;
        addPkg = [&](std::shared_ptr<ea::Package> pkg, QTreeWidgetItem* parentItem) {
            if (!pkg) return;

            for (size_t p = 0; p < pkg->GetPackages().size(); ++p) {
                auto sub = pkg->GetPackages().GetAt(static_cast<short>(p));
                if (!sub) continue;
                auto subItem = new QTreeWidgetItem(parentItem);
                subItem->setText(0, QString("📦 %1").arg(QString::fromStdString(sub->GetName())));
                subItem->setText(1, "Package");
                addPkg(sub, subItem);
            }

            for (size_t e = 0; e < pkg->GetElements().size(); ++e) {
                auto elem = pkg->GetElements().GetAt(static_cast<short>(e));
                if (!elem) continue;
                auto elemItem = new QTreeWidgetItem(parentItem);
                elemItem->setText(0, QString::fromStdString(elem->GetName()));
                elemItem->setText(1, QString::fromStdString(ea::to_string(elem->GetType())));
                elemItem->setData(0, Qt::UserRole, elem->GetElementID());
            }
        };

        addPkg(model, modelItem);
        modelItem->setExpanded(true);
    }

    // Select first element if available
    if (m_elementTree->topLevelItemCount() > 0) {
        auto first = m_elementTree->topLevelItem(0);
        if (first->childCount() > 0) {
            m_elementTree->setCurrentItem(first->child(0));
        }
    }
}

void CodeGeneratorWidget::onTreeItemSelectionChanged() {
    auto current = m_elementTree->currentItem();
    if (!current) return;

    int elementId = current->data(0, Qt::UserRole).toInt();
    if (elementId > 0) {
        generateForElement(elementId);
    }
}

void CodeGeneratorWidget::generateForElement(int elementId) {
    auto elem = m_repo.GetElementByID(elementId);
    if (!elem) return;

    m_cppEditor->setPlainText(generateCpp(elem));
    m_pythonEditor->setPlainText(generatePython(elem));
    m_javaEditor->setPlainText(generateJava(elem));
    m_sqlEditor->setPlainText(generateSql(elem));
    m_docBrowser->setHtml(generateDocumentation(elem));
}

QString CodeGeneratorWidget::generateCpp(std::shared_ptr<ea::Element> elem) {
    if (!elem) return "";
    QString code;
    QString className = QString::fromStdString(elem->GetName()).replace(" ", "");
    if (className.isEmpty()) className = "UnnamedClass";

    code += QString("// ================================================================\n");
    code += QString("// Generated by OpenEA Studio: %1.hpp\n").arg(className);
    code += QString("// ================================================================\n\n");
    code += "#pragma once\n\n";
    code += "#include <string>\n";
    code += "#include <vector>\n";
    code += "#include <memory>\n\n";

    if (!elem->GetNotes().empty()) {
        code += "/**\n";
        code += QString(" * %1\n").arg(QString::fromStdString(elem->GetNotes()));
        code += " */\n";
    }

    if (!elem->GetStereotype().empty()) {
        code += QString("// <<%1>>\n").arg(QString::fromStdString(elem->GetStereotype()));
    }

    code += QString("class %1 {\n").arg(className);
    code += "public:\n";
    code += QString("    %1() = default;\n").arg(className);
    code += QString("    virtual ~%1() = default;\n\n").arg(className);

    // Methods
    if (elem->GetMethods().size() > 0) {
        code += "    // Operations\n";
        for (size_t i = 0; i < elem->GetMethods().size(); ++i) {
            auto m = elem->GetMethods().GetAt(static_cast<short>(i));
            if (!m) continue;
            QString ret = QString::fromStdString(m->GetReturnType());
            if (ret.isEmpty()) ret = "void";
            QString name = QString::fromStdString(m->GetName()).replace(" ", "");
            code += QString("    %1 %2();\n").arg(ret, name);
        }
        code += "\n";
    }

    // Attributes
    if (elem->GetAttributes().size() > 0) {
        code += "private:\n";
        for (size_t i = 0; i < elem->GetAttributes().size(); ++i) {
            auto a = elem->GetAttributes().GetAt(static_cast<short>(i));
            if (!a) continue;
            QString type = QString::fromStdString(a->GetType());
            if (type.isEmpty()) type = "std::string";
            if (type == "int") type = "int";
            else if (type == "boolean" || type == "bool") type = "bool";
            else if (type == "float" || type == "double") type = "double";
            else type = "std::string";

            QString name = QString::fromStdString(a->GetName()).replace(" ", "");
            code += QString("    %1 m_%2;\n").arg(type, name);
        }
    }

    code += "};\n";
    return code;
}

QString CodeGeneratorWidget::generatePython(std::shared_ptr<ea::Element> elem) {
    if (!elem) return "";
    QString code;
    QString className = QString::fromStdString(elem->GetName()).replace(" ", "");
    if (className.isEmpty()) className = "UnnamedClass";

    code += "# ================================================================\n";
    code += QString("# Generated by OpenEA Studio: %1.py\n").arg(className);
    code += "# ================================================================\n\n";
    code += "from typing import List, Optional, Any\n\n\n";
    code += QString("class %1:\n").arg(className);

    if (!elem->GetNotes().empty()) {
        code += QString("    \"\"\"%1\"\"\"\n\n").arg(QString::fromStdString(elem->GetNotes()));
    }

    // Constructor
    code += "    def __init__(self):\n";
    if (elem->GetAttributes().size() == 0) {
        code += "        pass\n";
    } else {
        for (size_t i = 0; i < elem->GetAttributes().size(); ++i) {
            auto a = elem->GetAttributes().GetAt(static_cast<short>(i));
            if (!a) continue;
            QString name = QString::fromStdString(a->GetName()).replace(" ", "");
            QString type = QString::fromStdString(a->GetType());
            code += QString("        self.%1: Optional[%2] = None\n").arg(name, type.isEmpty() ? "Any" : type);
        }
    }
    code += "\n";

    // Methods
    for (size_t i = 0; i < elem->GetMethods().size(); ++i) {
        auto m = elem->GetMethods().GetAt(static_cast<short>(i));
        if (!m) continue;
        QString name = QString::fromStdString(m->GetName()).replace(" ", "");
        QString ret = QString::fromStdString(m->GetReturnType());
        code += QString("    def %1(self) -> %2:\n").arg(name, ret.isEmpty() ? "None" : ret);
        code += "        # TODO: Implement method logic\n";
        code += "        pass\n\n";
    }

    return code;
}

QString CodeGeneratorWidget::generateJava(std::shared_ptr<ea::Element> elem) {
    if (!elem) return "";
    QString code;
    QString className = QString::fromStdString(elem->GetName()).replace(" ", "");
    if (className.isEmpty()) className = "UnnamedClass";

    code += QString("package com.openea.model;\n\n");
    code += "import java.util.*;\n\n";

    if (!elem->GetNotes().empty()) {
        code += "/**\n";
        code += QString(" * %1\n").arg(QString::fromStdString(elem->GetNotes()));
        code += " */\n";
    }

    code += QString("public class %1 {\n\n").arg(className);

    // Fields
    for (size_t i = 0; i < elem->GetAttributes().size(); ++i) {
        auto a = elem->GetAttributes().GetAt(static_cast<short>(i));
        if (!a) continue;
        QString name = QString::fromStdString(a->GetName()).replace(" ", "");
        QString type = QString::fromStdString(a->GetType());
        if (type.isEmpty()) type = "String";
        code += QString("    private %1 %2;\n").arg(type, name);
    }
    code += "\n";

    // Constructor
    code += QString("    public %1() {\n    }\n\n").arg(className);

    // Methods
    for (size_t i = 0; i < elem->GetMethods().size(); ++i) {
        auto m = elem->GetMethods().GetAt(static_cast<short>(i));
        if (!m) continue;
        QString name = QString::fromStdString(m->GetName()).replace(" ", "");
        QString ret = QString::fromStdString(m->GetReturnType());
        if (ret.isEmpty()) ret = "void";
        code += QString("    public %1 %2() {\n        // TODO\n    }\n\n").arg(ret, name);
    }

    code += "}\n";
    return code;
}

QString CodeGeneratorWidget::generateSql(std::shared_ptr<ea::Element> elem) {
    if (!elem) return "";
    QString code;
    QString tableName = QString("t_%1").arg(QString::fromStdString(elem->GetName()).toLower().replace(" ", "_"));

    code += QString("CREATE TABLE IF NOT EXISTS %1 (\n").arg(tableName);
    code += "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n";
    code += "    ea_guid VARCHAR(40) UNIQUE,\n";

    for (size_t i = 0; i < elem->GetAttributes().size(); ++i) {
        auto a = elem->GetAttributes().GetAt(static_cast<short>(i));
        if (!a) continue;
        QString name = QString::fromStdString(a->GetName()).toLower().replace(" ", "_");
        QString type = QString::fromStdString(a->GetType()).toLower();
        QString sqlType = "TEXT";
        if (type == "int" || type == "integer") sqlType = "INTEGER";
        else if (type == "float" || type == "double") sqlType = "REAL";
        else if (type == "bool" || type == "boolean") sqlType = "INTEGER DEFAULT 0";

        code += QString("    %1 %2").arg(name, sqlType);
        if (i + 1 < elem->GetAttributes().size()) code += ",";
        code += "\n";
    }

    code += ");\n";
    return code;
}

QString CodeGeneratorWidget::generateDocumentation(std::shared_ptr<ea::Element> elem) {
    if (!elem) return "";
    QString html;
    html += "<div style='font-family: sans-serif; line-height: 1.5;'>";
    html += QString("<h2 style='color: #38bdf8; margin-bottom: 4px;'>%1 <span style='font-size: 13px; color: #9ca3af;'>[%2]</span></h2>")
                .arg(QString::fromStdString(elem->GetName()), QString::fromStdString(ea::to_string(elem->GetType())));

    if (!elem->GetStereotype().empty()) {
        html += QString("<p style='color: #f59e0b; margin-top: 0;'><b>Estereotipo:</b> «%1»</p>")
                    .arg(QString::fromStdString(elem->GetStereotype()));
    }

    if (!elem->GetNotes().empty()) {
        html += "<h3 style='color: #e2e8f0; margin-bottom: 4px;'>Descripción / Notas</h3>";
        html += QString("<div style='background-color: #1e222d; padding: 10px; border-left: 3px solid #007acc; border-radius: 4px;'>%1</div>")
                    .arg(QString::fromStdString(elem->GetNotes()));
    }

    if (elem->GetAttributes().size() > 0) {
        html += "<h3 style='color: #e2e8f0; margin-top: 14px; margin-bottom: 4px;'>Atributos</h3>";
        html += "<table border='1' cellspacing='0' cellpadding='6' style='width: 100%; border-color: #2b3140; border-collapse: collapse;'>";
        html += "<tr style='background-color: #1e222d; color: #9ca3af;'><th>Visibilidad</th><th>Nombre</th><th>Tipo</th><th>Notas</th></tr>";
        for (size_t i = 0; i < elem->GetAttributes().size(); ++i) {
            auto a = elem->GetAttributes().GetAt(static_cast<short>(i));
            if (!a) continue;
            QString vis = "+";
            if (a->GetVisibility() == ea::Scope::Private) vis = "-";
            else if (a->GetVisibility() == ea::Scope::Protected) vis = "#";
            html += QString("<tr><td align='center'>%1</td><td><b>%2</b></td><td style='color: #38bdf8;'>%3</td><td>%4</td></tr>")
                        .arg(vis, QString::fromStdString(a->GetName()), QString::fromStdString(a->GetType()), QString::fromStdString(a->GetNotes()));
        }
        html += "</table>";
    }

    if (elem->GetMethods().size() > 0) {
        html += "<h3 style='color: #e2e8f0; margin-top: 14px; margin-bottom: 4px;'>Operaciones</h3>";
        html += "<table border='1' cellspacing='0' cellpadding='6' style='width: 100%; border-color: #2b3140; border-collapse: collapse;'>";
        html += "<tr style='background-color: #1e222d; color: #9ca3af;'><th>Visibilidad</th><th>Firma</th><th>Tipo Retorno</th></tr>";
        for (size_t i = 0; i < elem->GetMethods().size(); ++i) {
            auto m = elem->GetMethods().GetAt(static_cast<short>(i));
            if (!m) continue;
            QString vis = "+";
            if (m->GetVisibility() == ea::Scope::Private) vis = "-";
            else if (m->GetVisibility() == ea::Scope::Protected) vis = "#";
            html += QString("<tr><td align='center'>%1</td><td><b>%2()</b></td><td style='color: #38bdf8;'>%3</td></tr>")
                        .arg(vis, QString::fromStdString(m->GetName()), QString::fromStdString(m->GetReturnType()));
        }
        html += "</table>";
    }

    html += "</div>";
    return html;
}

void CodeGeneratorWidget::onCopyCodeClicked() {
    auto currentEd = qobject_cast<QPlainTextEdit*>(m_codeTabs->currentWidget());
    if (currentEd) {
        QApplication::clipboard()->setText(currentEd->toPlainText());
        QMessageBox::information(this, "Copiado", "Código copiado al portapapeles con éxito.");
    }
}

void CodeGeneratorWidget::onExportDocsClicked() {
    QString path = QFileDialog::getSaveFileName(this, "Exportar Documentación HTML", "model_documentation.html", "HTML Files (*.html)");
    if (!path.isEmpty()) {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "<!DOCTYPE html><html><head><meta charset='utf-8'><title>OpenEA Documentation</title>";
            out << "<style>body { background-color: #0f172a; color: #f8fafc; font-family: sans-serif; padding: 24px; }</style></head><body>";
            out << m_docBrowser->toHtml();
            out << "</body></html>";
            QMessageBox::information(this, "Exportado", QString("Documentación guardada en:\n%1").arg(path));
        }
    }
}
