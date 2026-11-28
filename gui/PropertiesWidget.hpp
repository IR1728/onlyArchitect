#pragma once

#include <QWidget>
#include <functional>
#include "ea/model/Element.hpp"
#include "ea/repository/Repository.hpp"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTabWidget;
class QTableWidget;
class QTableWidgetItem;
class QToolButton;

// Property Editor (guiaSecuencia.md §1.1, §2.2 step 8): shows the selected element and lets the user
// edit it. It never writes the model itself: every change is described as a mutation and handed to
// whoever owns the undo stack (editRequested), so each edit is one undoable step.
class PropertiesWidget : public QWidget {
    Q_OBJECT
public:
    using Mutation = std::function<void(ea::Element&)>;

    // Columns of the feature tables
    enum AttributeColumn { AttrVisibility, AttrName, AttrType, AttrMultiplicity, AttrDefault, AttrStatic, AttrColumns };
    enum OperationColumn { OpVisibility, OpName, OpParameters, OpReturn, OpStatic, OpAbstract, OpColumns };

    explicit PropertiesWidget(ea::Repository& repo, QWidget* parent = nullptr);

    void showElement(int elementId);
    void showDiagram(int diagramId);
    void clear();
    void showTab(const QString& title);
    int elementId() const { return m_elementId; }
    QString lastError() const;

signals:
    void editRequested(int elementId, const QString& description, const PropertiesWidget::Mutation& mutation);

private:
    void setupUi();
    QWidget* buildGeneralTab();
    QWidget* buildAttributesTab();
    QWidget* buildOperationsTab();
    void setEditable(bool editable);

    // Populating (signals blocked: filling the form is not an edit)
    void fillGeneral(const ea::Element& elem);
    void fillAttributes(const ea::Element& elem);
    void fillOperations(const ea::Element& elem);

    // Committing
    void requestEdit(const QString& description, Mutation mutation);
    void commitText(QLineEdit* edit, const QString& property, void (ea::Element::*setter)(std::string));
    void commitName();
    void commitNotes();
    void onAttributeChanged(QTableWidgetItem* item);
    void onOperationChanged(QTableWidgetItem* item);
    void addAttribute();
    void addOperation();
    void removeSelectedFeature(QTableWidget* table, bool attributes);
    void moveSelectedFeature(QTableWidget* table, bool attributes, int delta);
    void reject(const QString& message); // invalid input: show why and restore the model's values

    ea::Repository& m_repo;
    int m_elementId{0};
    QTabWidget* m_tabs{nullptr};
    QLabel* m_errorLabel{nullptr};

    // General tab
    QLineEdit* m_nameEdit{nullptr};
    QLineEdit* m_typeEdit{nullptr};
    QLineEdit* m_stereotypeEdit{nullptr};
    QLineEdit* m_aliasEdit{nullptr};
    QComboBox* m_statusCombo{nullptr};
    QLineEdit* m_versionEdit{nullptr};
    QLineEdit* m_authorEdit{nullptr};
    QCheckBox* m_abstractCheck{nullptr};
    QLineEdit* m_guidEdit{nullptr};
    QPlainTextEdit* m_notesEdit{nullptr};

    // Feature tabs
    QTableWidget* m_attributesTable{nullptr};
    QTableWidget* m_operationsTable{nullptr};
    QTableWidget* m_connectorsTable{nullptr};
    QWidget* m_attributeButtons{nullptr};
    QWidget* m_operationButtons{nullptr};
};
