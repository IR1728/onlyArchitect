#pragma once

#include <QDialog>
#include <QString>
#include "ea/repository/Repository.hpp"

class QLineEdit;
class QComboBox;
class QListWidget;
class QDialogButtonBox;

// "Diagrama > Nuevo Diagrama": choose the UML 2.x diagram kind, its name and its owning package.
class NewDiagramDialog : public QDialog {
    Q_OBJECT
public:
    NewDiagramDialog(ea::Repository& repo, int defaultPackageId, QWidget* parent = nullptr);

    QString diagramName() const;
    int packageId() const;
    QString kindKey() const; // key of an ea::UmlDiagramKind

    // For tests and scripted use
    void setDiagramName(const QString& name);
    bool selectKind(const QString& key);
    bool selectPackage(int packageId);

private:
    void populatePackages(const ea::Package& pkg, int depth);
    void onKindChanged();
    void updateOkButton();

    QLineEdit* m_nameEdit{nullptr};
    QComboBox* m_packageCombo{nullptr};
    QListWidget* m_kindList{nullptr};
    QDialogButtonBox* m_buttons{nullptr};
    bool m_nameEditedByUser{false};
};
