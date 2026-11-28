#include "NewDiagramDialog.hpp"
#include "ea/diagram/UmlDiagramKinds.hpp"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int KindKeyRole = Qt::UserRole;
}

NewDiagramDialog::NewDiagramDialog(ea::Repository& repo, int defaultPackageId, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Nuevo diagrama");
    setMinimumSize(420, 520);

    auto layout = new QVBoxLayout(this);

    auto form = new QFormLayout();
    m_nameEdit = new QLineEdit(this);
    form->addRow("Nombre:", m_nameEdit);

    m_packageCombo = new QComboBox(this);
    for (const auto& model : repo.GetModels()) {
        if (model) populatePackages(*model, 0);
    }
    form->addRow("Paquete:", m_packageCombo);
    layout->addLayout(form);

    layout->addWidget(new QLabel("Tipo de diagrama (UML 2.x):", this));
    m_kindList = new QListWidget(this);
    ea::UmlDiagramCategory currentCategory{};
    bool first = true;
    for (const auto& kind : ea::uml_diagram_kinds()) {
        if (first || kind.category != currentCategory) {
            auto header = new QListWidgetItem(QString::fromStdString(to_string(kind.category)), m_kindList);
            header->setFlags(Qt::NoItemFlags);
            QFont f = header->font();
            f.setBold(true);
            header->setFont(f);
            currentCategory = kind.category;
            first = false;
        }
        auto item = new QListWidgetItem("    " + QString::fromStdString(kind.label), m_kindList);
        item->setData(KindKeyRole, QString::fromStdString(kind.key));
    }
    layout->addWidget(m_kindList, 1);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttons->button(QDialogButtonBox::Ok)->setText("Crear");
    m_buttons->button(QDialogButtonBox::Cancel)->setText("Cancelar");
    layout->addWidget(m_buttons);

    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_kindList, &QListWidget::currentItemChanged, this, [this]() { onKindChanged(); });
    connect(m_kindList, &QListWidget::itemDoubleClicked, this, [this]() {
        if (m_buttons->button(QDialogButtonBox::Ok)->isEnabled()) accept();
    });
    connect(m_nameEdit, &QLineEdit::textEdited, this, [this]() {
        m_nameEditedByUser = true;
        updateOkButton();
    });
    connect(m_nameEdit, &QLineEdit::textChanged, this, [this]() { updateOkButton(); });

    if (!selectPackage(defaultPackageId) && m_packageCombo->count() > 0) m_packageCombo->setCurrentIndex(0);
    selectKind("class");
    m_nameEdit->setFocus();
    m_nameEdit->selectAll();
    updateOkButton();
}

void NewDiagramDialog::populatePackages(const ea::Package& pkg, int depth) {
    QString indent(depth * 4, QChar(' '));
    m_packageCombo->addItem(indent + QString::fromStdString(pkg.GetName()), pkg.GetPackageID());
    for (const auto& sub : pkg.GetPackages()) {
        if (sub) populatePackages(*sub, depth + 1);
    }
}

void NewDiagramDialog::onKindChanged() {
    // Suggest a name from the kind until the user types their own
    if (!m_nameEditedByUser) {
        if (const auto* kind = ea::find_uml_diagram_kind(kindKey().toStdString())) {
            m_nameEdit->setText(QString("Diagrama de %1").arg(QString::fromStdString(kind->label)));
            m_nameEdit->selectAll();
        }
    }
    updateOkButton();
}

void NewDiagramDialog::updateOkButton() {
    const bool ok = !diagramName().isEmpty() && !kindKey().isEmpty() && packageId() > 0;
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(ok);
}

QString NewDiagramDialog::diagramName() const {
    return m_nameEdit->text().trimmed();
}

int NewDiagramDialog::packageId() const {
    return m_packageCombo->currentIndex() >= 0 ? m_packageCombo->currentData().toInt() : 0;
}

QString NewDiagramDialog::kindKey() const {
    auto* item = m_kindList->currentItem();
    return item ? item->data(KindKeyRole).toString() : QString();
}

void NewDiagramDialog::setDiagramName(const QString& name) {
    m_nameEditedByUser = true;
    m_nameEdit->setText(name);
}

bool NewDiagramDialog::selectKind(const QString& key) {
    for (int i = 0; i < m_kindList->count(); ++i) {
        auto* item = m_kindList->item(i);
        if (item->data(KindKeyRole).toString() == key) {
            m_kindList->setCurrentItem(item);
            return true;
        }
    }
    return false;
}

bool NewDiagramDialog::selectPackage(int packageId) {
    const int index = m_packageCombo->findData(packageId);
    if (index < 0) return false;
    m_packageCombo->setCurrentIndex(index);
    return true;
}
