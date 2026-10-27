#include "PropertiesWidget.hpp"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QLabel>

PropertiesWidget::PropertiesWidget(ea::Repository& repo, QWidget* parent)
    : QWidget(parent), m_repo(repo) {
    setupUi();
}

void PropertiesWidget::setupUi() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    m_tabs = new QTabWidget(this);
    mainLayout->addWidget(m_tabs);

    // 1. Element Tab (proto3.jpeg)
    auto genWidget = new QWidget(this);
    auto formLayout = new QFormLayout(genWidget);
    formLayout->setLabelAlignment(Qt::AlignRight);
    formLayout->setContentsMargins(6, 6, 6, 6);
    formLayout->setSpacing(4);

    auto lblGeneralGroup = new QLabel("▼ General", genWidget);
    lblGeneralGroup->setStyleSheet("color: #38bdf8; font-weight: bold; font-size: 11px; margin-top: 2px;");
    formLayout->addRow(lblGeneralGroup);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setReadOnly(true);
    formLayout->addRow("Name:", m_nameEdit);

    m_typeEdit = new QLineEdit(this);
    m_typeEdit->setReadOnly(true);
    formLayout->addRow("Type:", m_typeEdit);

    m_stereotypeEdit = new QLineEdit(this);
    m_stereotypeEdit->setReadOnly(true);
    formLayout->addRow("Stereotype:", m_stereotypeEdit);

    auto aliasEdit = new QLineEdit(this);
    aliasEdit->setReadOnly(true);
    formLayout->addRow("Alias:", aliasEdit);

    auto keywordsEdit = new QLineEdit(this);
    keywordsEdit->setReadOnly(true);
    formLayout->addRow("Keywords:", keywordsEdit);

    m_statusEdit = new QLineEdit("Proposed", this);
    m_statusEdit->setReadOnly(true);
    formLayout->addRow("Status:", m_statusEdit);

    m_versionEdit = new QLineEdit("1.0", this);
    m_versionEdit->setReadOnly(true);
    formLayout->addRow("Version:", m_versionEdit);

    auto lblPkgGroup = new QLabel("▼ Package", genWidget);
    lblPkgGroup->setStyleSheet("color: #38bdf8; font-weight: bold; font-size: 11px; margin-top: 6px;");
    formLayout->addRow(lblPkgGroup);

    auto uriEdit = new QLineEdit(this);
    uriEdit->setReadOnly(true);
    formLayout->addRow("URI:", uriEdit);

    auto visEdit = new QLineEdit("Public", this);
    visEdit->setReadOnly(true);
    formLayout->addRow("Visibility:", visEdit);

    auto lblProjGroup = new QLabel("▼ Project", genWidget);
    lblProjGroup->setStyleSheet("color: #38bdf8; font-weight: bold; font-size: 11px; margin-top: 6px;");
    formLayout->addRow(lblProjGroup);

    m_authorEdit = new QLineEdit("Sparx Systems", this);
    m_authorEdit->setReadOnly(true);
    formLayout->addRow("Author:", m_authorEdit);

    m_guidEdit = new QLineEdit(this);
    m_guidEdit->setReadOnly(true);
    formLayout->addRow("GUID:", m_guidEdit);

    m_tabs->addTab(genWidget, "Element");

    auto tagsWidget = new QWidget(this);
    m_tabs->addTab(tagsWidget, "Tags");

    // 2. Attributes Tab
    m_attributesTable = new QTableWidget(this);
    m_attributesTable->setColumnCount(3);
    m_attributesTable->setHorizontalHeaderLabels({"Vis", "Name", "Type"});
    m_attributesTable->horizontalHeader()->setStretchLastSection(true);
    m_attributesTable->verticalHeader()->setVisible(false);
    m_tabs->addTab(m_attributesTable, "Attributes");

    // 3. Operations Tab
    m_operationsTable = new QTableWidget(this);
    m_operationsTable->setColumnCount(3);
    m_operationsTable->setHorizontalHeaderLabels({"Vis", "Name", "Return Type"});
    m_operationsTable->horizontalHeader()->setStretchLastSection(true);
    m_operationsTable->verticalHeader()->setVisible(false);
    m_tabs->addTab(m_operationsTable, "Operations");

    // 4. Connectors Tab
    m_connectorsTable = new QTableWidget(this);
    m_connectorsTable->setColumnCount(3);
    m_connectorsTable->setHorizontalHeaderLabels({"Type", "Target", "Direction"});
    m_connectorsTable->horizontalHeader()->setStretchLastSection(true);
    m_connectorsTable->verticalHeader()->setVisible(false);
    m_tabs->addTab(m_connectorsTable, "Connectors");
}

void PropertiesWidget::clear() {
    if (m_nameEdit) m_nameEdit->clear();
    if (m_typeEdit) m_typeEdit->clear();
    if (m_stereotypeEdit) m_stereotypeEdit->clear();
    if (m_statusEdit) m_statusEdit->clear();
    if (m_authorEdit) m_authorEdit->clear();
    if (m_versionEdit) m_versionEdit->clear();
    if (m_guidEdit) m_guidEdit->clear();
    if (m_idEdit) m_idEdit->clear();
    if (m_notesEdit) m_notesEdit->clear();
    if (m_attributesTable) m_attributesTable->setRowCount(0);
    if (m_operationsTable) m_operationsTable->setRowCount(0);
    if (m_connectorsTable) m_connectorsTable->setRowCount(0);
}

void PropertiesWidget::showElement(int elementId) {
    clear();
    auto elem = m_repo.GetElementByID(elementId);
    if (!elem) return;

    if (m_idEdit) m_idEdit->setText(QString::number(elem->GetElementID()));
    if (m_guidEdit) m_guidEdit->setText(QString::fromStdString(elem->GetElementGUID()));
    if (m_nameEdit) m_nameEdit->setText(QString::fromStdString(elem->GetName()));
    if (m_typeEdit) m_typeEdit->setText(QString::fromStdString(to_string(elem->GetType())));
    if (m_stereotypeEdit) m_stereotypeEdit->setText(QString::fromStdString(elem->GetStereotype()));
    if (m_statusEdit) m_statusEdit->setText(QString::fromStdString(elem->GetStatus()));
    if (m_authorEdit) m_authorEdit->setText(QString::fromStdString(elem->GetAuthor()));
    if (m_versionEdit) m_versionEdit->setText(QString::fromStdString(elem->GetVersion()));
    if (m_notesEdit) m_notesEdit->setText(QString::fromStdString(elem->GetNotes()));

    // Populate Attributes
    if (m_attributesTable) {
        m_attributesTable->setRowCount(static_cast<int>(elem->GetAttributes().size()));
        for (size_t i = 0; i < elem->GetAttributes().size(); ++i) {
            auto attr = elem->GetAttributes().GetAt(static_cast<short>(i));
            if (!attr) continue;
            QString vis = "+";
            if (attr->GetVisibility() == ea::Scope::Private) vis = "-";
            else if (attr->GetVisibility() == ea::Scope::Protected) vis = "#";
            else if (attr->GetVisibility() == ea::Scope::Package) vis = "~";

            m_attributesTable->setItem(static_cast<int>(i), 0, new QTableWidgetItem(vis));
            m_attributesTable->setItem(static_cast<int>(i), 1, new QTableWidgetItem(QString::fromStdString(attr->GetName())));
            m_attributesTable->setItem(static_cast<int>(i), 2, new QTableWidgetItem(QString::fromStdString(attr->GetType())));
        }
    }

    // Populate Operations
    if (m_operationsTable) {
        m_operationsTable->setRowCount(static_cast<int>(elem->GetMethods().size()));
        for (size_t i = 0; i < elem->GetMethods().size(); ++i) {
            auto m = elem->GetMethods().GetAt(static_cast<short>(i));
            if (!m) continue;
            QString vis = "+";
            if (m->GetVisibility() == ea::Scope::Private) vis = "-";
            else if (m->GetVisibility() == ea::Scope::Protected) vis = "#";
            else if (m->GetVisibility() == ea::Scope::Package) vis = "~";

            m_operationsTable->setItem(static_cast<int>(i), 0, new QTableWidgetItem(vis));
            m_operationsTable->setItem(static_cast<int>(i), 1, new QTableWidgetItem(QString::fromStdString(m->GetName())));
            m_operationsTable->setItem(static_cast<int>(i), 2, new QTableWidgetItem(QString::fromStdString(m->GetReturnType())));
        }
    }

    // Populate Connectors
    if (m_connectorsTable) {
        m_connectorsTable->setRowCount(static_cast<int>(elem->GetConnectors().size()));
        for (size_t i = 0; i < elem->GetConnectors().size(); ++i) {
            auto c = elem->GetConnectors().GetAt(static_cast<short>(i));
            if (!c) continue;
            int targetId = (c->GetClientID() == elem->GetElementID()) ? c->GetSupplierID() : c->GetClientID();
            auto target = m_repo.GetElementByID(targetId);
            QString targetName = target ? QString::fromStdString(target->GetName()) : QString("ID: %1").arg(targetId);

            m_connectorsTable->setItem(static_cast<int>(i), 0, new QTableWidgetItem(QString::fromStdString(to_string(c->GetType()))));
            m_connectorsTable->setItem(static_cast<int>(i), 1, new QTableWidgetItem(targetName));
            m_connectorsTable->setItem(static_cast<int>(i), 2, new QTableWidgetItem(QString::fromStdString(c->GetDirection())));
        }
    }
}

void PropertiesWidget::showDiagram(int diagramId) {
    clear();
    auto diag = m_repo.GetDiagramByID(diagramId);
    if (!diag) return;

    if (m_idEdit) m_idEdit->setText(QString::number(diag->GetDiagramID()));
    if (m_guidEdit) m_guidEdit->setText(QString::fromStdString(diag->GetDiagramGUID()));
    if (m_nameEdit) m_nameEdit->setText(QString::fromStdString(diag->GetName()));
    if (m_typeEdit) m_typeEdit->setText(QString::fromStdString(to_string(diag->GetType())));
    if (m_stereotypeEdit) m_stereotypeEdit->setText(QString::fromStdString(diag->GetStereotype()));
    if (m_authorEdit) m_authorEdit->setText(QString::fromStdString(diag->GetAuthor()));
    if (m_versionEdit) m_versionEdit->setText(QString::fromStdString(diag->GetVersion()));
    if (m_notesEdit) m_notesEdit->setText(QString::fromStdString(diag->GetNotes()));
}
