#include "ea/desktop/CollaborationService.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include "ea/core/Types.hpp"
#include "ea/xml/Xml.hpp"

namespace ea::desktop {

namespace {

std::string sqlEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '\0') continue; // strip embedded NULs so SQLite never truncates queries
        if (c == '\'') out += "''";
        else out.push_back(c);
    }
    return out;
}

std::string currentTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M");
    return oss.str();
}

std::string childText(const ea::xml::Node& row, const std::string& tag) {
    if (const auto* n = row.child(tag)) return n->text;
    return {};
}

int childInt(const ea::xml::Node& row, const std::string& tag, int fallback = 0) {
    const std::string t = childText(row, tag);
    if (t.empty()) return fallback;
    try {
        return std::stoi(t);
    } catch (...) {
        return fallback;
    }
}

void collectKanbanElements(const Package& pkg, std::vector<KanbanCard>& out) {
    for (size_t i = 0; i < pkg.GetElements().size(); ++i) {
        auto elem = pkg.GetElements().GetAt(static_cast<short>(i));
        if (!elem) continue;
        if (elem->GetType() == ObjectType::Package || elem->GetType() == ObjectType::Boundary) continue;
        KanbanCard card;
        card.elementId = elem->GetElementID();
        card.name = elem->GetName();
        card.typeName = to_string(elem->GetType());
        card.status = elem->GetStatus().empty() ? "Proposed" : elem->GetStatus();
        card.author = elem->GetAuthor();
        card.stereotype = elem->GetStereotype();
        out.push_back(std::move(card));
    }
    for (size_t i = 0; i < pkg.GetPackages().size(); ++i) {
        if (auto sub = pkg.GetPackages().GetAt(static_cast<short>(i))) {
            collectKanbanElements(*sub, out);
        }
    }
}

std::vector<const ea::xml::Node*> queryRows(Repository& repo, const std::string& sql, std::unique_ptr<ea::xml::Node>& parsedRoot) {
    std::vector<const ea::xml::Node*> rows;
    if (!repo.IsOpen()) return rows;
    const std::string xmlStr = repo.SQLQuery(sql);
    auto doc = ea::xml::parse(xmlStr);
    if (!doc.ok()) return rows;
    parsedRoot = std::move(doc.root);
    const auto* ds = parsedRoot->child("Dataset_0");
    if (!ds) return rows;
    const auto* data = ds->child("Data");
    if (!data) return rows;
    return data->childrenNamed("Row");
}

} // namespace

bool CollaborationService::ensureSchema(Repository& repo) {
    if (!repo.IsOpen()) return false;
    const bool ok1 = repo.Execute(
        "CREATE TABLE IF NOT EXISTS oa_collab_entries ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  kind TEXT NOT NULL,"
        "  element_id INTEGER DEFAULT 0,"
        "  author TEXT DEFAULT '',"
        "  title TEXT DEFAULT '',"
        "  body TEXT DEFAULT '',"
        "  status TEXT DEFAULT 'open',"
        "  created_at TEXT DEFAULT ''"
        ");");
    const bool ok2 = repo.Execute(
        "CREATE TABLE IF NOT EXISTS oa_project_tasks ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  title TEXT NOT NULL,"
        "  assignee TEXT DEFAULT '',"
        "  start_day INTEGER DEFAULT 1,"
        "  duration_days INTEGER DEFAULT 3,"
        "  progress INTEGER DEFAULT 0,"
        "  status TEXT DEFAULT 'Proposed',"
        "  element_id INTEGER DEFAULT 0,"
        "  due_date TEXT DEFAULT ''"
        ");");
    return ok1 && ok2;
}

bool CollaborationService::addEntry(Repository& repo, const CollabEntry& entry) {
    if (!ensureSchema(repo)) return false;
    const std::string ts = entry.createdAt.empty() ? currentTimestamp() : entry.createdAt;
    const std::string st = entry.status.empty() ? "open" : entry.status;
    std::ostringstream sql;
    sql << "INSERT INTO oa_collab_entries (kind, element_id, author, title, body, status, created_at) VALUES ("
        << "'" << sqlEscape(entry.kind) << "', "
        << entry.elementId << ", "
        << "'" << sqlEscape(entry.author) << "', "
        << "'" << sqlEscape(entry.title) << "', "
        << "'" << sqlEscape(entry.body) << "', "
        << "'" << sqlEscape(st) << "', "
        << "'" << sqlEscape(ts) << "');";
    return repo.Execute(sql.str());
}

bool CollaborationService::updateEntryStatus(Repository& repo, int entryId, const std::string& newStatus) {
    if (!ensureSchema(repo) || entryId <= 0) return false;
    std::ostringstream sql;
    sql << "UPDATE oa_collab_entries SET status = '" << sqlEscape(newStatus) << "' WHERE id = " << entryId << ";";
    return repo.Execute(sql.str());
}

bool CollaborationService::deleteEntry(Repository& repo, int entryId) {
    if (!ensureSchema(repo) || entryId <= 0) return false;
    return repo.Execute("DELETE FROM oa_collab_entries WHERE id = " + std::to_string(entryId) + ";");
}

std::vector<CollabEntry> CollaborationService::listEntries(Repository& repo, const std::string& kind, int elementId) {
    std::vector<CollabEntry> out;
    if (!ensureSchema(repo)) return out;

    std::ostringstream sql;
    sql << "SELECT id, kind, element_id, author, title, body, status, created_at FROM oa_collab_entries WHERE 1=1";
    if (!kind.empty()) sql << " AND kind = '" << sqlEscape(kind) << "'";
    if (elementId >= 0) sql << " AND element_id = " << elementId;
    sql << " ORDER BY id DESC;";

    std::unique_ptr<ea::xml::Node> root;
    for (const auto* row : queryRows(repo, sql.str(), root)) {
        CollabEntry e;
        e.id = childInt(*row, "id");
        e.kind = childText(*row, "kind");
        e.elementId = childInt(*row, "element_id");
        e.author = childText(*row, "author");
        e.title = childText(*row, "title");
        e.body = childText(*row, "body");
        e.status = childText(*row, "status");
        e.createdAt = childText(*row, "created_at");
        out.push_back(std::move(e));
    }
    return out;
}

std::vector<KanbanCard> CollaborationService::listKanbanCards(Repository& repo) {
    std::vector<KanbanCard> out;
    if (!repo.IsOpen()) return out;
    for (size_t i = 0; i < repo.GetModels().size(); ++i) {
        if (auto m = repo.GetModels().GetAt(static_cast<short>(i))) {
            collectKanbanElements(*m, out);
        }
    }
    return out;
}

bool CollaborationService::moveKanbanCard(Repository& repo, int elementId, const std::string& newStatus) {
    if (!repo.IsOpen() || elementId <= 0 || newStatus.empty()) return false;
    auto elem = repo.GetElementByID(elementId);
    if (!elem) return false;
    elem->SetStatus(newStatus);
    return repo.SaveElement(*elem);
}

bool CollaborationService::addTask(Repository& repo, const ProjectTask& task) {
    if (!ensureSchema(repo) || task.title.empty()) return false;
    const int startDay = std::max(1, task.startDay);
    const int dur = std::max(1, task.durationDays);
    const int prog = std::clamp(task.progress, 0, 100);
    std::ostringstream sql;
    sql << "INSERT INTO oa_project_tasks (title, assignee, start_day, duration_days, progress, status, element_id, due_date) VALUES ("
        << "'" << sqlEscape(task.title) << "', "
        << "'" << sqlEscape(task.assignee) << "', "
        << startDay << ", "
        << dur << ", "
        << prog << ", "
        << "'" << sqlEscape(task.status.empty() ? "Proposed" : task.status) << "', "
        << task.elementId << ", "
        << "'" << sqlEscape(task.dueDate) << "');";
    return repo.Execute(sql.str());
}

bool CollaborationService::updateTaskProgress(Repository& repo, int taskId, int progress, const std::string& status) {
    if (!ensureSchema(repo) || taskId <= 0) return false;
    const int prog = std::clamp(progress, 0, 100);
    std::ostringstream sql;
    sql << "UPDATE oa_project_tasks SET progress = " << prog
        << ", status = '" << sqlEscape(status) << "' WHERE id = " << taskId << ";";
    return repo.Execute(sql.str());
}

bool CollaborationService::deleteTask(Repository& repo, int taskId) {
    if (!ensureSchema(repo) || taskId <= 0) return false;
    return repo.Execute("DELETE FROM oa_project_tasks WHERE id = " + std::to_string(taskId) + ";");
}

std::vector<ProjectTask> CollaborationService::listTasks(Repository& repo) {
    std::vector<ProjectTask> out;
    if (!ensureSchema(repo)) return out;

    std::unique_ptr<ea::xml::Node> root;
    for (const auto* row : queryRows(repo,
             "SELECT id, title, assignee, start_day, duration_days, progress, status, element_id, due_date "
             "FROM oa_project_tasks ORDER BY start_day ASC, id ASC;", root)) {
        ProjectTask t;
        t.id = childInt(*row, "id");
        t.title = childText(*row, "title");
        t.assignee = childText(*row, "assignee");
        t.startDay = std::max(1, childInt(*row, "start_day", 1));
        t.durationDays = std::max(1, childInt(*row, "duration_days", 1));
        t.progress = std::clamp(childInt(*row, "progress", 0), 0, 100);
        t.status = childText(*row, "status");
        t.elementId = childInt(*row, "element_id");
        t.dueDate = childText(*row, "due_date");
        out.push_back(std::move(t));
    }
    return out;
}

} // namespace ea::desktop
