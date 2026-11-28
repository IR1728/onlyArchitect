#ifdef NDEBUG
#undef NDEBUG
#endif
// guiaSecuencia.md §2.2 "Flujo de creación de un diagrama":
//   Diagrama > Nuevo Diagrama -> tipo -> nodo en el Model Explorer bajo el paquete activo
//   -> pestaña con el lienzo vacío -> ... -> se persiste en el proyecto
#include <QApplication>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTreeWidget>
#include <QUndoStack>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <map>
#include "ea/diagram/UmlDiagramKinds.hpp"
#include "ea/repository/Repository.hpp"
#include "gui/DiagramView.hpp"
#include "gui/MainWindow.hpp"
#include "gui/ModelTreeWidget.hpp"
#include "gui/NewDiagramDialog.hpp"
#include "gui/ValidationResultsWidget.hpp"

namespace fs = std::filesystem;

static DiagramView* findView(QWidget& window, int diagramId) {
    for (auto* v : window.findChildren<DiagramView*>()) {
        if (v->getDiagramId() == diagramId) return v;
    }
    return nullptr;
}

static QTabWidget* tabsHolding(QWidget& window, QWidget* page) {
    for (auto* tw : window.findChildren<QTabWidget*>()) {
        if (tw->indexOf(page) >= 0) return tw;
    }
    return nullptr;
}

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    std::cout << "==========================================\n";
    std::cout << " Running onlyArchitect New Diagram Tests\n";
    std::cout << "==========================================\n";

    // 1. Catalog: the 14 UML 2.x kinds of the guide, each stored in a way that reads back as itself
    const auto& kinds = ea::uml_diagram_kinds();
    assert(kinds.size() == 14);
    int structural = 0;
    for (const auto& k : kinds) {
        if (k.category == ea::UmlDiagramCategory::Structural) ++structural;
        assert(ea::diagram_type_from_string(ea::to_string(k.type)) == k.type && "Diagram_Type must round-trip");
        const auto* back = ea::find_uml_diagram_kind(k.type, k.stereotype);
        assert(back && back->key == k.key && "kind must be recognisable from type + stereotype");
    }
    assert(structural == 7);
    // Standard .qea diagram type names for interoperability
    assert(ea::to_string(ea::DiagramType::Communication) == "Collaboration");
    assert(ea::diagram_type_from_string("Collaboration") == ea::DiagramType::Communication);
    assert(ea::diagram_type_from_string("Object") == ea::DiagramType::Object);
    assert(ea::diagram_type_from_string("InteractionOverview") == ea::DiagramType::InteractionOverview);
    std::cout << "  ✓ Catalog: 14 UML 2.x kinds (7 structural, 7 behavioral), EA type names\n";

    // A blank project, as created by Archivo > Nuevo
    fs::path db = fs::temp_directory_path() / "onlyarchitect_test_new_diagram.qea";
    fs::remove(db);
    int modelPackageId = 0;
    {
        ea::Repository repo;
        assert(repo.CreateRepository(db.string()));
        assert(repo.GetModels().size() == 1);
        modelPackageId = repo.GetModels().GetAt(0)->GetPackageID();
        assert(!repo.CreateDiagram(987654, "Nope", ea::DiagramType::Logical) && "unknown package must fail");
        assert(!repo.CreateDiagram(modelPackageId, "", ea::DiagramType::Logical) && "empty name must fail");

        // 2. The dialog: given package preselected, "Clases" by default, name suggested, OK needs a name
        NewDiagramDialog dlg(repo, modelPackageId);
        auto* ok = dlg.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
        assert(dlg.packageId() == modelPackageId);
        assert(dlg.kindKey() == "class");
        assert(dlg.diagramName() == "Diagrama de Clases");
        assert(ok->isEnabled());
        assert(dlg.selectKind("sequence"));
        assert(dlg.diagramName() == "Diagrama de Secuencia" && "suggested name follows the kind");
        dlg.setDiagramName("   ");
        assert(!ok->isEnabled() && "a blank name cannot be accepted");
        dlg.setDiagramName("Login");
        assert(dlg.selectKind("timing"));
        assert(dlg.diagramName() == "Login" && "a typed name is not overwritten");
        assert(ok->isEnabled());
        assert(!dlg.selectKind("no_such_kind"));
        repo.CloseFile();
    }
    std::cout << "  ✓ Repository::CreateDiagram validation and NewDiagramDialog behaviour\n";

    std::map<int, std::string> created; // diagramId -> kind key
    {
        MainWindow window;
        assert(window.openFile(QString::fromStdString(db.string())));
        auto* tree = window.findChild<ModelTreeWidget*>()->findChild<QTreeWidget*>();
        assert(tree);

        // 3. One diagram of every kind: tree node under the package, its own tab, empty canvas
        for (const auto& k : kinds) {
            const QString name = QString::fromStdString("Diagrama " + k.label);
            const int id = window.createDiagram(modelPackageId, name, QString::fromStdString(k.key));
            assert(id > 0);
            created[id] = k.key;

            auto* item = tree->currentItem();
            assert(item && item->data(0, Qt::UserRole).toInt() == static_cast<int>(TreeItemType::Diagram));
            assert(item->data(0, Qt::UserRole + 1).toInt() == id && "new diagram is selected in the Model Explorer");
            assert(item->parent() && item->parent()->data(0, Qt::UserRole + 1).toInt() == modelPackageId);

            auto* view = findView(window, id);
            assert(view && "a tab is opened for the new diagram");
            auto* tabs = tabsHolding(window, view);
            assert(tabs && tabs->currentWidget() == view && "the new tab is the active one");
            assert(view->scene()->items().isEmpty() && "the canvas starts empty");
            assert(view->undoStack()->count() == 0);
        }
        assert(created.size() == kinds.size());
        std::cout << "  ✓ Created all 14 kinds: tree node, active tab, empty canvas\n";

        // 4. A unknown kind key is refused without touching the model
        assert(window.createDiagram(modelPackageId, "X", "no_such_kind") == 0);

        // 5. The new canvas is editable and undoable (point 1 integration)
        const int classDiagramId = created.begin()->first;
        auto* view = findView(window, classDiagramId);
        const int elemId = view->addElement(ea::ObjectType::Class, "Cliente", QPointF(60, 80));
        assert(elemId > 0 && view->elementItem(elemId));
        view->undoStack()->undo();
        assert(!view->elementItem(elemId));
        view->undoStack()->redo();
        assert(view->elementItem(elemId));
        assert(view->saveDiagramLayout());
        std::cout << "  ✓ New diagram is editable, undoable and savable\n";

        // 5b. guiaSecuencia.md §3.2 "Validación": an invalid relationship shows an error and is not created
        const int iface = view->addElement(ea::ObjectType::Interface, "IRepositorio", QPointF(60, 300));
        QString rejection;
        QObject::connect(view, &DiagramView::editRejected, [&rejection](const QString& r) { rejection = r; });
        const int stepsBefore = view->undoStack()->count();
        assert(view->addConnector(iface, elemId, ea::ConnectorType::Generalization) == 0);
        assert(!rejection.isEmpty() && rejection.contains("Realización") && "the user is told why");
        assert(view->undoStack()->count() == stepsBefore && "no undo step for a refused edit");
        assert(view->scene()->items().size() == 2 && "no connector drawn");
        const int realization = view->addConnector(elemId, iface, ea::ConnectorType::Realization);
        assert(realization > 0 && view->connectorItem(realization) && "the correct relationship is created");
        std::cout << "  ✓ Invalid relationship refused with a message; valid one created\n";

        // 5c. Model validation from the main window: report, results window, status bar
        auto* resultsWidget = window.findChild<ValidationResultsWidget*>();
        assert(resultsWidget);
        auto report = window.validateModel();
        assert(report.count(ea::validation::Severity::Error) == 0 && "everything created through the editor is valid");
        assert(resultsWidget->issueCount() == static_cast<int>(report.issues.size()));
        assert(report.elementsChecked >= 2);
        std::cout << "  ✓ Validate model: " << validation_summary(report).toStdString() << "\n";
    }

    // 6. Persistence: reopen the project and find every diagram with its kind
    {
        ea::Repository repo;
        assert(repo.OpenFile(db.string()));
        auto model = repo.GetModels().GetAt(0);
        assert(model->GetDiagrams().size() == created.size());
        for (const auto& [id, key] : created) {
            auto d = repo.GetDiagramByID(id);
            assert(d && d->GetPackageID() == model->GetPackageID());
            const auto* kind = ea::find_uml_diagram_kind(d->GetType(), d->GetStereotype());
            assert(kind && kind->key == key && "kind survives save + reload");
            assert(!d->GetDiagramGUID().empty());
        }
        auto first = repo.GetDiagramByID(created.begin()->first);
        assert(first->GetDiagramObjects().size() == 2 && "the class and the interface are on the reopened diagram");
        assert(first->GetDiagramLinks().size() == 1 && "only the valid realization was stored");

        const std::string types = repo.SQLQuery("SELECT Diagram_Type FROM t_diagram ORDER BY Diagram_ID;");
        assert(types.find("Collaboration") != std::string::npos);
        assert(types.find("InteractionOverview") != std::string::npos);
        repo.CloseFile();
    }
    std::cout << "  ✓ Diagrams persist with type, stereotype, package and contents\n";

    fs::remove(db);
    std::cout << "==========================================\n";
    std::cout << " ALL NEW DIAGRAM TESTS PASSED!\n";
    std::cout << "==========================================\n";
    return 0;
}
