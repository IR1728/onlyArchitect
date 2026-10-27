#include "EditElementDialog.hpp"
#include <QMessageBox>
#include <QInputDialog>

EditElementDialog::EditElementDialog(std::shared_ptr<ea::Element> elem, ea::Repository& repo, QWidget* parent)
    : QDialog(parent), m_elem(elem), m_repo(repo) {
    setWindowTitle(QString("Edit Element: %1").arg(elem ? QString::fromStdString(elem->GetName()) : ""));
    resize(550, 600);

    auto mainLayout = new QVBoxLayout(this);

    // Form
    auto form = new QFormLayout();
    m_nameEdit = new QLineEdit(this);
    if (m_elem) m_nameEdit->setText(QString::fromStdString(m_elem->GetName()));
    form->addRow("Name:", m_nameEdit);

    m_stereotypeEdit = new QLineEdit(this);
    if (m_elem) m_stereotypeEdit->setText(QString::fromStdString(m_elem->GetStereotype()));
    form->addRow("Stereotype:", m_stereotypeEdit);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItems({"Class", "Interface", "Enumeration", "Package", "Component", "UseCase", "Note"});
    if (m_elem) {
        m_typeCombo->setCurrentText(QString::fromStdString(to_string(m_elem->GetType())));
    }
    form->addRow("Type:", m_typeCombo);

    m_statusEdit = new QLineEdit(this);
    if (m_elem) m_statusEdit->setText(QString::fromStdString(m_elem->GetStatus()));
    form->addRow("Status:", m_statusEdit);

    m_notesEdit = new QTextEdit(this);
    m_notesEdit->setMaximumHeight(70);
    if (m_elem) m_notesEdit->setText(QString::fromStdString(m_elem->GetNotes()));
    form->addRow("Notes:", m_notesEdit);

    mainLayout->addLayout(form);

    // Attributes Table
    auto attrGroup = new QVBoxLayout();
    auto attrHeader = new QHBoxLayout();
    attrHeader->addWidget(new QLabel("<b>Attributes:</b>", this));
    auto addAttrBtn = new QPushButton("➕ Add", this);
    auto remAttrBtn = new QPushButton("➖ Remove", this);
    attrHeader->addStretch();
    attrHeader->addWidget(addAttrBtn);
    attrHeader->addWidget(remAttrBtn);
    attrGroup->addLayout(attrHeader);

    m_attrTable = new QTableWidget(this);
    m_attrTable->setColumnCount(3);
    m_attrTable->setHorizontalHeaderLabels({"Vis", "Name", "Type"});
    m_attrTable->horizontalHeader()->setStretchLastSection(true);
    m_attrTable->setMaximumHeight(130);
    attrGroup->addWidget(m_attrTable);
    mainLayout->addLayout(attrGroup);

    // Methods Table
    auto methGroup = new QVBoxLayout();
    auto methHeader = new QHBoxLayout();
    methHeader->addWidget(new QLabel("<b>Operations / Methods:</b>", this));
    auto addMethBtn = new QPushButton("➕ Add", this);
    auto remMethBtn = new QPushButton("➖ Remove", this);
    methHeader->addStretch();
    methHeader->addWidget(addMethBtn);
    methHeader->addWidget(remMethBtn);
    methGroup->addLayout(methHeader);

    m_methTable = new QTableWidget(this);
    m_methTable->setColumnCount(3);
    m_methTable->setHorizontalHeaderLabels({"Vis", "Name", "Return Type"});
    m_methTable->horizontalHeader()->setStretchLastSection(true);
    m_methTable->setMaximumHeight(130);
    methGroup->addWidget(m_methTable);
    mainLayout->addLayout(methGroup);

    // Populate tables
    if (m_elem) {
        m_attrTable->setRowCount(static_cast<int>(m_elem->GetAttributes().size()));
        for (size_t i = 0; i < m_elem->GetAttributes().size(); ++i) {
            auto a = m_elem->GetAttributes().GetAt(static_cast<short>(i));
            if (!a) continue;
            m_attrTable->setItem(static_cast<int>(i), 0, new QTableWidgetItem(to_string(a->GetVisibility()) == "Public" ? "+" : "-"));
            m_attrTable->setItem(static_cast<int>(i), 1, new QTableWidgetItem(QString::fromStdString(a->GetName())));
            m_attrTable->setItem(static_cast<int>(i), 2, new QTableWidgetItem(QString::fromStdString(a->GetType())));
        }

        m_methTable->setRowCount(static_cast<int>(m_elem->GetMethods().size()));
        for (size_t i = 0; i < m_elem->GetMethods().size(); ++i) {
            auto m = m_elem->GetMethods().GetAt(static_cast<short>(i));
            if (!m) continue;
            m_methTable->setItem(static_cast<int>(i), 0, new QTableWidgetItem(to_string(m->GetVisibility()) == "Public" ? "+" : "-"));
            m_methTable->setItem(static_cast<int>(i), 1, new QTableWidgetItem(QString::fromStdString(m->GetName())));
            m_methTable->setItem(static_cast<int>(i), 2, new QTableWidgetItem(QString::fromStdString(m->GetReturnType())));
        }
    }

    // Dialog buttons
    auto btnBox = new QHBoxLayout();
    btnBox->addStretch();
    auto cancelBtn = new QPushButton("Cancel", this);
    auto saveBtn = new QPushButton("Save & Apply", this);
    saveBtn->setStyleSheet("background-color: #007acc; color: white; font-weight: bold; padding: 6px 14px;");
    btnBox->addWidget(cancelBtn);
    btnBox->addWidget(saveBtn);
    mainLayout->addLayout(btnBox);

    connect(addAttrBtn, &QPushButton::clicked, this, &EditElementDialog::onAddAttribute);
    connect(remAttrBtn, &QPushButton::clicked, this, &EditElementDialog::onRemoveAttribute);
    connect(addMethBtn, &QPushButton::clicked, this, &EditElementDialog::onAddMethod);
    connect(remMethBtn, &QPushButton::clicked, this, &EditElementDialog::onRemoveMethod);
    connect(saveBtn, &QPushButton::clicked, this, &EditElementDialog::onSave);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void EditElementDialog::onAddAttribute() {
    int row = m_attrTable->rowCount();
    m_attrTable->insertRow(row);
    m_attrTable->setItem(row, 0, new QTableWidgetItem("+"));
    m_attrTable->setItem(row, 1, new QTableWidgetItem("newAttribute"));
    m_attrTable->setItem(row, 2, new QTableWidgetItem("int"));
}

void EditElementDialog::onRemoveAttribute() {
    int row = m_attrTable->currentRow();
    if (row >= 0) {
        m_attrTable->removeRow(row);
    }
}

void EditElementDialog::onAddMethod() {
    int row = m_methTable->rowCount();
    m_methTable->insertRow(row);
    m_methTable->setItem(row, 0, new QTableWidgetItem("+"));
    m_methTable->setItem(row, 1, new QTableWidgetItem("newOperation"));
    m_methTable->setItem(row, 2, new QTableWidgetItem("void"));
}

void EditElementDialog::onRemoveMethod() {
    int row = m_methTable->currentRow();
    if (row >= 0) {
        m_methTable->removeRow(row);
    }
}

void EditElementDialog::onSave() {
    if (!m_elem) {
        reject();
        return;
    }

    m_elem->SetName(m_nameEdit->text().toStdString());
    m_elem->SetStereotype(m_stereotypeEdit->text().toStdString());
    m_elem->SetType(ea::object_type_from_string(m_typeCombo->currentText().toStdString()));
    m_elem->SetStatus(m_statusEdit->text().toStdString());
    m_elem->SetNotes(m_notesEdit->toPlainText().toStdString());

    // Update attributes in Element
    m_elem->GetAttributes().Clear();
    for (int r = 0; r < m_attrTable->rowCount(); ++r) {
        QString vis = m_attrTable->item(r, 0) ? m_attrTable->item(r, 0)->text() : "+";
        QString name = m_attrTable->item(r, 1) ? m_attrTable->item(r, 1)->text() : "attr";
        QString type = m_attrTable->item(r, 2) ? m_attrTable->item(r, 2)->text() : "int";

        auto attr = m_elem->GetAttributes().AddNew(name.toStdString(), type.toStdString());
        attr->SetVisibility(ea::scope_from_string(vis.toStdString()));
    }

    // Update methods in Element
    m_elem->GetMethods().Clear();
    for (int r = 0; r < m_methTable->rowCount(); ++r) {
        QString vis = m_methTable->item(r, 0) ? m_methTable->item(r, 0)->text() : "+";
        QString name = m_methTable->item(r, 1) ? m_methTable->item(r, 1)->text() : "op";
        QString ret = m_methTable->item(r, 2) ? m_methTable->item(r, 2)->text() : "void";

        auto meth = m_elem->GetMethods().AddNew(name.toStdString(), ret.toStdString());
        meth->SetVisibility(ea::scope_from_string(vis.toStdString()));
    }

    // Persist element to SQLite database
    if (m_repo.GetStorageEngine()) {
        m_repo.GetStorageEngine()->SaveElement(*m_elem);
    }

    m_modified = true;
    accept();
}
