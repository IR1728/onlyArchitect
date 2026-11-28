#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>

class EapToQeaTransferDialog : public QDialog {
    Q_OBJECT
public:
    explicit EapToQeaTransferDialog(const QString& initialSourcePath = "", QWidget* parent = nullptr);

    QString getTargetQeaPath() const { return m_targetPath; }
    bool shouldOpenImmediately() const { return m_openImmediatelyCheck->isChecked(); }

private slots:
    void onBrowseSource();
    void onBrowseTarget();
    void onTransfer();

private:
    QLineEdit* m_sourceEdit{nullptr};
    QLineEdit* m_targetEdit{nullptr};
    QProgressBar* m_progressBar{nullptr};
    QCheckBox* m_openImmediatelyCheck{nullptr};
    QPushButton* m_transferBtn{nullptr};
    QLabel* m_statusLabel{nullptr};

    QString m_targetPath;
};
