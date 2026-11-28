#include "CodeGeneratorWidget.hpp"
#include "DarkTheme.hpp"
#include "M3Icons.hpp"
#include "ea/export/ModelDocExporter.hpp"
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
    ea::gui::setThemedStyleSheet(title, "font-size: 15px; color: @primary;");
    headerLayout->addWidget(title);
    headerLayout->addStretch();

    m_copyBtn = new QPushButton("Copiar Código", this);
    ea::gui::M3Icons::decorate(m_copyBtn, "content_copy");
    m_saveCodeBtn = new QPushButton("Guardar Archivo...", this);
    ea::gui::M3Icons::decorate(m_saveCodeBtn, "save");
    m_exportDocBtn = new QPushButton("Exportar Docs (HTML)...", this);
    ea::gui::M3Icons::decorate(m_exportDocBtn, "article");
    m_exportMdBtn = new QPushButton("Exportar Modelo (MD)...", this);
    ea::gui::M3Icons::decorate(m_exportMdBtn, "description");
    headerLayout->addWidget(m_copyBtn);
    headerLayout->addWidget(m_saveCodeBtn);
    headerLayout->addWidget(m_exportDocBtn);
    headerLayout->addWidget(m_exportMdBtn);
    mainLayout->addLayout(headerLayout);

    connect(m_copyBtn, &QPushButton::clicked, this, &CodeGeneratorWidget::onCopyCodeClicked);
    connect(m_saveCodeBtn, &QPushButton::clicked, this, &CodeGeneratorWidget::onSaveCodeClicked);
    connect(m_exportDocBtn, &QPushButton::clicked, this, &CodeGeneratorWidget::onExportDocsClicked);
    connect(m_exportMdBtn, &QPushButton::clicked, this, &CodeGeneratorWidget::onExportMarkdownClicked);

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
        ea::gui::setThemedStyleSheet(ed,
            "QPlainTextEdit { background-color: @surfaceLowest; color: @primary; font-family: 'JetBrains Mono', 'Fira Code', 'Fira Mono', 'Noto Sans Mono', monospace; font-size: 12px; border: 1px solid @outlineVariant; border-radius: 4px; }"
        );
        return ed;
    };

    m_cppEditor = makeEditor(m_codeTabs);
    m_rustEditor = makeEditor(m_codeTabs);
    m_csharpEditor = makeEditor(m_codeTabs);
    m_pythonEditor = makeEditor(m_codeTabs);
    m_javaEditor = makeEditor(m_codeTabs);
    m_sqlEditor = makeEditor(m_codeTabs);

    m_codeTabs->addTab(m_cppEditor, "C++ (.hpp / .cpp)");
    m_codeTabs->addTab(m_rustEditor, "Rust (.rs)");
    m_codeTabs->addTab(m_csharpEditor, "C# (.cs)");
    m_codeTabs->addTab(m_pythonEditor, "Python (.py)");
    m_codeTabs->addTab(m_javaEditor, "Java (.java)");
    m_codeTabs->addTab(m_sqlEditor, "SQL DDL (.sql)");

    rightSplitter->addWidget(m_codeTabs);

    // Documentation View
    auto docWidget = new QWidget(rightSplitter);
    auto docLayout = new QVBoxLayout(docWidget);
    docLayout->setContentsMargins(0, 4, 0, 0);
    auto docHeader = new QLabel("<b>Documentación generada automáticamente:</b>", docWidget);
    ea::gui::setThemedStyleSheet(docHeader, "color: @onSurface; font-size: 12px;");
    docLayout->addWidget(docHeader);

    m_docBrowser = new QTextBrowser(docWidget);
    ea::gui::setThemedStyleSheet(m_docBrowser,
        "QTextBrowser { background-color: @surface; color: @onSurface; border: 1px solid @outlineVariant; border-radius: 4px; padding: 12px; font-family: 'Segoe UI', sans-serif; font-size: 12px; }"
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
        modelItem->setText(0, QString::fromStdString(model->GetName()));
        modelItem->setIcon(0, ea::gui::M3Icons::icon("folder"));
        modelItem->setText(1, "Model");

        // Recursively list packages and elements
        std::function<void(std::shared_ptr<ea::Package>, QTreeWidgetItem*)> addPkg;
        addPkg = [&](std::shared_ptr<ea::Package> pkg, QTreeWidgetItem* parentItem) {
            if (!pkg) return;

            for (size_t p = 0; p < pkg->GetPackages().size(); ++p) {
                auto sub = pkg->GetPackages().GetAt(static_cast<short>(p));
                if (!sub) continue;
                auto subItem = new QTreeWidgetItem(parentItem);
                subItem->setText(0, QString::fromStdString(sub->GetName()));
                subItem->setIcon(0, ea::gui::M3Icons::icon("inventory_2"));
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

    m_currentElementName = QString::fromStdString(elem->GetName()).replace(" ", "");
    if (m_currentElementName.isEmpty()) m_currentElementName = "UnnamedElement";

    m_cppEditor->setPlainText(generateCpp(elem));
    m_rustEditor->setPlainText(generateRust(elem));
    m_csharpEditor->setPlainText(generateCSharp(elem));
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
    code += QString("// Generated by onlyArchitect Studio: %1.hpp\n").arg(className);
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
    code += QString("# Generated by onlyArchitect Studio: %1.py\n").arg(className);
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

    code += QString("package com.onlyarchitect.model;\n\n");
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
    html += ea::gui::themed("<h2 style='color: @primary; margin-bottom: 4px;'>%1 <span style='font-size: 13px; color: @onSurfaceMuted;'>[%2]</span></h2>")
                .arg(QString::fromStdString(elem->GetName()), QString::fromStdString(ea::to_string(elem->GetType())));

    if (!elem->GetStereotype().empty()) {
        html += ea::gui::themed("<p style='color: @warning; margin-top: 0;'><b>Estereotipo:</b> «%1»</p>")
                    .arg(QString::fromStdString(elem->GetStereotype()));
    }

    if (!elem->GetNotes().empty()) {
        html += ea::gui::themed("<h3 style='color: @onSurface; margin-bottom: 4px;'>Descripción / Notas</h3>");
        html += ea::gui::themed("<div style='background-color: @surfaceLow; padding: 10px; border-left: 3px solid @primaryStrong; border-radius: 4px;'>%1</div>")
                    .arg(QString::fromStdString(elem->GetNotes()));
    }

    if (elem->GetAttributes().size() > 0) {
        html += ea::gui::themed("<h3 style='color: @onSurface; margin-top: 14px; margin-bottom: 4px;'>Atributos</h3>");
        html += ea::gui::themed("<table border='1' cellspacing='0' cellpadding='6' style='width: 100%; border-color: @outlineVariant; border-collapse: collapse;'>");
        html += ea::gui::themed("<tr style='background-color: @surfaceLow; color: @onSurfaceMuted;'><th>Visibilidad</th><th>Nombre</th><th>Tipo</th><th>Notas</th></tr>");
        for (size_t i = 0; i < elem->GetAttributes().size(); ++i) {
            auto a = elem->GetAttributes().GetAt(static_cast<short>(i));
            if (!a) continue;
            QString vis = "+";
            if (a->GetVisibility() == ea::Scope::Private) vis = "-";
            else if (a->GetVisibility() == ea::Scope::Protected) vis = "#";
            html += ea::gui::themed("<tr><td align='center'>%1</td><td><b>%2</b></td><td style='color: @primary;'>%3</td><td>%4</td></tr>")
                        .arg(vis, QString::fromStdString(a->GetName()), QString::fromStdString(a->GetType()), QString::fromStdString(a->GetNotes()));
        }
        html += "</table>";
    }

    if (elem->GetMethods().size() > 0) {
        html += ea::gui::themed("<h3 style='color: @onSurface; margin-top: 14px; margin-bottom: 4px;'>Operaciones</h3>");
        html += ea::gui::themed("<table border='1' cellspacing='0' cellpadding='6' style='width: 100%; border-color: @outlineVariant; border-collapse: collapse;'>");
        html += ea::gui::themed("<tr style='background-color: @surfaceLow; color: @onSurfaceMuted;'><th>Visibilidad</th><th>Firma</th><th>Tipo Retorno</th></tr>");
        for (size_t i = 0; i < elem->GetMethods().size(); ++i) {
            auto m = elem->GetMethods().GetAt(static_cast<short>(i));
            if (!m) continue;
            QString vis = "+";
            if (m->GetVisibility() == ea::Scope::Private) vis = "-";
            else if (m->GetVisibility() == ea::Scope::Protected) vis = "#";
            html += ea::gui::themed("<tr><td align='center'>%1</td><td><b>%2()</b></td><td style='color: @primary;'>%3</td></tr>")
                        .arg(vis, QString::fromStdString(m->GetName()), QString::fromStdString(m->GetReturnType()));
        }
        html += "</table>";
    }

    html += "</div>";
    return html;
}

QString CodeGeneratorWidget::generateRust(std::shared_ptr<ea::Element> elem) {
    if (!elem) return "";
    QString typeName = QString::fromStdString(elem->GetName()).replace(" ", "");
    if (typeName.isEmpty()) typeName = "UnnamedType";

    auto mapRustType = [](const std::string& raw) -> QString {
        QString t = QString::fromStdString(raw).trimmed().toLower();
        if (t.isEmpty() || t == "void") return "()";
        if (t == "int" || t == "integer" || t == "int32") return "i32";
        if (t == "long" || t == "int64") return "i64";
        if (t == "float") return "f32";
        if (t == "double" || t == "real") return "f64";
        if (t == "bool" || t == "boolean") return "bool";
        if (t == "string" || t == "std::string") return "String";
        return QString::fromStdString(raw);
    };

    QString code;
    code += QString("// Generated by onlyArchitect Studio: %1.rs\n\n").arg(typeName.toLower());
    if (!elem->GetNotes().empty()) {
        code += QString("/// %1\n").arg(QString::fromStdString(elem->GetNotes()).replace("\n", "\n/// "));
    }

    if (elem->GetType() == ea::ObjectType::Interface) {
        code += QString("pub trait %1 {\n").arg(typeName);
        for (size_t i = 0; i < elem->GetMethods().size(); ++i) {
            auto m = elem->GetMethods().GetAt(static_cast<short>(i));
            if (!m) continue;
            QString ret = mapRustType(m->GetReturnType());
            code += QString("    fn %1(&self)%2;\n")
                        .arg(QString::fromStdString(m->GetName()), ret == "()" ? "" : " -> " + ret);
        }
        code += "}\n";
        return code;
    }

    code += "#[derive(Debug, Clone)]\n";
    code += QString("pub struct %1 {\n").arg(typeName);
    for (size_t i = 0; i < elem->GetAttributes().size(); ++i) {
        auto a = elem->GetAttributes().GetAt(static_cast<short>(i));
        if (!a) continue;
        QString vis = (a->GetVisibility() == ea::Scope::Public) ? "pub " : "";
        code += QString("    %1%2: %3,\n")
                    .arg(vis, QString::fromStdString(a->GetName()), mapRustType(a->GetType()));
    }
    code += "}\n\n";

    code += QString("impl %1 {\n").arg(typeName);
    for (size_t i = 0; i < elem->GetMethods().size(); ++i) {
        auto m = elem->GetMethods().GetAt(static_cast<short>(i));
        if (!m) continue;
        QString vis = (m->GetVisibility() == ea::Scope::Public) ? "pub " : "";
        QString ret = mapRustType(m->GetReturnType());
        code += QString("    %1fn %2(&self)%3 {\n        todo!()\n    }\n\n")
                    .arg(vis, QString::fromStdString(m->GetName()), ret == "()" ? "" : " -> " + ret);
    }
    code += "}\n";
    return code;
}

QString CodeGeneratorWidget::generateCSharp(std::shared_ptr<ea::Element> elem) {
    if (!elem) return "";
    QString className = QString::fromStdString(elem->GetName()).replace(" ", "");
    if (className.isEmpty()) className = "UnnamedClass";

    auto mapCsType = [](const std::string& raw) -> QString {
        QString t = QString::fromStdString(raw).trimmed().toLower();
        if (t.isEmpty() || t == "void") return "void";
        if (t == "int" || t == "integer") return "int";
        if (t == "long") return "long";
        if (t == "float") return "float";
        if (t == "double" || t == "real") return "double";
        if (t == "bool" || t == "boolean") return "bool";
        if (t == "string" || t == "std::string") return "string";
        return QString::fromStdString(raw);
    };

    auto mapCsVis = [](ea::Scope s) -> QString {
        switch (s) {
            case ea::Scope::Private: return "private";
            case ea::Scope::Protected: return "protected";
            case ea::Scope::Package: return "internal";
            default: return "public";
        }
    };

    QString code;
    code += QString("// Generated by onlyArchitect Studio: %1.cs\n\n").arg(className);
    code += "using System;\nusing System.Collections.Generic;\n\n";
    code += "namespace OnlyArchitect.Model\n{\n";
    if (!elem->GetNotes().empty()) {
        code += "    /// <summary>\n";
        code += QString("    /// %1\n").arg(QString::fromStdString(elem->GetNotes()).replace("\n", "\n    /// "));
        code += "    /// </summary>\n";
    }

    const bool isInterface = (elem->GetType() == ea::ObjectType::Interface);
    code += QString("    public %1 %2\n    {\n").arg(isInterface ? "interface" : "class", className);

    if (!isInterface) {
        for (size_t i = 0; i < elem->GetAttributes().size(); ++i) {
            auto a = elem->GetAttributes().GetAt(static_cast<short>(i));
            if (!a) continue;
            code += QString("        %1 %2 %3 { get; set; }\n")
                        .arg(mapCsVis(a->GetVisibility()), mapCsType(a->GetType()), QString::fromStdString(a->GetName()));
        }
        if (elem->GetAttributes().size() > 0) code += "\n";
    }

    for (size_t i = 0; i < elem->GetMethods().size(); ++i) {
        auto m = elem->GetMethods().GetAt(static_cast<short>(i));
        if (!m) continue;
        QString ret = mapCsType(m->GetReturnType());
        if (isInterface) {
            code += QString("        %1 %2();\n").arg(ret, QString::fromStdString(m->GetName()));
        } else {
            code += QString("        %1 %2 %3()\n        {\n            throw new NotImplementedException();\n        }\n\n")
                        .arg(mapCsVis(m->GetVisibility()), ret, QString::fromStdString(m->GetName()));
        }
    }

    code += "    }\n}\n";
    return code;
}

void CodeGeneratorWidget::onCopyCodeClicked() {
    auto currentEd = qobject_cast<QPlainTextEdit*>(m_codeTabs->currentWidget());
    if (currentEd) {
        QApplication::clipboard()->setText(currentEd->toPlainText());
        QMessageBox::information(this, "Copiado", "Código copiado al portapapeles con éxito.");
    }
}

void CodeGeneratorWidget::onSaveCodeClicked() {
    auto currentEd = qobject_cast<QPlainTextEdit*>(m_codeTabs->currentWidget());
    if (!currentEd) return;

    static const QStringList exts = {".hpp", ".rs", ".cs", ".py", ".java", ".sql"};
    const int idx = m_codeTabs->currentIndex();
    const QString ext = (idx >= 0 && idx < exts.size()) ? exts[idx] : ".txt";
    const QString base = m_currentElementName.isEmpty() ? "GeneratedCode" : m_currentElementName;
    const QString path = QFileDialog::getSaveFileName(this, "Guardar Código Fuente", base + ext,
                                                      QString("Archivo (*%1);;Todos (*)").arg(ext));
    if (path.isEmpty()) return;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << currentEd->toPlainText();
        QMessageBox::information(this, "Guardado", QString("Código guardado en:\n%1").arg(path));
    }
}

void CodeGeneratorWidget::onExportMarkdownClicked() {
    if (!m_repo.IsOpen()) {
        QMessageBox::warning(this, "Exportar Markdown", "Abre o crea un repositorio antes de exportar la documentación.");
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, "Exportar Documentación del Modelo (Markdown)",
                                                      "arquitectura_modelo.md", "Markdown (*.md)");
    if (path.isEmpty()) return;
    if (ea::ModelDocExporter::exportMarkdownToFile(m_repo, path.toStdString())) {
        QMessageBox::information(this, "Exportado", QString("Documentación Markdown guardada en:\n%1").arg(path));
    } else {
        QMessageBox::critical(this, "Error", QString("No se pudo escribir:\n%1").arg(path));
    }
}

void CodeGeneratorWidget::onExportDocsClicked() {
    QString path = QFileDialog::getSaveFileName(this, "Exportar Documentación HTML", "model_documentation.html", "HTML Files (*.html)");
    if (!path.isEmpty()) {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "<!DOCTYPE html><html><head><meta charset='utf-8'><title>onlyArchitect Documentation</title>";
            out << ea::gui::themed("<style>body { background-color: @surfaceLowest; color: @onSurface; font-family: sans-serif; padding: 24px; }</style></head><body>");
            out << m_docBrowser->toHtml();
            out << "</body></html>";
            QMessageBox::information(this, "Exportado", QString("Documentación guardada en:\n%1").arg(path));
        }
    }
}
