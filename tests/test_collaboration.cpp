#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <iostream>
#include "ea/desktop/CollaborationService.hpp"
#include "ea/repository/Repository.hpp"

namespace fs = std::filesystem;

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Running Collaboration & Planner Core Tests (.qea)     \n";
    std::cout << "=======================================================\n";

    const fs::path db = fs::temp_directory_path() / "onlyarchitect_test_collab.qea";
    fs::remove(db);

    int elemId = 0;
    {
        ea::Repository repo;
        assert(repo.CreateRepository(db.string()));
        auto root = repo.GetModels().GetAt(0);
        assert(root);

        ea::Element cls;
        cls.SetName("AuthService");
        cls.SetType(ea::ObjectType::Class);
        cls.SetStatus("Proposed");
        cls.SetAuthor("Lenin");
        cls.SetPackageID(root->GetPackageID());
        auto saved = repo.AddElement(cls);
        assert(saved);
        elemId = saved->GetElementID();

        // 1. Journal, Discuss, Review CRUD
        assert(ea::desktop::CollaborationService::addEntry(repo, {
            0, "journal", elemId, "Lenin", "Decision JWT", "Usar tokens cortos con refresh", "open", "2026-11-23 20:00"
        }));
        assert(ea::desktop::CollaborationService::addEntry(repo, {
            0, "review", elemId, "Iran", "Revisar AuthService", "Falta diagrama de secuencia", "open", "2026-11-23 20:05"
        }));

        auto reviews = ea::desktop::CollaborationService::listEntries(repo, "review", elemId);
        assert(reviews.size() == 1);
        assert(reviews[0].title == "Revisar AuthService");
        assert(ea::desktop::CollaborationService::updateEntryStatus(repo, reviews[0].id, "approved"));

        // 2. Kanban backed by real Element::Status
        auto cards = ea::desktop::CollaborationService::listKanbanCards(repo);
        assert(cards.size() == 1 && cards[0].status == "Proposed");
        assert(ea::desktop::CollaborationService::moveKanbanCard(repo, elemId, "Implemented"));

        // 3. Gantt & Calendar tasks
        assert(ea::desktop::CollaborationService::addTask(repo, {
            0, "Modelar AuthService", "Lenin", 2, 5, 25, "Approved", elemId, "2026-11-30"
        }));
        auto tasks = ea::desktop::CollaborationService::listTasks(repo);
        assert(tasks.size() == 1 && tasks[0].durationDays == 5 && tasks[0].dueDate == "2026-11-30");
        assert(ea::desktop::CollaborationService::updateTaskProgress(repo, tasks[0].id, 100, "Validated"));

        repo.CloseFile();
    }

    // Reopen .qea and confirm full SQLite persistence
    {
        ea::Repository reopened;
        assert(reopened.OpenFile(db.string()));

        auto reviews = ea::desktop::CollaborationService::listEntries(reopened, "review", elemId);
        assert(reviews.size() == 1 && reviews[0].status == "approved");
        auto journals = ea::desktop::CollaborationService::listEntries(reopened, "journal", -1);
        assert(journals.size() == 1 && journals[0].title == "Decision JWT");

        auto elem = reopened.GetElementByID(elemId);
        assert(elem && elem->GetStatus() == "Implemented");

        auto tasks = ea::desktop::CollaborationService::listTasks(reopened);
        assert(tasks.size() == 1 && tasks[0].progress == 100 && tasks[0].status == "Validated");
        reopened.CloseFile();
    }

    fs::remove(db);
    std::cout << "  ✓ Journal, Discuss, Review persisted in .qea\n";
    std::cout << "  ✓ Kanban status transitions persisted to real t_object.Status\n";
    std::cout << "  ✓ Gantt & Calendar tasks persisted across reopen\n";
    std::cout << "=======================================================\n";
    std::cout << " ALL COLLABORATION CORE TESTS PASSED!                  \n";
    std::cout << "=======================================================\n";
    return 0;
}
