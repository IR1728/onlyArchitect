#include "WorkspaceLayouts.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace ea::gui {

namespace {
constexpr int kUserLayoutRole = Qt::UserRole + 1;
const char* kUserLayoutsKey = "workspaces/userLayouts";
} // namespace

const std::vector<SystemWorkspaceLayout>& systemWorkspaceLayouts() {
    static const std::vector<SystemWorkspaceLayout> layouts = {
        {"Default", "Design and Construct", true},
        {"Clear", "Design and Construct", true},
        {"Browse", "Design and Construct", true},
        {"Wide View", "Design and Construct", false},   // needs auto-hide windows
        {"Explore", "Design and Construct", true},
        {"Core Modeling", "Design and Construct", true},
        {"Database Modeling", "Design and Construct", false},
        {"Gap Analysis", "Design and Construct", false},
        {"Schema Design", "Design and Construct", false},
        {"Trace by Matrix", "Design and Construct", true},
        {"Software Engineering", "Software & Simulation", true},
        {"Debug", "Software & Simulation", false},
        {"Record", "Software & Simulation", false},
        {"Profile", "Software & Simulation", false},
        {"Testpoints", "Software & Simulation", false},
        {"XSLT", "Software & Simulation", false},
        {"Simulation", "Software & Simulation", true},
        {"DMN-Sim", "Software & Simulation", false},
        {"SysML-Sim", "Software & Simulation", false},
        {"BPSim", "Software & Simulation", false},
    };
    return layouts;
}

bool isImplementedSystemLayout(const QString& name) {
    for (const auto& l : systemWorkspaceLayouts()) {
        if (l.name == name) return l.implemented;
    }
    return false;
}

// ---------------------------------------------------------------------------
// WorkspaceLayoutStore
// ---------------------------------------------------------------------------

QVariantMap WorkspaceLayoutStore::userLayouts() const {
    return m_settings.value(kUserLayoutsKey).toMap();
}

QStringList WorkspaceLayoutStore::userLayoutNames() const {
    return userLayouts().keys();
}

bool WorkspaceLayoutStore::hasUserLayout(const QString& name) const {
    return userLayouts().contains(name);
}

void WorkspaceLayoutStore::saveUserLayout(const QString& name, const QByteArray& dockState, int centralWorkspace) {
    auto layouts = userLayouts();
    layouts[name] = QVariantMap{{"state", dockState}, {"central", centralWorkspace}};
    m_settings.setValue(kUserLayoutsKey, layouts);
}

bool WorkspaceLayoutStore::loadUserLayout(const QString& name, QByteArray& dockState, int& centralWorkspace) const {
    auto layouts = userLayouts();
    if (!layouts.contains(name)) return false;
    auto entry = layouts.value(name).toMap();
    dockState = entry.value("state").toByteArray();
    centralWorkspace = entry.value("central").toInt();
    return true;
}

void WorkspaceLayoutStore::removeUserLayout(const QString& name) {
    auto layouts = userLayouts();
    layouts.remove(name);
    m_settings.setValue(kUserLayoutsKey, layouts);
}

QString WorkspaceLayoutStore::lastApplied() const {
    return m_settings.value("workspaces/lastApplied", "Default").toString();
}

void WorkspaceLayoutStore::setLastApplied(const QString& name) {
    m_settings.setValue("workspaces/lastApplied", name);
}

bool WorkspaceLayoutStore::loadOnStartup() const {
    return m_settings.value("workspaces/loadOnStartup", false).toBool();
}

void WorkspaceLayoutStore::setLoadOnStartup(bool enabled) {
    m_settings.setValue("workspaces/loadOnStartup", enabled);
}

bool WorkspaceLayoutStore::closeOnApply() const {
    return m_settings.value("workspaces/closeOnApply", true).toBool();
}

void WorkspaceLayoutStore::setCloseOnApply(bool enabled) {
    m_settings.setValue("workspaces/closeOnApply", enabled);
}

// ---------------------------------------------------------------------------
// SaveWorkspaceLayoutDialog
// ---------------------------------------------------------------------------

SaveWorkspaceLayoutDialog::SaveWorkspaceLayoutDialog(const QStringList& existingUserLayouts, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Save Custom Workspace Layout");
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    // Selecting an existing name overwrites that layout (workspace.md)
    m_nameCombo = new QComboBox(this);
    m_nameCombo->setEditable(true);
    m_nameCombo->addItems(existingUserLayouts);
    m_nameCombo->setCurrentText("");
    form->addRow("Custom Workspace Layout:", m_nameCombo);
    layout->addLayout(form);

    auto* includeViews = new QCheckBox("Include active custom views", this);
    includeViews->setEnabled(false);
    includeViews->setToolTip("onlyArchitect todavía no tiene vistas personalizadas.");
    layout->addWidget(includeViews);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    auto* saveButton = buttons->button(QDialogButtonBox::Save);
    saveButton->setEnabled(false);
    connect(m_nameCombo, &QComboBox::currentTextChanged, this, [saveButton](const QString& text) {
        saveButton->setEnabled(!text.trimmed().isEmpty());
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QString SaveWorkspaceLayoutDialog::layoutName() const {
    return m_nameCombo->currentText().trimmed();
}

// ---------------------------------------------------------------------------
// WorkspaceLayoutDialog
// ---------------------------------------------------------------------------

WorkspaceLayoutDialog::WorkspaceLayoutDialog(WorkspaceLayoutStore& store, QWidget* parent)
    : QDialog(parent), m_store(store) {
    setWindowTitle("Workspace Layout");
    resize(380, 520);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Workspaces", this));

    m_list = new QListWidget(this);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_list, 1);

    auto* form = new QFormLayout();
    m_lastAppliedEdit = new QLineEdit(this);
    m_lastAppliedEdit->setReadOnly(true);
    form->addRow("Last Applied Workspace:", m_lastAppliedEdit);
    layout->addLayout(form);

    m_closeOnApply = new QCheckBox("Close on Apply", this);
    m_closeOnApply->setChecked(m_store.closeOnApply());
    m_loadOnStartup = new QCheckBox("Load workspace on startup", this);
    m_loadOnStartup->setChecked(m_store.loadOnStartup());
    layout->addWidget(m_closeOnApply);
    layout->addWidget(m_loadOnStartup);

    auto* buttons = new QHBoxLayout();
    auto* applyButton = new QPushButton("Apply", this);
    auto* restoreButton = new QPushButton("Restore", this);
    auto* closeButton = new QPushButton("Close", this);
    buttons->addWidget(applyButton);
    buttons->addWidget(restoreButton);
    buttons->addStretch();
    buttons->addWidget(closeButton);
    layout->addLayout(buttons);

    connect(m_list, &QListWidget::itemDoubleClicked, this, &WorkspaceLayoutDialog::applySelected);
    connect(m_list, &QListWidget::customContextMenuRequested, this, &WorkspaceLayoutDialog::showContextMenu);
    connect(applyButton, &QPushButton::clicked, this, &WorkspaceLayoutDialog::applySelected);
    // Restore re-applies the last applied layout, discarding later changes
    connect(restoreButton, &QPushButton::clicked, this, [this]() { emit applyRequested(m_store.lastApplied()); });
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_closeOnApply, &QCheckBox::toggled, this, [this](bool on) { m_store.setCloseOnApply(on); });
    connect(m_loadOnStartup, &QCheckBox::toggled, this, [this](bool on) { m_store.setLoadOnStartup(on); });

    refresh();
}

void WorkspaceLayoutDialog::refresh() {
    m_list->clear();
    for (const auto& l : systemWorkspaceLayouts()) {
        auto* item = new QListWidgetItem(l.name, m_list);
        if (!l.implemented) {
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            item->setToolTip("Necesita ventanas que onlyArchitect todavía no tiene.");
        }
    }
    for (const auto& name : m_store.userLayoutNames()) {
        auto* item = new QListWidgetItem(name + "  (custom)", m_list);
        item->setData(Qt::UserRole, name);
        item->setData(kUserLayoutRole, true);
    }
    m_lastAppliedEdit->setText(m_store.lastApplied());
}

QString WorkspaceLayoutDialog::selectedLayout() const {
    auto* item = m_list->currentItem();
    if (!item || !(item->flags() & Qt::ItemIsEnabled)) return {};
    return item->data(kUserLayoutRole).toBool() ? item->data(Qt::UserRole).toString() : item->text();
}

void WorkspaceLayoutDialog::applySelected() {
    QString name = selectedLayout();
    if (name.isEmpty()) return;
    emit applyRequested(name);
    m_lastAppliedEdit->setText(m_store.lastApplied());
    if (m_closeOnApply->isChecked()) accept();
}

void WorkspaceLayoutDialog::copySelectedAsNew() {
    SaveWorkspaceLayoutDialog dlg(m_store.userLayoutNames(), this);
    if (dlg.exec() != QDialog::Accepted) return;
    emit saveCurrentLayoutRequested(dlg.layoutName());
    refresh();
}

void WorkspaceLayoutDialog::deleteSelected() {
    auto* item = m_list->currentItem();
    if (!item || !item->data(kUserLayoutRole).toBool()) return;
    QString name = item->data(Qt::UserRole).toString();
    if (QMessageBox::question(this, "Delete Workspace Layout",
                              QString("Delete the workspace layout '%1'?").arg(name)) != QMessageBox::Yes) {
        return;
    }
    m_store.removeUserLayout(name);
    refresh();
}

void WorkspaceLayoutDialog::showContextMenu(const QPoint& pos) {
    auto* item = m_list->itemAt(pos);
    if (!item || !(item->flags() & Qt::ItemIsEnabled)) return;
    m_list->setCurrentItem(item);

    QMenu menu(this);
    auto* applyAction = menu.addAction("Apply");
    auto* copyAction = menu.addAction("Copy As New");
    QAction* deleteAction = nullptr;
    if (item->data(kUserLayoutRole).toBool()) deleteAction = menu.addAction("Delete");

    auto* chosen = menu.exec(m_list->viewport()->mapToGlobal(pos));
    if (chosen == applyAction) applySelected();
    else if (chosen == copyAction) copySelectedAsNew();
    else if (chosen && chosen == deleteAction) deleteSelected();
}

} // namespace ea::gui
