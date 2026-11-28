#include "EditConnectorDialog.hpp"
#include "DarkTheme.hpp"
#include "M3Icons.hpp"
#include "ea/diagram/SequenceModel.hpp"
#include "ea/editor/FeatureText.hpp"
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

const QStringList kMultiplicities = {"", "1", "0..1", "0..*", "1..*", "*", "0..n", "1..n"};
const QStringList kDirections = {
    "Unspecified",
    "Source -> Destination",
    "Destination -> Source",
    "Bi-Directional"
};
const QStringList kStereotypes = {"", "include", "extend", "uses", "create", "destroy", "deploy", "manifest", "import"};

QString elementLabel(ea::Repository& repo, int id) {
    if (auto e = repo.GetElementByID(id)) {
        return QString("%1 (%2)").arg(QString::fromStdString(e->GetName()), QString::fromStdString(to_string(e->GetType())));
    }
    return QString("#%1").arg(id);
}

QStringList operationsForTarget(ea::Repository& repo, int targetId) {
    QStringList ops;
    auto target = repo.GetElementByID(targetId);
    if (!target) return ops;

    auto collect = [&](const ea::Element& elem) {
        for (size_t i = 0; i < elem.GetMethods().size(); ++i) {
            if (auto m = elem.GetMethods().GetAt(static_cast<short>(i))) {
                const QString params = QString::fromStdString(ea::editor::format_parameters(m->GetParameters()));
                QString sig = QString("%1(%2)").arg(QString::fromStdString(m->GetName()), params);
                if (!m->GetReturnType().empty() && m->GetReturnType() != "void") {
                    sig += QString(" : %1").arg(QString::fromStdString(m->GetReturnType()));
                }
                if (!ops.contains(sig)) ops.append(sig);
            }
        }
    };

    collect(*target);
    if (target->GetClassifierID() > 0) {
        if (auto cls = repo.GetElementByID(target->GetClassifierID())) collect(*cls);
    }
    return ops;
}

} // namespace

EditConnectorDialog::EditConnectorDialog(std::shared_ptr<ea::Connector> conn, ea::Repository& repo, QWidget* parent)
    : QDialog(parent), m_conn(std::move(conn)), m_repo(repo) {
    m_isMessage = m_conn && m_conn->GetType() == ea::ConnectorType::Message;
    setWindowTitle(m_isMessage ? "Editar Mensaje de Secuencia" : "Editar Relación / Conector UML");
    resize(500, m_isMessage ? 360 : 500);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);

    if (m_conn) {
        const QString srcName = elementLabel(m_repo, m_conn->GetClientID());
        const QString dstName = elementLabel(m_repo, m_conn->GetSupplierID());
        auto* banner = new QLabel(QString("<b>%1</b>  →  <b>%2</b>").arg(srcName.toHtmlEscaped(), dstName.toHtmlEscaped()), this);
        banner->setWordWrap(true);
        ea::gui::setThemedStyleSheet(banner, "background-color: @surfaceContainer; color: @primary; padding: 8px 12px; border-radius: 8px; font-size: 12px;");
        root->addWidget(banner);
    }

    auto* form = new QFormLayout();
    form->setSpacing(8);

    m_nameCombo = new QComboBox(this);
    m_nameCombo->setEditable(true);
    if (m_conn && m_isMessage) {
        const QStringList targetOps = operationsForTarget(m_repo, m_conn->GetSupplierID());
        for (const QString& sig : targetOps) m_nameCombo->addItem(sig);
    }
    m_nameCombo->setEditText(m_conn ? QString::fromStdString(m_conn->GetName()) : QString());
    m_nameCombo->lineEdit()->setPlaceholderText(m_isMessage ? "Ej. autenticar(usuario, clave) o elige una operación..."
                                                            : "Ej. contiene, gestiona, [condición]...");
    form->addRow(m_isMessage ? "Mensaje / Operación:" : "Nombre / Etiqueta:", m_nameCombo);

    if (m_isMessage) {
        m_messageSortCombo = new QComboBox(this);
        m_messageSortCombo->addItem("Síncrono (llamada)", static_cast<int>(ea::sequence::MessageSort::Synchronous));
        m_messageSortCombo->addItem("Asíncrono", static_cast<int>(ea::sequence::MessageSort::Asynchronous));
        m_messageSortCombo->addItem("Respuesta (Reply)", static_cast<int>(ea::sequence::MessageSort::Reply));
        m_messageSortCombo->addItem("Crear instancia («create»)", static_cast<int>(ea::sequence::MessageSort::Create));
        m_messageSortCombo->addItem("Destruir instancia («destroy»)", static_cast<int>(ea::sequence::MessageSort::Delete));
        if (m_conn) {
            const int sortVal = static_cast<int>(ea::sequence::message_sort(*m_conn));
            const int idx = m_messageSortCombo->findData(sortVal);
            if (idx >= 0) m_messageSortCombo->setCurrentIndex(idx);
        }
        form->addRow("Tipo de mensaje:", m_messageSortCombo);
    } else {
        m_typeCombo = new QComboBox(this);
        const QStringList types = {
            "Association", "Aggregation", "Composition", "Generalization",
            "Realization", "Dependency", "UseCase", "ControlFlow",
            "ObjectFlow", "StateFlow", "Assembly", "Delegate", "NoteLink"
        };
        m_typeCombo->addItems(types);
        if (m_conn) {
            const QString cur = QString::fromStdString(to_string(m_conn->GetType()));
            if (m_typeCombo->findText(cur) < 0 && !cur.isEmpty()) m_typeCombo->addItem(cur);
            m_typeCombo->setCurrentText(cur);
        }
        form->addRow("Tipo UML:", m_typeCombo);

        m_stereotypeCombo = new QComboBox(this);
        m_stereotypeCombo->setEditable(true);
        m_stereotypeCombo->addItems(kStereotypes);
        if (m_conn) m_stereotypeCombo->setEditText(QString::fromStdString(m_conn->GetStereotype()));
        m_stereotypeCombo->lineEdit()->setPlaceholderText("Ej. include, extend, uses...");
        form->addRow("Estereotipo («...»):", m_stereotypeCombo);

        m_directionCombo = new QComboBox(this);
        m_directionCombo->addItems(kDirections);
        if (m_conn && !m_conn->GetDirection().empty()) {
            const QString dir = QString::fromStdString(m_conn->GetDirection());
            if (m_directionCombo->findText(dir) < 0) m_directionCombo->addItem(dir);
            m_directionCombo->setCurrentText(dir);
        }
        form->addRow("Navegabilidad:", m_directionCombo);
    }

    root->addLayout(form);

    // Source & Target Roles + Multiplicities for Domain / Class / Structural diagrams
    if (!m_isMessage) {
        auto* endsRow = new QHBoxLayout();
        endsRow->setSpacing(10);

        auto* srcGroup = new QGroupBox("Extremo Origen (Client)", this);
        auto* srcForm = new QFormLayout(srcGroup);
        m_sourceRoleEdit = new QLineEdit(m_conn ? QString::fromStdString(m_conn->GetClientRole()) : QString(), srcGroup);
        m_sourceRoleEdit->setPlaceholderText("Ej. pedido");
        m_sourceMultCombo = new QComboBox(srcGroup);
        m_sourceMultCombo->setEditable(true);
        m_sourceMultCombo->addItems(kMultiplicities);
        if (m_conn) m_sourceMultCombo->setEditText(QString::fromStdString(m_conn->GetClientMultiplicity()));
        srcForm->addRow("Rol:", m_sourceRoleEdit);
        srcForm->addRow("Multiplicidad:", m_sourceMultCombo);

        auto* dstGroup = new QGroupBox("Extremo Destino (Supplier)", this);
        auto* dstForm = new QFormLayout(dstGroup);
        m_targetRoleEdit = new QLineEdit(m_conn ? QString::fromStdString(m_conn->GetSupplierRole()) : QString(), dstGroup);
        m_targetRoleEdit->setPlaceholderText("Ej. items");
        m_targetMultCombo = new QComboBox(dstGroup);
        m_targetMultCombo->setEditable(true);
        m_targetMultCombo->addItems(kMultiplicities);
        if (m_conn) m_targetMultCombo->setEditText(QString::fromStdString(m_conn->GetSupplierMultiplicity()));
        dstForm->addRow("Rol:", m_targetRoleEdit);
        dstForm->addRow("Multiplicidad:", m_targetMultCombo);

        endsRow->addWidget(srcGroup, 1);
        endsRow->addWidget(dstGroup, 1);
        root->addLayout(endsRow);
    }

    auto* notesLabel = new QLabel(m_isMessage ? "Guarda / Notas del mensaje:" : "Notas / Condición de extensión:", this);
    ea::gui::setThemedStyleSheet(notesLabel, "color: @onSurfaceVariant; font-size: 11.5px; font-weight: 600;");
    root->addWidget(notesLabel);

    m_notesEdit = new QTextEdit(this);
    m_notesEdit->setMaximumHeight(82);
    if (m_conn) m_notesEdit->setPlainText(QString::fromStdString(m_conn->GetNotes()));
    root->addWidget(m_notesEdit);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* cancelBtn = new QPushButton("Cancelar", this);
    auto* saveBtn = new QPushButton("Guardar cambios", this);
    ea::gui::M3Icons::decorate(saveBtn, "save", 16);
    ea::gui::setThemedStyleSheet(saveBtn, "QPushButton { background-color: @primaryStrong; color: white; font-weight: bold; padding: 7px 16px; border-radius: 12px; }");
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(saveBtn);
    root->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, this, &EditConnectorDialog::onSave);
    m_nameCombo->lineEdit()->setFocus();
    m_nameCombo->lineEdit()->selectAll();
}

void EditConnectorDialog::onSave() {
    if (!m_conn) {
        reject();
        return;
    }

    m_conn->SetName(m_nameCombo->currentText().trimmed().toStdString());
    m_conn->SetNotes(m_notesEdit->toPlainText().trimmed().toStdString());

    if (m_isMessage && m_messageSortCombo) {
        const auto sort = static_cast<ea::sequence::MessageSort>(m_messageSortCombo->currentData().toInt());
        ea::sequence::set_message_sort(*m_conn, sort);
    } else {
        if (m_typeCombo) {
            const auto newType = ea::connector_type_from_string(m_typeCombo->currentText().toStdString());
            if (newType != ea::ConnectorType::Unknown) m_conn->SetType(newType);
        }
        if (m_stereotypeCombo) m_conn->SetStereotype(m_stereotypeCombo->currentText().trimmed().toStdString());
        if (m_directionCombo) m_conn->SetDirection(m_directionCombo->currentText().toStdString());
        if (m_sourceRoleEdit) m_conn->SetClientRole(m_sourceRoleEdit->text().trimmed().toStdString());
        if (m_sourceMultCombo) m_conn->SetClientMultiplicity(m_sourceMultCombo->currentText().trimmed().toStdString());
        if (m_targetRoleEdit) m_conn->SetSupplierRole(m_targetRoleEdit->text().trimmed().toStdString());
        if (m_targetMultCombo) m_conn->SetSupplierMultiplicity(m_targetMultCombo->currentText().trimmed().toStdString());
    }

    m_repo.SaveConnector(*m_conn);
    m_modified = true;
    accept();
}
