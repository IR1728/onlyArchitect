#include "EditElementDialog.hpp"
#include "DarkTheme.hpp"
#include "M3Icons.hpp"
#include "ea/editor/FeatureText.hpp"
#include <QMessageBox>
#include <QInputDialog>

namespace {

bool supportsClassFeatures(ea::ObjectType t) {
    switch (t) {
        case ea::ObjectType::Class:
        case ea::ObjectType::Interface:
        case ea::ObjectType::Enumeration:
        case ea::ObjectType::DataType:
        case ea::ObjectType::Component:
        case ea::ObjectType::Object:
        case ea::ObjectType::Lifeline:
            return true;
        default:
            return false;
    }
}

} // namespace

EditElementDialog::EditElementDialog(std::shared_ptr<ea::Element> elem, ea::Repository& repo, QWidget* parent)
    : QDialog(parent), m_elem(elem), m_repo(repo) {
    m_originalType = m_elem ? m_elem->GetType() : ea::ObjectType::Class;
    m_originalTypeLabel = QString::fromStdString(to_string(m_originalType));
    m_showCodeFeatures = supportsClassFeatures(m_originalType);

    setWindowTitle(QString("Editar Elemento: %1").arg(elem ? QString::fromStdString(elem->GetName()) : ""));
    resize(600, m_showCodeFeatures ? 620 : 440);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    // Form
    auto form = new QFormLayout();
    form->setSpacing(8);
    m_nameEdit = new QLineEdit(this);
    if (m_elem) m_nameEdit->setText(QString::fromStdString(m_elem->GetName()));
    form->addRow("Nombre:", m_nameEdit);

    m_stereotypeEdit = new QLineEdit(this);
    m_stereotypeEdit->setPlaceholderText("Ej. entity, control, boundary, service...");
    if (m_elem) m_stereotypeEdit->setText(QString::fromStdString(m_elem->GetStereotype()));
    form->addRow("Estereotipo:", m_stereotypeEdit);

    m_typeCombo = new QComboBox(this);
    const QStringList knownTypes = {
        "Class", "Interface", "Enumeration", "DataType", "Actor", "UseCase", "Boundary",
        "Lifeline", "InteractionFragment", "Component", "Package", "Requirement",
        "State", "Activity", "Action", "Object", "Node", "Artifact", "Note"
    };
    m_typeCombo->addItems(knownTypes);
    // CRITICAL: Always preserve the element's exact existing type so Actors/Lifelines/Fragments are never corrupted to Class
    if (!m_originalTypeLabel.isEmpty() && m_typeCombo->findText(m_originalTypeLabel) < 0) {
        m_typeCombo->addItem(m_originalTypeLabel);
    }
    m_typeCombo->setCurrentText(m_originalTypeLabel);
    form->addRow("Tipo UML:", m_typeCombo);

    m_statusCombo = new QComboBox(this);
    m_statusCombo->setEditable(true);
    m_statusCombo->addItems({"Proposed", "Approved", "Implemented", "Mandatory", "Validated"});
    if (m_elem && !m_elem->GetStatus().empty()) {
        m_statusCombo->setEditText(QString::fromStdString(m_elem->GetStatus()));
    }
    form->addRow("Estado:", m_statusCombo);

    mainLayout->addLayout(form);

    // Notes / Use Case Specification section
    auto notesHeader = new QHBoxLayout();
    const bool isUseCase = (m_originalType == ea::ObjectType::UseCase);
    notesHeader->addWidget(new QLabel(isUseCase ? "<b>Especificación / Escenario del Caso de Uso:</b>" : "<b>Notas / Descripción:</b>", this));
    notesHeader->addStretch();
    if (isUseCase) {
        auto tplBtn = new QPushButton("+ Plantilla CUS", this);
        tplBtn->setToolTip("Inserta estructura estándar: Actor, Precondiciones, Flujo Principal, Alternos y Postcondiciones");
        ea::gui::M3Icons::decorate(tplBtn, "article", 15);
        ea::gui::setThemedStyleSheet(tplBtn, "QPushButton { padding: 4px 10px; font-size: 11px; color: @primary; }");
        connect(tplBtn, &QPushButton::clicked, this, &EditElementDialog::onInsertUseCaseTemplate);
        notesHeader->addWidget(tplBtn);
    }
    mainLayout->addLayout(notesHeader);

    m_notesEdit = new QTextEdit(this);
    m_notesEdit->setMinimumHeight(m_showCodeFeatures ? 72 : 165);
    if (m_showCodeFeatures) m_notesEdit->setMaximumHeight(95);
    if (m_elem) m_notesEdit->setPlainText(QString::fromStdString(m_elem->GetNotes()));
    mainLayout->addWidget(m_notesEdit, m_showCodeFeatures ? 0 : 1);

    if (m_showCodeFeatures) {
        // Attributes Table (preserves visibility, name, type, multiplicity)
        auto attrGroup = new QVBoxLayout();
        auto attrHeader = new QHBoxLayout();
        attrHeader->addWidget(new QLabel("<b>Atributos:</b>", this));
        auto addAttrBtn = new QPushButton("Añadir", this);
        ea::gui::M3Icons::decorate(addAttrBtn, "add", 15);
        auto remAttrBtn = new QPushButton("Quitar", this);
        ea::gui::M3Icons::decorate(remAttrBtn, "remove", 15);
        attrHeader->addStretch();
        attrHeader->addWidget(addAttrBtn);
        attrHeader->addWidget(remAttrBtn);
        attrGroup->addLayout(attrHeader);

        m_attrTable = new QTableWidget(this);
        m_attrTable->setColumnCount(4);
        m_attrTable->setHorizontalHeaderLabels({"Vis", "Nombre", "Tipo", "Mult."});
        m_attrTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_attrTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_attrTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        m_attrTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        m_attrTable->setMaximumHeight(130);
        attrGroup->addWidget(m_attrTable);
        mainLayout->addLayout(attrGroup);

        // Methods Table (preserves parameters!)
        auto methGroup = new QVBoxLayout();
        auto methHeader = new QHBoxLayout();
        methHeader->addWidget(new QLabel("<b>Operaciones / Métodos:</b>", this));
        auto addMethBtn = new QPushButton("Añadir", this);
        ea::gui::M3Icons::decorate(addMethBtn, "add", 15);
        auto remMethBtn = new QPushButton("Quitar", this);
        ea::gui::M3Icons::decorate(remMethBtn, "remove", 15);
        methHeader->addStretch();
        methHeader->addWidget(addMethBtn);
        methHeader->addWidget(remMethBtn);
        methGroup->addLayout(methHeader);

        m_methTable = new QTableWidget(this);
        m_methTable->setColumnCount(4);
        m_methTable->setHorizontalHeaderLabels({"Vis", "Nombre", "Parámetros", "Retorno"});
        m_methTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_methTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_methTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        m_methTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        m_methTable->setMaximumHeight(130);
        methGroup->addWidget(m_methTable);
        mainLayout->addLayout(methGroup);

        if (m_elem) {
            m_attrTable->setRowCount(static_cast<int>(m_elem->GetAttributes().size()));
            for (size_t i = 0; i < m_elem->GetAttributes().size(); ++i) {
                auto a = m_elem->GetAttributes().GetAt(static_cast<short>(i));
                if (!a) continue;
                const QString vis = QString::fromStdString(ea::editor::visibility_symbol(a->GetVisibility()));
                const QString mult = QString::fromStdString(
                    ea::editor::format_multiplicity({a->GetLowerBound(), a->GetUpperBound()}));
                auto* nameItem = new QTableWidgetItem(QString::fromStdString(a->GetName()));
                nameItem->setData(Qt::UserRole, static_cast<int>(i));
                m_attrTable->setItem(static_cast<int>(i), 0, new QTableWidgetItem(vis));
                m_attrTable->setItem(static_cast<int>(i), 1, nameItem);
                m_attrTable->setItem(static_cast<int>(i), 2, new QTableWidgetItem(QString::fromStdString(a->GetType())));
                m_attrTable->setItem(static_cast<int>(i), 3, new QTableWidgetItem(mult));
            }

            m_methTable->setRowCount(static_cast<int>(m_elem->GetMethods().size()));
            for (size_t i = 0; i < m_elem->GetMethods().size(); ++i) {
                auto m = m_elem->GetMethods().GetAt(static_cast<short>(i));
                if (!m) continue;
                const QString vis = QString::fromStdString(ea::editor::visibility_symbol(m->GetVisibility()));
                const QString params = QString::fromStdString(ea::editor::format_parameters(m->GetParameters()));
                auto* nameItem = new QTableWidgetItem(QString::fromStdString(m->GetName()));
                nameItem->setData(Qt::UserRole, static_cast<int>(i));
                m_methTable->setItem(static_cast<int>(i), 0, new QTableWidgetItem(vis));
                m_methTable->setItem(static_cast<int>(i), 1, nameItem);
                m_methTable->setItem(static_cast<int>(i), 2, new QTableWidgetItem(params));
                m_methTable->setItem(static_cast<int>(i), 3, new QTableWidgetItem(QString::fromStdString(m->GetReturnType())));
            }
        }

        connect(addAttrBtn, &QPushButton::clicked, this, &EditElementDialog::onAddAttribute);
        connect(remAttrBtn, &QPushButton::clicked, this, &EditElementDialog::onRemoveAttribute);
        connect(addMethBtn, &QPushButton::clicked, this, &EditElementDialog::onAddMethod);
        connect(remMethBtn, &QPushButton::clicked, this, &EditElementDialog::onRemoveMethod);
    }

    // Dialog buttons
    auto btnBox = new QHBoxLayout();
    btnBox->addStretch();
    auto cancelBtn = new QPushButton("Cancelar", this);
    auto saveBtn = new QPushButton("Guardar y Aplicar", this);
    ea::gui::setThemedStyleSheet(saveBtn, "background-color: @primaryStrong; color: white; font-weight: bold; padding: 7px 16px;");
    btnBox->addWidget(cancelBtn);
    btnBox->addWidget(saveBtn);
    mainLayout->addLayout(btnBox);

    connect(saveBtn, &QPushButton::clicked, this, &EditElementDialog::onSave);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    m_nameEdit->setFocus();
    m_nameEdit->selectAll();
}

void EditElementDialog::onAddAttribute() {
    if (!m_attrTable) return;
    int row = m_attrTable->rowCount();
    m_attrTable->insertRow(row);
    m_attrTable->setItem(row, 0, new QTableWidgetItem("-"));
    m_attrTable->setItem(row, 1, new QTableWidgetItem("nuevoAtributo"));
    m_attrTable->setItem(row, 2, new QTableWidgetItem("string"));
    m_attrTable->setItem(row, 3, new QTableWidgetItem("1"));
}

void EditElementDialog::onRemoveAttribute() {
    if (!m_attrTable) return;
    int row = m_attrTable->currentRow();
    if (row >= 0) m_attrTable->removeRow(row);
}

void EditElementDialog::onAddMethod() {
    if (!m_methTable) return;
    int row = m_methTable->rowCount();
    m_methTable->insertRow(row);
    m_methTable->setItem(row, 0, new QTableWidgetItem("+"));
    m_methTable->setItem(row, 1, new QTableWidgetItem("nuevaOperacion"));
    m_methTable->setItem(row, 2, new QTableWidgetItem(""));
    m_methTable->setItem(row, 3, new QTableWidgetItem("void"));
}

void EditElementDialog::onRemoveMethod() {
    if (!m_methTable) return;
    int row = m_methTable->currentRow();
    if (row >= 0) m_methTable->removeRow(row);
}

void EditElementDialog::onInsertUseCaseTemplate() {
    const QString tpl =
        "Actor Principal:\n- \n\n"
        "Precondiciones:\n- \n\n"
        "Flujo Principal:\n"
        "1. El actor solicita ...\n"
        "2. El sistema valida ...\n"
        "3. El sistema registra y confirma ...\n\n"
        "Flujos Alternos / Excepciones:\n"
        "- A1: Si los datos son inválidos, el sistema notifica el error.\n\n"
        "Postcondiciones:\n- ";
    if (m_notesEdit->toPlainText().trimmed().isEmpty()) {
        m_notesEdit->setPlainText(tpl);
    } else {
        m_notesEdit->append("\n" + tpl);
    }
}

void EditElementDialog::onSave() {
    if (!m_elem) {
        reject();
        return;
    }

    m_elem->SetName(m_nameEdit->text().trimmed().toStdString());
    m_elem->SetStereotype(m_stereotypeEdit->text().trimmed().toStdString());
    // Only change ObjectType if the user actually chose a different type (never corrupt exotic types!)
    if (m_typeCombo->currentText() != m_originalTypeLabel) {
        const auto parsed = ea::object_type_from_string(m_typeCombo->currentText().toStdString());
        if (parsed != ea::ObjectType::Unknown) {
            m_elem->SetType(parsed);
            m_elem->SetStoredTypeName("");
        }
    }
    m_elem->SetStatus(m_statusCombo->currentText().trimmed().toStdString());
    m_elem->SetNotes(m_notesEdit->toPlainText().toStdString());

    if (m_showCodeFeatures && m_attrTable && m_methTable) {
        // Snapshot existing attributes & methods so metadata (static, const, default, guid) is preserved
        std::vector<std::shared_ptr<ea::Attribute>> prevAttrs(m_elem->GetAttributes().begin(), m_elem->GetAttributes().end());
        std::vector<std::shared_ptr<ea::Method>> prevMethods(m_elem->GetMethods().begin(), m_elem->GetMethods().end());

        m_elem->GetAttributes().Clear();
        for (int r = 0; r < m_attrTable->rowCount(); ++r) {
            const QString vis = m_attrTable->item(r, 0) ? m_attrTable->item(r, 0)->text().trimmed() : "-";
            const QString name = m_attrTable->item(r, 1) ? m_attrTable->item(r, 1)->text().trimmed() : "attr";
            const QString type = m_attrTable->item(r, 2) ? m_attrTable->item(r, 2)->text().trimmed() : "string";
            const QString mult = m_attrTable->item(r, 3) ? m_attrTable->item(r, 3)->text().trimmed() : "1";
            if (name.isEmpty()) continue;

            auto attr = m_elem->GetAttributes().AddNew(name.toStdString(), type.toStdString());
            const auto parsedVis = ea::editor::parse_visibility(vis.toStdString());
            attr->SetVisibility(parsedVis.value_or(ea::Scope::Private));
            if (auto m = ea::editor::parse_multiplicity(mult.toStdString())) {
                attr->SetLowerBound(m->lower);
                attr->SetUpperBound(m->upper);
            }
            const QVariant origIdx = m_attrTable->item(r, 1) ? m_attrTable->item(r, 1)->data(Qt::UserRole) : QVariant();
            if (origIdx.isValid()) {
                const size_t idx = static_cast<size_t>(origIdx.toInt());
                if (idx < prevAttrs.size() && prevAttrs[idx]) {
                    attr->SetStatic(prevAttrs[idx]->IsStatic());
                    attr->SetConst(prevAttrs[idx]->IsConst());
                    attr->SetDefault(prevAttrs[idx]->GetDefault());
                    attr->SetNotes(prevAttrs[idx]->GetNotes());
                }
            }
        }

        m_elem->GetMethods().Clear();
        for (int r = 0; r < m_methTable->rowCount(); ++r) {
            const QString vis = m_methTable->item(r, 0) ? m_methTable->item(r, 0)->text().trimmed() : "+";
            const QString name = m_methTable->item(r, 1) ? m_methTable->item(r, 1)->text().trimmed() : "op";
            const QString paramsStr = m_methTable->item(r, 2) ? m_methTable->item(r, 2)->text().trimmed() : "";
            const QString ret = m_methTable->item(r, 3) ? m_methTable->item(r, 3)->text().trimmed() : "void";
            if (name.isEmpty()) continue;

            auto meth = m_elem->GetMethods().AddNew(name.toStdString(), ret.toStdString());
            const auto parsedVis = ea::editor::parse_visibility(vis.toStdString());
            meth->SetVisibility(parsedVis.value_or(ea::Scope::Public));
            auto parsedParams = ea::editor::parse_parameters(paramsStr.toStdString());
            if (parsedParams.ok()) {
                meth->SetParameters(std::move(parsedParams.parameters));
            }
            const QVariant origIdx = m_methTable->item(r, 1) ? m_methTable->item(r, 1)->data(Qt::UserRole) : QVariant();
            if (origIdx.isValid()) {
                const size_t idx = static_cast<size_t>(origIdx.toInt());
                if (idx < prevMethods.size() && prevMethods[idx]) {
                    meth->SetStatic(prevMethods[idx]->IsStatic());
                    meth->SetAbstract(prevMethods[idx]->IsAbstract());
                    meth->SetConst(prevMethods[idx]->IsConst());
                    meth->SetNotes(prevMethods[idx]->GetNotes());
                    if (!parsedParams.ok()) {
                        meth->SetParameters(prevMethods[idx]->GetParameters());
                    }
                }
            }
        }
    }

    m_repo.SaveElement(*m_elem);
    m_modified = true;
    accept();
}
