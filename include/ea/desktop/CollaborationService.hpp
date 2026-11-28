#pragma once

#include <string>
#include <vector>
#include "ea/repository/Repository.hpp"

namespace ea::desktop {

struct CollabEntry {
    int id{0};
    std::string kind;      // "journal", "discuss", "review"
    int elementId{0};      // 0 = project-level
    std::string author;
    std::string title;
    std::string body;
    std::string status;    // "open", "approved", "changes_requested", "resolved"
    std::string createdAt; // ISO-like "YYYY-MM-DD HH:MM"
};

struct ProjectTask {
    int id{0};
    std::string title;
    std::string assignee;
    int startDay{1};       // 1-based project day (for Gantt)
    int durationDays{3};
    int progress{0};       // 0..100
    std::string status{"Proposed"};
    int elementId{0};
    std::string dueDate;   // "YYYY-MM-DD" (for Calendar)
};

struct KanbanCard {
    int elementId{0};
    std::string name;
    std::string typeName;
    std::string status;    // "Proposed", "Approved", "Implemented", "Validated"
    std::string author;
    std::string stereotype;
};

class CollaborationService {
public:
    static bool ensureSchema(Repository& repo);

    // Journal / Discussions / Reviews persisted in .qea (table oa_collab_entries)
    static bool addEntry(Repository& repo, const CollabEntry& entry);
    static bool updateEntryStatus(Repository& repo, int entryId, const std::string& newStatus);
    static bool deleteEntry(Repository& repo, int entryId);
    static std::vector<CollabEntry> listEntries(Repository& repo, const std::string& kind = "", int elementId = -1);

    // Real UML Kanban backed directly by t_object.Status
    static std::vector<KanbanCard> listKanbanCards(Repository& repo);
    static bool moveKanbanCard(Repository& repo, int elementId, const std::string& newStatus);

    // Real Gantt & Calendar tasks persisted in .qea (table oa_project_tasks)
    static bool addTask(Repository& repo, const ProjectTask& task);
    static bool updateTaskProgress(Repository& repo, int taskId, int progress, const std::string& status);
    static bool deleteTask(Repository& repo, int taskId);
    static std::vector<ProjectTask> listTasks(Repository& repo);
};

} // namespace ea::desktop
