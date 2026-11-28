#include "PropertiesWidget.hpp"
#include "DarkTheme.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include "ea/editor/FeatureText.hpp"

namespace fx = ea::editor;

namespace {

const QStringList kStatuses = {"Proposed", "Approved", "Implemented", "Mandatory", "Validated"};
constexpr const char* kSectionStyle = "color: @primary; font-weight: bold; font-size: 11px; margin-top: 4px;";

QString q(const std::string& s) { return QString::fromStdString(s); }

QTableWidgetItem* textItem(const QString& text) { return new QTableWidgetItem(text); }

QTableWidgetItem* checkItem(bool checked) {
    auto* item = new QTableWidgetItem();
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    return item;
}

QTableWidget* featureTable(const QStringList& headers, QWidget* parent) {
    auto* table = new QTableWidget(parent);
    table->setColumnCount(static_cast<int>(headers.size()));
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                           QAbstractItemView::AnyKeyPressed);
    return table;
}

QToolButton* toolButton(const QString& text, const QString& tip, const QString& name, QWidget* parent) {
    auto* b = new QToolButton(parent);
    b->setText(text);
    b->setToolTip(tip);
    b->setObjectName(name);
    return b;
}

// New name not used yet among `existing` (attribute1, attribute2, ...)
template <typename Collection>
std::string unique_name(const Collection& existing, const std::string& base) {
    for (int n = 1;; ++n) {
        const std::string candidate = base + std::to_string(n);
        bool used = false;
        for (const auto& f : existing) used = used || (f && f->GetName() == candidate);
        if (!used) return candidate;
    }
}

// Moves item `from` to `to` in a collection (order is what the diagram and code generation show)
template <typename Collection>
void move_item(Collection& c, int from, int to) {
    std::vector<typename Collection::ItemPtr> items(c.begin(), c.end());
    if (from < 0 || to < 0 || from >= static_cast<int>(items.size()) || to >= static_cast<int>(items.size())) return;
    auto moved = items[static_cast<size_t>(from)];
    items.erase(items.begin() + from);
    items.insert(items.begin() + to, moved);
    c.Clear();
    for (auto& i : items) c.AddExisting(i);
}

// Commits a QPlainTextEdit when it loses focus (it has no editingFinished)
class FocusOutCommit : public QObject {
public:
    FocusOutCommit(QObject* parent, std::function<void()> commit) : QObject(parent), m_commit(std::move(commit)) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::FocusOut) m_commit();
        return QObject::eventFilter(watched, event);
    }

private:
    std::function<void()> m_commit;
};

} // namespace

PropertiesWidget::PropertiesWidget(ea::Repository& repo, QWidget* parent) : QWidget(parent), m_repo(repo) {
    setupUi();
    setEditable(false);
}

// ============================================================================
// Layout
// ============================================================================

void PropertiesWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    constexpr int Margin = ea::gui::M3Spacing::Small;
    mainLayout->setContentsMargins(Margin, Margin, Margin, Margin);
    mainLayout->setSpacing(ea::gui::M3Spacing::Small);
    setMinimumSize(180, 140);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setObjectName("propError");
    m_errorLabel->setWordWrap(true);
    ea::gui::setThemedStyleSheet(m_errorLabel, "color: @error; font-size: 11px;");
    m_errorLabel->hide();

    m_tabs = new QTabWidget(this);
    m_tabs->setUsesScrollButtons(true);
    m_tabs->setElideMode(Qt::ElideNone);
    m_tabs->setMinimumSize(160, 120);
    ea::gui::setThemedStyleSheet(m_tabs,
        "QTabBar::tab { padding: 6px 10px; font-size: 11px; }"
    );
    mainLayout->addWidget(m_tabs);
    mainLayout->addWidget(m_errorLabel);

    m_tabs->addTab(buildGeneralTab(), "Element");
    m_tabs->addTab(new QWidget(this), "Tags");
    m_tabs->addTab(buildAttributesTab(), "Attributes");
    m_tabs->addTab(buildOperationsTab(), "Operations");

    m_connectorsTable = featureTable({"Type", "Target", "Direction"}, this);
    m_connectorsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_connectorsTable->horizontalHeader()->setStretchLastSection(true);
    m_tabs->addTab(m_connectorsTable, "Connectors");
}

QWidget* PropertiesWidget::buildGeneralTab() {
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);
    form->setLabelAlignment(Qt::AlignRight);
    form->setContentsMargins(ea::gui::M3Spacing::Medium, ea::gui::M3Spacing::Medium, ea::gui::M3Spacing::Medium, ea::gui::M3Spacing::Medium);
    form->setVerticalSpacing(ea::gui::M3Spacing::Medium);  // 12 px between rows
    form->setHorizontalSpacing(ea::gui::M3Spacing::Large); // 16 px label <-> field

    auto section = [&](const QString& title) {
        auto* l = new QLabel("▼ " + title, page);
        ea::gui::setThemedStyleSheet(l, kSectionStyle);
        form->addRow(l);
    };
    auto line = [&](const QString& label, const QString& name, bool readOnly = false) {
        auto* e = new QLineEdit(page);
        e->setObjectName(name);
        e->setReadOnly(readOnly);
        form->addRow(label, e);
        return e;
    };

    section("General");
    m_nameEdit = line("Name:", "propName");
    m_typeEdit = line("Type:", "propType", true);
    m_stereotypeEdit = line("Stereotype:", "propStereotype");
    m_aliasEdit = line("Alias:", "propAlias");
    m_statusCombo = new QComboBox(page);
    m_statusCombo->setObjectName("propStatus");
    m_statusCombo->setEditable(true);
    m_statusCombo->addItems(kStatuses);
    form->addRow("Status:", m_statusCombo);
    m_versionEdit = line("Version:", "propVersion");
    m_abstractCheck = new QCheckBox("Abstract", page);
    m_abstractCheck->setObjectName("propAbstract");
    form->addRow("", m_abstractCheck);

    section("Project");
    m_authorEdit = line("Author:", "propAuthor");
    m_guidEdit = line("GUID:", "propGuid", true);

    section("Notes");
    m_notesEdit = new QPlainTextEdit(page);
    m_notesEdit->setObjectName("propNotes");
    m_notesEdit->setMaximumHeight(90);
    form->addRow(m_notesEdit);

    connect(m_nameEdit, &QLineEdit::editingFinished, this, &PropertiesWidget::commitName);
    connect(m_stereotypeEdit, &QLineEdit::editingFinished, this,
            [this] { commitText(m_stereotypeEdit, "estereotipo", &ea::Element::SetStereotype); });
    connect(m_aliasEdit, &QLineEdit::editingFinished, this,
            [this] { commitText(m_aliasEdit, "alias", &ea::Element::SetAlias); });
    connect(m_versionEdit, &QLineEdit::editingFinished, this,
            [this] { commitText(m_versionEdit, "versión", &ea::Element::SetVersion); });
    connect(m_authorEdit, &QLineEdit::editingFinished, this,
            [this] { commitText(m_authorEdit, "autor", &ea::Element::SetAuthor); });
    connect(m_statusCombo, &QComboBox::currentTextChanged, this, [this](const QString& status) {
        const auto value = status.trimmed().toStdString();
        if (value.empty()) return;
        requestEdit("Cambiar estado", [value](ea::Element& e) { e.SetStatus(value); });
    });
    connect(m_abstractCheck, &QCheckBox::toggled, this, [this](bool abstract) {
        requestEdit(abstract ? "Marcar abstracto" : "Quitar abstracto", [abstract](ea::Element& e) { e.SetAbstract(abstract); });
    });
    m_notesEdit->installEventFilter(new FocusOutCommit(this, [this] { commitNotes(); }));

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMinimumSize(160, 120);
    scroll->setWidget(page);
    return scroll;
}

QWidget* PropertiesWidget::buildAttributesTab() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    m_attributeButtons = new QWidget(page);
    auto* bar = new QHBoxLayout(m_attributeButtons);
    bar->setContentsMargins(2, 2, 2, 2);
    auto* add = toolButton("＋", "Añadir atributo", "addAttribute", m_attributeButtons);
    auto* del = toolButton("－", "Eliminar atributo", "removeAttribute", m_attributeButtons);
    auto* up = toolButton("▲", "Subir", "attributeUp", m_attributeButtons);
    auto* down = toolButton("▼", "Bajar", "attributeDown", m_attributeButtons);
    for (auto* b : {add, del, up, down}) bar->addWidget(b);
    bar->addStretch();
    layout->addWidget(m_attributeButtons);

    m_attributesTable = featureTable({"Vis", "Name", "Type", "Mult.", "Initial", "Static"}, page);
    m_attributesTable->setObjectName("attributesTable");
    m_attributesTable->setToolTip("Vis: + público, - privado, # protegido, ~ paquete · Mult.: 1, 0..1, *, 1..*");
    layout->addWidget(m_attributesTable);

    connect(add, &QToolButton::clicked, this, &PropertiesWidget::addAttribute);
    connect(del, &QToolButton::clicked, this, [this] { removeSelectedFeature(m_attributesTable, true); });
    connect(up, &QToolButton::clicked, this, [this] { moveSelectedFeature(m_attributesTable, true, -1); });
    connect(down, &QToolButton::clicked, this, [this] { moveSelectedFeature(m_attributesTable, true, +1); });
    connect(m_attributesTable, &QTableWidget::itemChanged, this, &PropertiesWidget::onAttributeChanged);
    return page;
}

QWidget* PropertiesWidget::buildOperationsTab() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    m_operationButtons = new QWidget(page);
    auto* bar = new QHBoxLayout(m_operationButtons);
    bar->setContentsMargins(2, 2, 2, 2);
    auto* add = toolButton("＋", "Añadir operación", "addOperation", m_operationButtons);
    auto* del = toolButton("－", "Eliminar operación", "removeOperation", m_operationButtons);
    auto* up = toolButton("▲", "Subir", "operationUp", m_operationButtons);
    auto* down = toolButton("▼", "Bajar", "operationDown", m_operationButtons);
    for (auto* b : {add, del, up, down}) bar->addWidget(b);
    bar->addStretch();
    layout->addWidget(m_operationButtons);

    m_operationsTable = featureTable({"Vis", "Name", "Parameters", "Return", "Static", "Abstract"}, page);
    m_operationsTable->setObjectName("operationsTable");
    m_operationsTable->setToolTip("Parámetros: nombre: Tipo, out otro: Tipo = valor");
    layout->addWidget(m_operationsTable);

    connect(add, &QToolButton::clicked, this, &PropertiesWidget::addOperation);
    connect(del, &QToolButton::clicked, this, [this] { removeSelectedFeature(m_operationsTable, false); });
    connect(up, &QToolButton::clicked, this, [this] { moveSelectedFeature(m_operationsTable, false, -1); });
    connect(down, &QToolButton::clicked, this, [this] { moveSelectedFeature(m_operationsTable, false, +1); });
    connect(m_operationsTable, &QTableWidget::itemChanged, this, &PropertiesWidget::onOperationChanged);
    return page;
}

void PropertiesWidget::setEditable(bool editable) {
    for (auto* e : {m_nameEdit, m_stereotypeEdit, m_aliasEdit, m_versionEdit, m_authorEdit}) e->setReadOnly(!editable);
    m_notesEdit->setReadOnly(!editable);
    m_statusCombo->setEnabled(editable);
    m_abstractCheck->setEnabled(editable);
    m_attributeButtons->setEnabled(editable);
    m_operationButtons->setEnabled(editable);
    const auto triggers = editable ? (QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                                      QAbstractItemView::AnyKeyPressed)
                                   : QAbstractItemView::NoEditTriggers;
    m_attributesTable->setEditTriggers(triggers);
    m_operationsTable->setEditTriggers(triggers);
}

// ============================================================================
// Showing
// ============================================================================

void PropertiesWidget::clear() {
    const QSignalBlocker b1(m_statusCombo), b2(m_abstractCheck), b3(m_attributesTable), b4(m_operationsTable);
    m_elementId = 0;
    for (auto* e : {m_nameEdit, m_typeEdit, m_stereotypeEdit, m_aliasEdit, m_versionEdit, m_authorEdit, m_guidEdit}) e->clear();
    m_statusCombo->setCurrentIndex(-1);
    m_abstractCheck->setChecked(false);
    m_notesEdit->clear();
    m_attributesTable->setRowCount(0);
    m_operationsTable->setRowCount(0);
    m_connectorsTable->setRowCount(0);
    m_errorLabel->hide();
    setEditable(false);
}

void PropertiesWidget::showElement(int elementId) {
    auto elem = m_repo.GetElementByID(elementId);
    if (!elem) {
        clear();
        return;
    }
    if (elementId != m_elementId) m_errorLabel->hide(); // keep the message while refreshing the same element
    m_elementId = elementId;
    fillGeneral(*elem);
    fillAttributes(*elem);
    fillOperations(*elem);

    m_connectorsTable->setRowCount(static_cast<int>(elem->GetConnectors().size()));
    int row = 0;
    for (const auto& c : elem->GetConnectors()) {
        if (!c) continue;
        const int otherId = c->GetClientID() == elementId ? c->GetSupplierID() : c->GetClientID();
        auto other = m_repo.GetElementByID(otherId);
        m_connectorsTable->setItem(row, 0, textItem(q(to_string(c->GetType()))));
        m_connectorsTable->setItem(row, 1, textItem(other ? q(other->GetName()) : QString("ID: %1").arg(otherId)));
        m_connectorsTable->setItem(row, 2, textItem(q(c->GetDirection())));
        ++row;
    }
    setEditable(true);
}

void PropertiesWidget::showDiagram(int diagramId) {
    clear();
    auto diag = m_repo.GetDiagramByID(diagramId);
    if (!diag) return;
    m_guidEdit->setText(q(diag->GetDiagramGUID()));
    m_nameEdit->setText(q(diag->GetName()));
    m_typeEdit->setText(q(to_string(diag->GetType())));
    m_stereotypeEdit->setText(q(diag->GetStereotype()));
    m_authorEdit->setText(q(diag->GetAuthor()));
    m_versionEdit->setText(q(diag->GetVersion()));
    m_notesEdit->setPlainText(q(diag->GetNotes()));
}

void PropertiesWidget::fillGeneral(const ea::Element& elem) {
    const QSignalBlocker b1(m_statusCombo), b2(m_abstractCheck);
    // Text the user is typing into is left alone; it is committed when they leave the field
    auto set = [](QLineEdit* e, const std::string& v) { if (!e->hasFocus() || !e->isModified()) e->setText(q(v)); };
    set(m_nameEdit, elem.GetName());
    m_typeEdit->setText(q(to_string(elem.GetType())));
    set(m_stereotypeEdit, elem.GetStereotype());
    set(m_aliasEdit, elem.GetAlias());
    set(m_versionEdit, elem.GetVersion());
    set(m_authorEdit, elem.GetAuthor());
    m_guidEdit->setText(q(elem.GetElementGUID()));
    m_statusCombo->setCurrentText(q(elem.GetStatus()));
    m_abstractCheck->setChecked(elem.IsAbstract());
    if (!m_notesEdit->hasFocus()) m_notesEdit->setPlainText(q(elem.GetNotes()));
    for (auto* e : {m_nameEdit, m_stereotypeEdit, m_aliasEdit, m_versionEdit, m_authorEdit}) e->setModified(false);
}

void PropertiesWidget::fillAttributes(const ea::Element& elem) {
    const QSignalBlocker block(m_attributesTable);
    const int current = m_attributesTable->currentRow();
    m_attributesTable->setRowCount(static_cast<int>(elem.GetAttributes().size()));
    int row = 0;
    for (const auto& a : elem.GetAttributes()) {
        if (!a) continue;
        m_attributesTable->setItem(row, AttrVisibility, textItem(q(fx::visibility_symbol(a->GetVisibility()))));
        m_attributesTable->setItem(row, AttrName, textItem(q(a->GetName())));
        m_attributesTable->setItem(row, AttrType, textItem(q(a->GetType())));
        m_attributesTable->setItem(row, AttrMultiplicity,
                                   textItem(q(fx::format_multiplicity({a->GetLowerBound(), a->GetUpperBound()}))));
        m_attributesTable->setItem(row, AttrDefault, textItem(q(a->GetDefault())));
        m_attributesTable->setItem(row, AttrStatic, checkItem(a->IsStatic()));
        ++row;
    }
    if (current >= 0 && current < row) m_attributesTable->selectRow(current);
}

void PropertiesWidget::fillOperations(const ea::Element& elem) {
    const QSignalBlocker block(m_operationsTable);
    const int current = m_operationsTable->currentRow();
    m_operationsTable->setRowCount(static_cast<int>(elem.GetMethods().size()));
    int row = 0;
    for (const auto& m : elem.GetMethods()) {
        if (!m) continue;
        m_operationsTable->setItem(row, OpVisibility, textItem(q(fx::visibility_symbol(m->GetVisibility()))));
        m_operationsTable->setItem(row, OpName, textItem(q(m->GetName())));
        m_operationsTable->setItem(row, OpParameters, textItem(q(fx::format_parameters(m->GetParameters()))));
        m_operationsTable->setItem(row, OpReturn, textItem(q(m->GetReturnType())));
        m_operationsTable->setItem(row, OpStatic, checkItem(m->IsStatic()));
        m_operationsTable->setItem(row, OpAbstract, checkItem(m->IsAbstract()));
        ++row;
    }
    if (current >= 0 && current < row) m_operationsTable->selectRow(current);
}

void PropertiesWidget::showTab(const QString& title) {
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (m_tabs->tabText(i) == title) {
            m_tabs->setCurrentIndex(i);
            return;
        }
    }
}

QString PropertiesWidget::lastError() const {
    return m_errorLabel->text();
}

// ============================================================================
// Committing
// ============================================================================

void PropertiesWidget::requestEdit(const QString& description, Mutation mutation) {
    if (m_elementId == 0) return;
    m_errorLabel->clear();
    m_errorLabel->hide();
    // Deferred: the edit refreshes this form, which must not happen inside the signal of the very
    // cell or field being committed (QTableWidget deletes items on refill)
    const int id = m_elementId;
    QTimer::singleShot(0, this, [this, id, description, mutation = std::move(mutation)] {
        emit editRequested(id, description, mutation);
    });
}

void PropertiesWidget::reject(const QString& message) {
    m_errorLabel->setText(message);
    m_errorLabel->show();
    const int id = m_elementId;
    QTimer::singleShot(0, this, [this, id] {
        if (m_elementId == id) showElement(id); // back to the model's values
    });
}

void PropertiesWidget::commitText(QLineEdit* edit, const QString& property, void (ea::Element::*setter)(std::string)) {
    if (!edit->isModified() || m_elementId == 0) return;
    edit->setModified(false);
    const std::string value = edit->text().trimmed().toStdString();
    requestEdit(QString("Cambiar %1").arg(property), [setter, value](ea::Element& e) { (e.*setter)(value); });
}

void PropertiesWidget::commitName() {
    if (!m_nameEdit->isModified() || m_elementId == 0) return;
    m_nameEdit->setModified(false);
    const QString name = m_nameEdit->text().trimmed();
    if (!fx::is_valid_feature_name(name.toStdString())) {
        reject("El nombre no puede quedar vacío.");
        return;
    }
    const std::string value = name.toStdString();
    requestEdit(QString("Renombrar a \"%1\"").arg(name), [value](ea::Element& e) { e.SetName(value); });
}

void PropertiesWidget::commitNotes() {
    if (m_elementId == 0 || m_notesEdit->isReadOnly()) return;
    auto elem = m_repo.GetElementByID(m_elementId);
    const std::string value = m_notesEdit->toPlainText().toStdString();
    if (!elem || elem->GetNotes() == value) return;
    requestEdit("Cambiar notas", [value](ea::Element& e) { e.SetNotes(value); });
}

void PropertiesWidget::onAttributeChanged(QTableWidgetItem* item) {
    if (!item || m_elementId == 0) return;
    const int row = item->row();
    const std::string text = item->text().trimmed().toStdString();
    auto at = [row](ea::Element& e) { return e.GetAttributes().GetAt(static_cast<short>(row)); };

    switch (item->column()) {
        case AttrVisibility: {
            const auto scope = fx::parse_visibility(text);
            if (!scope) return reject("Visibilidad no válida: usa +, -, # o ~.");
            return requestEdit("Cambiar visibilidad", [at, s = *scope](ea::Element& e) { if (auto a = at(e)) a->SetVisibility(s); });
        }
        case AttrName:
            if (!fx::is_valid_feature_name(text)) return reject("El atributo necesita un nombre.");
            return requestEdit(QString("Renombrar atributo a %1").arg(q(text)),
                               [at, text](ea::Element& e) { if (auto a = at(e)) a->SetName(text); });
        case AttrType:
            return requestEdit("Cambiar tipo de atributo", [at, text](ea::Element& e) { if (auto a = at(e)) a->SetType(text); });
        case AttrMultiplicity: {
            const auto m = fx::parse_multiplicity(text);
            if (!m) return reject("Multiplicidad no válida: usa 1, 0..1, *, 1..* o n..m con n ≤ m.");
            return requestEdit("Cambiar multiplicidad", [at, m = *m](ea::Element& e) {
                if (auto a = at(e)) { a->SetLowerBound(m.lower); a->SetUpperBound(m.upper); }
            });
        }
        case AttrDefault:
            return requestEdit("Cambiar valor inicial", [at, text](ea::Element& e) { if (auto a = at(e)) a->SetDefault(text); });
        case AttrStatic: {
            const bool on = item->checkState() == Qt::Checked;
            return requestEdit(on ? "Atributo estático" : "Atributo de instancia",
                               [at, on](ea::Element& e) { if (auto a = at(e)) a->SetStatic(on); });
        }
        default: return;
    }
}

void PropertiesWidget::onOperationChanged(QTableWidgetItem* item) {
    if (!item || m_elementId == 0) return;
    const int row = item->row();
    const std::string text = item->text().trimmed().toStdString();
    auto at = [row](ea::Element& e) { return e.GetMethods().GetAt(static_cast<short>(row)); };

    switch (item->column()) {
        case OpVisibility: {
            const auto scope = fx::parse_visibility(text);
            if (!scope) return reject("Visibilidad no válida: usa +, -, # o ~.");
            return requestEdit("Cambiar visibilidad", [at, s = *scope](ea::Element& e) { if (auto m = at(e)) m->SetVisibility(s); });
        }
        case OpName:
            if (!fx::is_valid_feature_name(text)) return reject("La operación necesita un nombre.");
            return requestEdit(QString("Renombrar operación a %1").arg(q(text)),
                               [at, text](ea::Element& e) { if (auto m = at(e)) m->SetName(text); });
        case OpParameters: {
            auto parsed = fx::parse_parameters(text);
            if (!parsed.ok()) return reject(q(parsed.error));
            return requestEdit("Cambiar parámetros", [at, params = std::move(parsed.parameters)](ea::Element& e) {
                if (auto m = at(e)) m->SetParameters(params);
            });
        }
        case OpReturn:
            return requestEdit("Cambiar tipo de retorno",
                               [at, text](ea::Element& e) { if (auto m = at(e)) m->SetReturnType(text.empty() ? "void" : text); });
        case OpStatic: {
            const bool on = item->checkState() == Qt::Checked;
            return requestEdit(on ? "Operación estática" : "Operación de instancia",
                               [at, on](ea::Element& e) { if (auto m = at(e)) m->SetStatic(on); });
        }
        case OpAbstract: {
            const bool on = item->checkState() == Qt::Checked;
            return requestEdit(on ? "Operación abstracta" : "Operación concreta",
                               [at, on](ea::Element& e) { if (auto m = at(e)) m->SetAbstract(on); });
        }
        default: return;
    }
}

void PropertiesWidget::addAttribute() {
    requestEdit("Añadir atributo", [](ea::Element& e) {
        auto a = e.GetAttributes().AddNew(unique_name(e.GetAttributes(), "attribute"), "");
        a->SetType("int");
        a->SetVisibility(ea::Scope::Private); // UML/EA convention: attributes are private by default
    });
}

void PropertiesWidget::addOperation() {
    requestEdit("Añadir operación", [](ea::Element& e) {
        auto m = e.GetMethods().AddNew(unique_name(e.GetMethods(), "operation"), "");
        m->SetReturnType("void");
        m->SetVisibility(ea::Scope::Public);
    });
}

void PropertiesWidget::removeSelectedFeature(QTableWidget* table, bool attributes) {
    const int row = table->currentRow();
    if (row < 0) return;
    const QString name = table->item(row, attributes ? static_cast<int>(AttrName) : static_cast<int>(OpName))->text();
    requestEdit(QString("Eliminar %1 %2").arg(attributes ? "atributo" : "operación", name), [row, attributes](ea::Element& e) {
        if (attributes) e.GetAttributes().Delete(static_cast<short>(row));
        else e.GetMethods().Delete(static_cast<short>(row));
    });
}

void PropertiesWidget::moveSelectedFeature(QTableWidget* table, bool attributes, int delta) {
    const int row = table->currentRow();
    const int to = row + delta;
    if (row < 0 || to < 0 || to >= table->rowCount()) return;
    table->selectRow(to); // the selection follows the moved feature after the refresh
    requestEdit(attributes ? "Reordenar atributos" : "Reordenar operaciones", [row, to, attributes](ea::Element& e) {
        if (attributes) move_item(e.GetAttributes(), row, to);
        else move_item(e.GetMethods(), row, to);
    });
}
