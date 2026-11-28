#ifdef NDEBUG
#undef NDEBUG
#endif
// guiaSecuencia.md §1.1 / §2.2 step 8: the Property Editor edits the selected element; every edit
// is one undoable step, invalid input is refused in place (no modal dialog).
#include <QApplication>
#include <QCheckBox>
#include <QLineEdit>
#include <QTableWidget>
#include <QToolButton>
#include <QUndoGroup>
#include <QUndoStack>
#include <cassert>
#include <filesystem>
#include <iostream>
#include "ea/repository/Repository.hpp"
#include "gui/DiagramView.hpp"
#include "gui/MainWindow.hpp"
#include "gui/PropertiesWidget.hpp"

namespace fs = std::filesystem;

namespace {

void settle() {
    for (int i = 0; i < 3; ++i) QApplication::processEvents();
}

void typeInto(QLineEdit* edit, const QString& text) {
    edit->setText(text);
    edit->setModified(true);
    emit edit->editingFinished();
    settle();
}

void setCell(QTableWidget* table, int row, int column, const QString& text) {
    table->item(row, column)->setText(text); // itemChanged, as when the user finishes editing a cell
    settle();
}

template <typename T>
T* child(QWidget& parent, const char* name) {
    auto* w = parent.findChild<T*>(name);
    assert(w && name);
    return w;
}

} // namespace

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    std::cout << "==========================================\n";
    std::cout << " Running onlyArchitect Property Editor Tests\n";
    std::cout << "==========================================\n";

    fs::path db = fs::temp_directory_path() / "onlyarchitect_test_property_editor.qea";
    fs::remove(db);
    int pkg = 0, looseClassId = 0;
    {
        ea::Repository repo;
        assert(repo.CreateRepository(db.string()));
        pkg = repo.GetModels().GetAt(0)->GetPackageID();
        ea::Element loose;
        loose.SetType(ea::ObjectType::Class);
        loose.SetName("SinDiagrama");
        loose.SetPackageID(pkg);
        looseClassId = repo.AddElement(loose)->GetElementID();
        repo.CloseFile();
    }

    {
    MainWindow window;
    assert(window.openFile(QString::fromStdString(db.string())));
    auto* props = window.findChild<PropertiesWidget*>();
    auto* undo = window.findChild<QUndoGroup*>();
    assert(props && undo);

    // 0. With no diagram open, edits go to the model's own undo history
    props->showElement(looseClassId);
    auto* nameEdit = child<QLineEdit>(*props, "propName");
    typeInto(nameEdit, "Cliente");
    assert(props->elementId() == looseClassId);
    {
        ea::Repository probe;
        assert(probe.OpenFile(db.string()));
        assert(probe.GetElementByID(looseClassId)->GetName() == "Cliente" && "persisted");
        probe.CloseFile();
    }
    assert(undo->canUndo());
    undo->undo();
    assert(nameEdit->text() == "SinDiagrama" && "the form follows undo");
    std::cout << "  ✓ Editing without a diagram: persisted and undoable (model history)\n";

    const int diagramId = window.createDiagram(pkg, "Ventas", "class");
    DiagramView* view = nullptr;
    for (auto* v : window.findChildren<DiagramView*>()) if (v->getDiagramId() == diagramId) view = v;
    assert(view);
    const auto& palette = ea::palette_for(ea::find_uml_diagram_kind("class"));
    const ea::PaletteTool* classTool = nullptr;
    for (const auto& g : palette.groups) for (const auto& t : g.tools) if (t.label == "Class") classTool = &t;
    const int classId = view->applyTool(*classTool, QPointF(200, 200));
    emit view->elementSelected(classId);
    assert(props->elementId() == classId);
    auto elem = [&] { return view->elementItem(classId)->getElement(); };
    const int base = view->undoStack()->count();

    // 1. General properties: each commit is one undo step in the diagram's history
    typeInto(nameEdit, "Pedido");
    typeInto(child<QLineEdit>(*props, "propStereotype"), "entity");
    typeInto(child<QLineEdit>(*props, "propAlias"), "Order");
    child<QCheckBox>(*props, "propAbstract")->setChecked(true);
    settle();
    assert(elem()->GetName() == "Pedido" && elem()->GetStereotype() == "entity" && elem()->GetAlias() == "Order" &&
           elem()->IsAbstract());
    assert(view->undoStack()->count() == base + 4);
    view->undoStack()->undo();
    settle();
    assert(!elem()->IsAbstract() && !child<QCheckBox>(*props, "propAbstract")->isChecked());
    view->undoStack()->redo();
    settle();

    // Invalid name: refused in place, model untouched, no undo step
    typeInto(nameEdit, "   ");
    assert(!props->lastError().isEmpty() && elem()->GetName() == "Pedido" && nameEdit->text() == "Pedido");
    assert(view->undoStack()->count() == base + 4);
    std::cout << "  ✓ Name, stereotype, alias, abstract: edited, undoable; empty name refused in place\n";

    // 2. Attributes: add, edit every column, validation, reorder, delete
    auto* attrs = child<QTableWidget>(*props, "attributesTable");
    child<QToolButton>(*props, "addAttribute")->click();
    settle();
    child<QToolButton>(*props, "addAttribute")->click();
    settle();
    assert(attrs->rowCount() == 2 && elem()->GetAttributes().size() == 2);
    assert(attrs->item(0, PropertiesWidget::AttrVisibility)->text() == "-" && "attributes are private by default");
    setCell(attrs, 0, PropertiesWidget::AttrName, "lineas");
    setCell(attrs, 0, PropertiesWidget::AttrType, "Linea");
    setCell(attrs, 0, PropertiesWidget::AttrMultiplicity, "1..*");
    setCell(attrs, 0, PropertiesWidget::AttrVisibility, "#");
    attrs->item(0, PropertiesWidget::AttrStatic)->setCheckState(Qt::Checked);
    settle();
    auto first = elem()->GetAttributes().GetAt(0);
    assert(first->GetName() == "lineas" && first->GetType() == "Linea" && first->GetLowerBound() == "1" &&
           first->GetUpperBound() == "*" && first->GetVisibility() == ea::Scope::Protected && first->IsStatic());

    const int beforeInvalid = view->undoStack()->count();
    setCell(attrs, 0, PropertiesWidget::AttrMultiplicity, "5..2");
    assert(!props->lastError().isEmpty() && first->GetUpperBound() == "*");
    assert(attrs->item(0, PropertiesWidget::AttrMultiplicity)->text() == "1..*" && "cell restored");
    setCell(attrs, 0, PropertiesWidget::AttrName, "");
    assert(elem()->GetAttributes().GetAt(0)->GetName() == "lineas");
    assert(view->undoStack()->count() == beforeInvalid);

    attrs->setCurrentCell(0, PropertiesWidget::AttrName);
    child<QToolButton>(*props, "attributeDown")->click();
    settle();
    assert(elem()->GetAttributes().GetAt(1)->GetName() == "lineas" && attrs->item(1, PropertiesWidget::AttrName)->text() == "lineas");
    assert(attrs->currentRow() == 1 && "selection follows the moved attribute");
    child<QToolButton>(*props, "removeAttribute")->click();
    settle();
    assert(elem()->GetAttributes().size() == 1 && attrs->rowCount() == 1);
    view->undoStack()->undo();
    settle();
    assert(elem()->GetAttributes().size() == 2 && attrs->rowCount() == 2);
    std::cout << "  ✓ Attributes: add / edit (vis, name, type, multiplicity, static) / reorder / delete, with validation\n";

    // 3. Operations with parameters
    auto* ops = child<QTableWidget>(*props, "operationsTable");
    child<QToolButton>(*props, "addOperation")->click();
    settle();
    assert(ops->rowCount() == 1);
    setCell(ops, 0, PropertiesWidget::OpName, "total");
    setCell(ops, 0, PropertiesWidget::OpParameters, "moneda: string, out redondeo: int = 2");
    setCell(ops, 0, PropertiesWidget::OpReturn, "double");
    auto op = elem()->GetMethods().GetAt(0);
    assert(op->GetName() == "total" && op->GetReturnType() == "double" && op->GetParameters().size() == 2 &&
           op->GetParameters()[1].kind == "out" && op->GetParameters()[1].default_value == "2");
    setCell(ops, 0, PropertiesWidget::OpParameters, "a b: int");
    assert(!props->lastError().isEmpty() && elem()->GetMethods().GetAt(0)->GetParameters().size() == 2);
    std::cout << "  ✓ Operations: name, parameters (parsed, validated), return type\n";

    // 4. Everything was persisted as it was edited
    {
        ea::Repository probe;
        assert(probe.OpenFile(db.string()));
        auto saved = probe.GetElementByID(classId);
        assert(saved->GetName() == "Pedido" && saved->GetAlias() == "Order" && saved->GetAttributes().size() == 2 &&
               saved->GetMethods().size() == 1 && saved->GetMethods().GetAt(0)->GetParameters().size() == 2);
        probe.CloseFile();
    }
    std::cout << "  ✓ Edits are in the repository without an explicit Save\n";
    } // Close MainWindow & SQLite handle before removing temp .qea (required on Windows)

    std::error_code ec;
    fs::remove(db, ec);
    std::cout << "==========================================\n";
    std::cout << " ALL PROPERTY EDITOR TESTS PASSED!\n";
    std::cout << "==========================================\n";
    return 0;
}
