#include "EapToQeaTransferDialog.hpp"
#include "DarkTheme.hpp"
#include "M3Icons.hpp"
#include "ea/repository/EapxImporter.hpp"
#include <QFileInfo>
#include <QApplication>
#include <QMetaObject>
#include <thread>

EapToQeaTransferDialog::EapToQeaTransferDialog(const QString& initialSourcePath, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Transfer Project: EAP / EAPX to QEA");
    resize(580, 260);

    auto mainLayout = new QVBoxLayout(this);

    auto headerLabel = new QLabel(
        "<b>Transfer Legacy Project (.EAP / .EAPX) to Modern SQLite (.QEA)</b><br>"
        "<span style='color: #666; font-size: 11px;'>"
        "This tool migrates legacy projects (.eapx/.eap) into SQLite (.qea), transporting all packages, "
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
    ea::gui::setThemedStyleSheet(m_statusLabel, "color: #555;");
    mainLayout->addWidget(m_statusLabel);

    // Buttons
    auto btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    m_transferBtn = new QPushButton("Transfer", this);
    ea::gui::M3Icons::decorate(m_transferBtn, "sync_alt");
    ea::gui::setThemedStyleSheet(m_transferBtn, "QPushButton { font-weight: bold; padding: 6px 16px; background-color: @primaryStrong; color: white; border-radius: 4px; }"
                                 "QPushButton:hover { background-color: @primaryStrong; }");
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
                                                "Legacy Projects (*.eapx *.eap);;All Files (*)");
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
                                                "SQLite Repository (*.qea)");
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
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(5);
    m_progressBar->setTextVisible(true);
    m_statusLabel->setText("Detectando versión y migrando tablas hacia SQLite (.qea)...");

    std::string srcStr = source.toStdString();
    std::string dstStr = target.toStdString();

    std::thread([this, srcStr = std::move(srcStr), dstStr = std::move(dstStr)]() {
        auto res = ea::EapxImporter::ConvertToSqliteDetailed(
            srcStr, dstStr,
            [this](int current, int total, const std::string& msg) {
                QString qmsg = QString::fromStdString(msg);
                QMetaObject::invokeMethod(this, [this, current, total, qmsg]() {
                    m_progressBar->setRange(0, total);
                    m_progressBar->setValue(current);
                    m_statusLabel->setText(qmsg);
                }, Qt::QueuedConnection);
            });

        QMetaObject::invokeMethod(this, [this, res]() {
            m_progressBar->setRange(0, 100);
            m_progressBar->setValue(res.ok ? 100 : 0);
            m_transferBtn->setEnabled(true);

            if (res.ok) {
                m_targetPath = QString::fromStdString(res.outputPath);
                m_statusLabel->setText("Migración completada correctamente.");
                QString summary = QString("Proyecto migrado exitosamente a:\n%1\n\n"
                                          "Formato detectado: %2\n"
                                          "Tablas con datos: %3 (vacías omitidas: %4)\n"
                                          "Registros insertados: %5")
                                      .arg(m_targetPath,
                                           QString::fromStdString(res.detectedFormat))
                                      .arg(res.tablesProcessed)
                                      .arg(res.tablesSkippedEmpty)
                                      .arg(res.rowsInserted);
                if (res.failedStatements > 0) {
                    summary += QString("\nAdvertencias SQL toleradas: %1").arg(res.failedStatements);
                }
                QMessageBox::information(this, "Transferencia Completada", summary);
                accept();
            } else {
                m_statusLabel->setText("Error al transferir el proyecto.");
                QMessageBox::critical(this, "Error en Transferencia",
                                      QString::fromStdString(res.error.empty()
                                                                 ? "No se pudo convertir el proyecto."
                                                                 : res.error));
            }
        }, Qt::QueuedConnection);
    }).detach();
}
