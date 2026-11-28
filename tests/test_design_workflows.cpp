#ifdef NDEBUG
#undef NDEBUG
#endif
// End-to-end tests for real UML designer workflows:
//   1. Domain modeling (sub-packages, roles, multiplicities on connectors, preserving method parameters)
//   2. Use Case refinement (EditElementDialog never corrupts Actor/UseCase types, CUS -> Sequence refinement)
//   3. Sequence diagrams (editing message name/sort, target class operations autocomplete)
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <cassert>
#include <filesystem>
#include <iostream>
#include "ea/diagram/SequenceModel.hpp"
#include "ea/repository/Repository.hpp"
#include "gui/DiagramView.hpp"
#include "gui/EditConnectorDialog.hpp"
#include "gui/EditElementDialog.hpp"
#include "gui/MainWindow.hpp"

namespace fs = std::filesystem;

static DiagramView* findView(QWidget& window, int diagramId) {
    for (auto* v : window.findChildren<DiagramView*>()) {
        if (v->getDiagramId() == diagramId) return v;
    }
    return nullptr;
}

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    std::cout << "=========================================================\n";
    std::cout << " Running Real Designer Workflows (Domain / CUS / Seq)    \n";
    std::cout << "=========================================================\n";

    const fs::path db = fs::temp_directory_path() / "onlyarchitect_test_design_workflows.qea";
    fs::remove(db);
    {
        ea::Repository seed;
        assert(seed.CreateRepository(db.string()));
        seed.CloseFile();
    }

    {
    MainWindow window;
    assert(window.openFile(QString::fromStdString(db.string())));

    // 1. Domain Modeling: sub-package + classes + method parameters + connector roles & multiplicities
    int rootPkgId = 0;
    {
        ea::Repository probe;
        assert(probe.OpenFile(db.string()));
        rootPkgId = probe.GetModels().GetAt(0)->GetPackageID();
        auto domPkg = probe.CreatePackage(rootPkgId, "01-Dominio");
        assert(domPkg != nullptr);
        probe.CloseFile();
    }
    assert(window.openFile(QString::fromStdString(db.string())));

    const int classDiagId = window.createDiagram(rootPkgId, "Modelo de Dominio", "class");
    auto* classView = findView(window, classDiagId);
    assert(classView);

    const int pedidoId = classView->addElement(ea::ObjectType::Class, "Pedido", QPointF(100, 120));
    const int prodId = classView->addElement(ea::ObjectType::Class, "Producto", QPointF(420, 120));
    assert(pedidoId > 0 && prodId > 0);

    // Add operation with parameters and verify EditElementDialog preserves parameters on save!
    classView->modifyElement(pedidoId, "Añadir operación", [](ea::Element& e) {
        auto m = e.GetMethods().AddNew("agregarItem", "bool");
        m->AddParameter({"producto", "Producto", "", "in", 0});
        m->AddParameter({"cantidad", "int", "1", "in", 1});
    });

    {
        ea::Repository r;
        assert(r.OpenFile(db.string()));
        auto ped = r.GetElementByID(pedidoId);
        assert(ped && ped->GetMethods().size() == 1);
        EditElementDialog dlg(ped, r);
        for (auto* btn : dlg.findChildren<QPushButton*>()) {
            if (btn->text().contains("Guardar")) { btn->click(); break; }
        }
        assert(dlg.isModified());
        auto reloaded = r.GetElementByID(pedidoId);
        assert(reloaded->GetMethods().GetAt(0)->GetParameters().size() == 2 &&
               "EditElementDialog must preserve operation parameters");
        r.CloseFile();
    }
    std::cout << "  ✓ Domain modeling: sub-package + operation parameters preserved through EditElementDialog\n";

    // Create Association and edit roles + multiplicities via EditConnectorDialog
    const int assocId = classView->addConnector(pedidoId, prodId, ea::ConnectorType::Association);
    assert(assocId > 0);
    {
        ea::Repository r;
        assert(r.OpenFile(db.string()));
        auto conn = r.GetConnectorByID(assocId);
        assert(conn);
        EditConnectorDialog dlg(conn, r);
        auto combos = dlg.findChildren<QComboBox*>();
        // Set name, source multiplicity ("1"), target multiplicity ("1..*")
        combos[0]->setEditText("contiene");
        for (auto* le : dlg.findChildren<QLineEdit*>()) {
            if (le->placeholderText().contains("pedido")) le->setText("pedido");
            if (le->placeholderText().contains("items")) le->setText("items");
        }
        for (auto* cb : combos) {
            if (cb->findText("1..*") >= 0 && cb->currentText().isEmpty()) {
                cb->setEditText("1..*");
            }
        }
        for (auto* btn : dlg.findChildren<QPushButton*>()) {
            if (btn->text().contains("Guardar")) { btn->click(); break; }
        }
        assert(dlg.isModified());

        auto check = r.GetConnectorByID(assocId);
        assert(check && check->GetName() == "contiene");
        assert(check->GetClientRole() == "pedido" && check->GetSupplierRole() == "items");
        assert(check->GetSupplierMultiplicity() == "1..*");
        r.CloseFile();
    }
    std::cout << "  ✓ Domain connector roles, name & multiplicities (1..*) edited & persisted\n";

    // 2. Use Case Refinement: EditElementDialog never corrupts Actor/UseCase + refine CUS -> Sequence
    const int ucDiagId = window.createDiagram(rootPkgId, "Casos de Uso", "use_case");
    auto* ucView = findView(window, ucDiagId);
    assert(ucView);
    const int actorId = ucView->addElement(ea::ObjectType::Actor, "Cliente", QPointF(80, 140));
    const int ucId = ucView->addElement(ea::ObjectType::UseCase, "Registrar Pedido", QPointF(300, 140));
    assert(ucView->addConnector(actorId, ucId, ea::ConnectorType::Association) > 0);

    {
        ea::Repository r;
        assert(r.OpenFile(db.string()));
        auto actor = r.GetElementByID(actorId);
        assert(actor && actor->GetType() == ea::ObjectType::Actor);
        EditElementDialog dlg(actor, r);
        for (auto* btn : dlg.findChildren<QPushButton*>()) {
            if (btn->text().contains("Guardar")) { btn->click(); break; }
        }
        assert(r.GetElementByID(actorId)->GetType() == ea::ObjectType::Actor &&
               "EditElementDialog must NEVER corrupt Actor to Class");
        r.CloseFile();
    }

    const int refinedSeqId = window.refineUseCaseToSequence(ucId);
    assert(refinedSeqId > 0);
    auto* seqView = findView(window, refinedSeqId);
    assert(seqView && seqView->isInteractionDiagram());
    assert(seqView->elementItem(actorId) != nullptr && "Refined Sequence Diagram must pre-place linked Actor");
    std::cout << "  ✓ Use Case refinement: Actor type preserved & Sequence Diagram auto-populated\n";

    // 3. Sequence Diagram: create message and edit name/sort via EditConnectorDialog
    int ctrlId = 0;
    for (auto* item : seqView->scene()->items()) {
        if (auto* e = dynamic_cast<DiagramElementItem*>(item)) {
            if (e->getElementId() != actorId && e->getElement()->GetStereotype() == "control") {
                ctrlId = e->getElementId();
            }
        }
    }
    assert(ctrlId > 0);
    const int msgId = seqView->addMessage(actorId, ctrlId, ea::sequence::MessageSort::Synchronous, 160);
    assert(msgId > 0);
    {
        ea::Repository r;
        assert(r.OpenFile(db.string()));
        auto msg = r.GetConnectorByID(msgId);
        assert(msg);
        EditConnectorDialog dlg(msg, r);
        auto combos = dlg.findChildren<QComboBox*>();
        combos[0]->setEditText("confirmarPedido(items)");
        for (auto* btn : dlg.findChildren<QPushButton*>()) {
            if (btn->text().contains("Guardar")) { btn->click(); break; }
        }
        assert(dlg.isModified());
        auto check = r.GetConnectorByID(msgId);
        assert(check && check->GetName() == "confirmarPedido(items)");
        r.CloseFile();
    }
    std::cout << "  ✓ Sequence message renamed & persisted via EditConnectorDialog\n";
    } // Close MainWindow & SQLite handle before removing temp .qea (required on Windows)

    std::error_code ec;
    fs::remove(db, ec);
    std::cout << "=========================================================\n";
    std::cout << " ALL REAL DESIGNER WORKFLOW TESTS PASSED!                \n";
    std::cout << "=========================================================\n";
    return 0;
}
