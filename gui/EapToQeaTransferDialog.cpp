#include "EapToQeaTransferDialog.hpp"
#include "ea/repository/EapxImporter.hpp"
#include <QFileInfo>
#include <QApplication>

EapToQeaTransferDialog::EapToQeaTransferDialog(const QString& initialSourcePath, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Transfer Project: EAP / EAPX to QEA");
    resize(580, 260);

    auto mainLayout = new QVBoxLayout(this);

    auto headerLabel = new QLabel(
        "<b>Transfer Legacy Project (.EAP / .EAPX) to Modern SQLite (.QEA)</b><br>"
        "<span style='color: #666; font-size: 11px;'>"
        "Enterprise Architect 16+ uses SQLite (.qea). This tool migrates all packages, "
        "elements, diagrams, and connectors into a high-performance native repository."
        "</span>", this);
    headerLabel->setWordWrap(true);
    mainLayout->addWidget(headerLabel);
    mainLayout->addSpacing(10);

    // Source Row
    auto sourceLayout = new QHBoxLayout();
    sourceLayout->addWidget(new QLabel("Source File (.eapx / .eap):", this));
    m_sourceEdit = new QLineEdit(initialSourcePath, this);
    auto sourceBrowseBtn = new QPushButton("Browse...", this);
    sourceLayout->addWidget(m_sourceEdit, 1);
    sourceLayout->addWidget(sourceBrowseBtn);
    mainLayout->addLayout(sourceLayout);

    // Target Row
    auto targetLayout = new QHBoxLayout();
    targetLayout->addWidget(new QLabel("Target File (.qea):", this));
    m_targetEdit = new QLineEdit(this);
    auto targetBrowseBtn = new QPushButton("Browse...", this);
    targetLayout->addWidget(m_targetEdit, 1);
    targetLayout->addWidget(targetBrowseBtn);
    mainLayout->addLayout(targetLayout);

    if (!initialSourcePath.isEmpty()) {
        QFileInfo fi(initialSourcePath);
        QString suggested = fi.absolutePath() + "/" + fi.completeBaseName() + ".qea";
        m_targetEdit->setText(suggested);
    }

    // Checkbox
    m_openImmediatelyCheck = new QCheckBox("Open transferred repository immediately after completion", this);
    m_openImmediatelyCheck->setChecked(true);
    mainLayout->addWidget(m_openImmediatelyCheck);

    // Progress Bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    mainLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel("Ready.", this);
    m_statusLabel->setStyleSheet("color: #555;");
    mainLayout->addWidget(m_statusLabel);

    // Buttons
    auto btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    m_transferBtn = new QPushButton("⚡ Transfer", this);
    m_transferBtn->setStyleSheet("QPushButton { font-weight: bold; padding: 6px 16px; background-color: #0078d4; color: white; border-radius: 4px; }"
                                 "QPushButton:hover { background-color: #106ebe; }");
    auto closeBtn = new QPushButton("Close", this);
    btnLayout->addWidget(m_transferBtn);
    btnLayout->addWidget(closeBtn);
    mainLayout->addLayout(btnLayout);

    connect(sourceBrowseBtn, &QPushButton::clicked, this, &EapToQeaTransferDialog::onBrowseSource);
    connect(targetBrowseBtn, &QPushButton::clicked, this, &EapToQeaTransferDialog::onBrowseTarget);
    connect(m_transferBtn, &QPushButton::clicked, this, &EapToQeaTransferDialog::onTransfer);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void EapToQeaTransferDialog::onBrowseSource() {
    QString path = QFileDialog::getOpenFileName(this, "Select Source EAP / EAPX Project",
                                                m_sourceEdit->text(),
                                                "Enterprise Architect Legacy Projects (*.eapx *.eap);;All Files (*)");
    if (!path.isEmpty()) {
        m_sourceEdit->setText(path);
        QFileInfo fi(path);
        QString suggested = fi.absolutePath() + "/" + fi.completeBaseName() + ".qea";
        m_targetEdit->setText(suggested);
    }
}

void EapToQeaTransferDialog::onBrowseTarget() {
    QString path = QFileDialog::getSaveFileName(this, "Select Target QEA Project",
                                                m_targetEdit->text(),
                                                "Enterprise Architect SQLite Repository (*.qea)");
    if (!path.isEmpty()) {
        m_targetEdit->setText(path);
    }
}

void EapToQeaTransferDialog::onTransfer() {
    QString source = m_sourceEdit->text().trimmed();
    QString target = m_targetEdit->text().trimmed();

    if (source.isEmpty() || !QFile::exists(source)) {
        QMessageBox::warning(this, "Invalid Source", "Please select a valid source .eap or .eapx file.");
        return;
    }

    if (target.isEmpty()) {
        QMessageBox::warning(this, "Invalid Target", "Please specify a target .qea file path.");
        return;
    }

    m_transferBtn->setEnabled(false);
    m_progressBar->setRange(0, 0); // Indeterminate
    m_statusLabel->setText("Migrating schema, tables, and records from MS Access Jet to SQLite...");
    qApp->processEvents();

    std::string result = ea::EapxImporter::ConvertToSqlite(source.toStdString(), target.toStdString());

    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(100);
    m_transferBtn->setEnabled(true);

    if (!result.empty()) {
        m_targetPath = QString::fromStdString(result);
        m_statusLabel->setText("✅ Project transfer completed successfully!");
        QMessageBox::information(this, "Transfer Complete",
            QString("Successfully transferred project to:\n%1").arg(m_targetPath));
        accept();
    } else {
        m_statusLabel->setText("❌ Failed to transfer project.");
        QMessageBox::critical(this, "Transfer Failed",
            "Could not transfer project. Ensure mdbtools utilities are accessible.");
    }
}
