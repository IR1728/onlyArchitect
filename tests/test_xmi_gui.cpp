#ifdef NDEBUG
#undef NDEBUG
#endif
// XMI from the application: export a package, import it back, the Model Explorer shows it; a broken
// file is reported without a modal dialog and nothing changes.
#include <QApplication>
#include <QMessageBox>
#include <QTreeWidget>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "ea/repository/Repository.hpp"
#include "gui/MainWindow.hpp"
#include "gui/ModelTreeWidget.hpp"

namespace fs = std::filesystem;

static int treeItemsNamed(QWidget& window, const QString& text) {
    int n = 0;
    for (auto* tree : window.findChild<ModelTreeWidget*>()->findChildren<QTreeWidget*>()) {
        for (auto* item : tree->findItems(text, Qt::MatchContains | Qt::MatchRecursive)) n += item != nullptr;
    }
    return n;
}

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    std::cout << "==========================================\n";
    std::cout << " Running onlyArchitect XMI GUI Tests\n";
    std::cout << "==========================================\n";

    fs::path db = fs::temp_directory_path() / "onlyarchitect_test_xmi_gui.qea";
    fs::path file = fs::temp_directory_path() / "onlyarchitect_test_xmi_gui.xmi";
    fs::remove(db);
    int root = 0, pkgId = 0;
    {
        ea::Repository repo;
        assert(repo.CreateRepository(db.string()));
        root = repo.GetModels().GetAt(0)->GetPackageID();
        auto pkg = repo.CreatePackage(root, "Facturacion");
        pkgId = pkg->GetPackageID();
        ea::Element e;
        e.SetType(ea::ObjectType::Class);
        e.SetName("Factura");
        e.SetPackageID(pkgId);
        assert(repo.AddElement(e));
        repo.CloseFile();
    }

    {
    MainWindow window;
    assert(window.openFile(QString::fromStdString(db.string())));
    assert(treeItemsNamed(window, "Facturacion") == 1 && "a package appears once, not also as its t_object row");

    assert(window.exportXmi(pkgId, QString::fromStdString(file.string())));
    assert(fs::file_size(file) > 0);
    assert(window.importXmi(root, QString::fromStdString(file.string())));
    assert(treeItemsNamed(window, "Facturacion") == 2 && "the imported copy is in the Model Explorer");
    std::cout << "  ✓ Export a package to XMI and import it back from the window; the explorer refreshes\n";

    std::ofstream(file) << "<xmi:XMI><uml:Model>";
    const int dialogsBefore = static_cast<int>(window.findChildren<QMessageBox*>().size());
    assert(!window.importXmi(root, QString::fromStdString(file.string())));
    assert(static_cast<int>(window.findChildren<QMessageBox*>().size()) == dialogsBefore + 1 && "reported, non-blocking");
    assert(treeItemsNamed(window, "Facturacion") == 2);
    std::cout << "  ✓ A broken XMI file is reported (non-blocking) and changes nothing\n";
    } // Close MainWindow & SQLite handle before removing temp .qea (required on Windows)

    std::error_code ec;
    fs::remove(file, ec);
    fs::remove(db, ec);
    std::cout << "==========================================\n";
    std::cout << " ALL XMI GUI TESTS PASSED!\n";
    std::cout << "==========================================\n";
    return 0;
}
