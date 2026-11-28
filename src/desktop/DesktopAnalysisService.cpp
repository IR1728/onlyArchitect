#include "ea/desktop/DesktopAnalysisService.hpp"
#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>

namespace ea::desktop {

namespace {
std::string toLowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool matchString(const std::string& fieldVal,
                 const std::string& filterVal,
                 DiagramFilterOperator op) {
    std::string f = toLowerCopy(fieldVal);
    std::string v = toLowerCopy(filterVal);
    if (v.empty()) return true;

    switch (op) {
        case DiagramFilterOperator::Contains:
            return f.find(v) != std::string::npos;
        case DiagramFilterOperator::EqualTo:
            return f == v;
        case DiagramFilterOperator::NotEqualTo:
            return f != v;
        case DiagramFilterOperator::InRange: {
            auto sep = v.find("..");
            if (sep == std::string::npos) return f.find(v) != std::string::npos;
            std::string low = v.substr(0, sep);
            std::string high = v.substr(sep + 2);
            return f >= low && f <= high;
        }
    }
    return true;
}

void collectPackagesRecursive(const std::shared_ptr<Package>& pkg,
                              std::vector<std::shared_ptr<Package>>& out) {
    if (!pkg) return;
    out.push_back(pkg);
    for (size_t i = 0; i < pkg->GetPackages().size(); ++i) {
        collectPackagesRecursive(pkg->GetPackages().GetAt(static_cast<short>(i)), out);
    }
}
} // namespace

void StartPageProjectCatalog::AddOrTouchRecent(const std::string& name,
                                               const std::string& connectionString,
                                               const std::string& isoDate) {
    for (auto& p : m_pinned) {
        if (p.connectionString == connectionString) {
            p.lastOpenedIsoDate = isoDate;
            return;
        }
    }

    auto it = std::find_if(m_recent.begin(), m_recent.end(),
                           [&](const ProjectLinkEntry& e) { return e.connectionString == connectionString; });
    bool watched = false;
    if (it != m_recent.end()) {
        watched = it->isWatched;
        m_recent.erase(it);
    }

    ProjectLinkEntry entry;
    entry.name = name;
    entry.connectionString = connectionString;
    entry.lastOpenedIsoDate = isoDate;
    entry.isWatched = watched;
    entry.isCloud = (connectionString.find("http://") == 0 || connectionString.find("https://") == 0);

    m_recent.insert(m_recent.begin(), entry);
    if (m_recent.size() > kMaxRecentProjects) {
        m_recent.resize(kMaxRecentProjects);
    }
}

bool StartPageProjectCatalog::PinProject(const std::string& connectionString) {
    auto it = std::find_if(m_recent.begin(), m_recent.end(),
                           [&](const ProjectLinkEntry& e) { return e.connectionString == connectionString; });
    if (it == m_recent.end() || m_pinned.size() >= kMaxPinnedProjects) {
        return false;
    }
    ProjectLinkEntry moved = *it;
    m_recent.erase(it);
    m_pinned.push_back(moved);
    return true;
}

bool StartPageProjectCatalog::RenameProject(const std::string& connectionString,
                                            const std::string& newName) {
    if (newName.empty()) return false;
    for (auto& e : m_recent) {
        if (e.connectionString == connectionString) {
            e.name = newName;
            return true;
        }
    }
    for (auto& e : m_pinned) {
        if (e.connectionString == connectionString) {
            e.name = newName;
            return true;
        }
    }
    return false;
}

bool StartPageProjectCatalog::UpdateConnectionString(const std::string& oldConn,
                                                     const std::string& newConn) {
    if (newConn.empty()) return false;
    for (auto& e : m_pinned) {
        if (e.connectionString == oldConn) {
            e.connectionString = newConn;
            e.isCloud = (newConn.find("http://") == 0 || newConn.find("https://") == 0);
            return true;
        }
    }
    return false;
}

bool StartPageProjectCatalog::SetWatched(const std::string& connectionString, bool watched) {
    for (auto& e : m_recent) {
        if (e.connectionString == connectionString) {
            e.isWatched = watched;
            return true;
        }
    }
    for (auto& e : m_pinned) {
        if (e.connectionString == connectionString) {
            e.isWatched = watched;
            return true;
        }
    }
    return false;
}

bool StartPageProjectCatalog::RemoveFromRecent(const std::string& connectionString) {
    if (GetTotalProjectLinksCount() < kMinTotalProjectsToRemoveLink) {
        return false;
    }
    auto it = std::find_if(m_recent.begin(), m_recent.end(),
                           [&](const ProjectLinkEntry& e) { return e.connectionString == connectionString; });
    if (it == m_recent.end()) return false;
    m_recent.erase(it);
    return true;
}

bool StartPageProjectCatalog::RemoveFromPinned(const std::string& connectionString) {
    if (GetTotalProjectLinksCount() < kMinTotalProjectsToRemoveLink) {
        return false;
    }
    auto it = std::find_if(m_pinned.begin(), m_pinned.end(),
                           [&](const ProjectLinkEntry& e) { return e.connectionString == connectionString; });
    if (it == m_pinned.end()) return false;
    m_pinned.erase(it);
    return true;
}

std::vector<ProjectLinkEntry> StartPageProjectCatalog::GetRecentProjects(
    RecentSortOrder order,
    bool onlyWatched) const {
    std::vector<ProjectLinkEntry> result;
    for (const auto& e : m_recent) {
        if (!onlyWatched || e.isWatched) {
            result.push_back(e);
        }
    }
    if (order == RecentSortOrder::Alphabetical) {
        std::sort(result.begin(), result.end(),
                  [](const ProjectLinkEntry& a, const ProjectLinkEntry& b) {
                      return toLowerCopy(a.name) < toLowerCopy(b.name);
                  });
    }
    return result;
}

bool DesktopAnalysisService::MatchesDiagramFilter(const Element& elem,
                                                  const DiagramFilterCriteria& criteria,
                                                  Repository& /*repo*/) {
    if (criteria.field == DiagramFilterField::FilteringOff || criteria.value.empty()) {
        return true;
    }

    switch (criteria.field) {
        case DiagramFilterField::Name:
            return matchString(elem.GetName(), criteria.value, criteria.op);
        case DiagramFilterField::Alias:
            return matchString(elem.GetAlias(), criteria.value, criteria.op);
        case DiagramFilterField::NameOrAlias:
            return matchString(elem.GetName(), criteria.value, criteria.op) ||
                   matchString(elem.GetAlias(), criteria.value, criteria.op);
        case DiagramFilterField::Author:
            return matchString(elem.GetAuthor(), criteria.value, criteria.op);
        case DiagramFilterField::Notes:
            return matchString(elem.GetNotes(), criteria.value, criteria.op);
        case DiagramFilterField::ObjectType:
            return matchString(to_string(elem.GetType()), criteria.value, criteria.op);
        case DiagramFilterField::Phase:
            return matchString(elem.GetPhase(), criteria.value, criteria.op);
        case DiagramFilterField::Version:
            return matchString(elem.GetVersion(), criteria.value, criteria.op);
        case DiagramFilterField::Status:
            return matchString(elem.GetStatus(), criteria.value, criteria.op);
        case DiagramFilterField::Stereotype:
            return matchString(elem.GetStereotype(), criteria.value, criteria.op);
        case DiagramFilterField::Relationships: {
            if (criteria.referenceElementId <= 0) return true;
            if (elem.GetElementID() == criteria.referenceElementId) return true;
            for (size_t i = 0; i < elem.GetConnectors().size(); ++i) {
                auto c = elem.GetConnectors().GetAt(static_cast<short>(i));
                if (c && (c->GetClientID() == criteria.referenceElementId ||
                          c->GetSupplierID() == criteria.referenceElementId)) {
                    return true;
                }
            }
            return false;
        }
        default:
            return matchString(elem.GetName(), criteria.value, criteria.op);
    }
}

TraceNode DesktopAnalysisService::BuildTraceabilityTree(int rootElementId,
                                                        Repository& repo,
                                                        int maxDepth) {
    TraceNode root;
    auto elem = repo.GetElementByID(rootElementId);
    if (!elem) return root;

    root.elementId = elem->GetElementID();
    root.elementName = elem->GetName();
    root.elementType = to_string(elem->GetType());
    root.relationLabel = "Focus Element";

    if (maxDepth <= 0) return root;

    for (size_t i = 0; i < elem->GetElements().size(); ++i) {
        auto child = elem->GetElements().GetAt(static_cast<short>(i));
        if (!child) continue;
        TraceNode childNode;
        childNode.elementId = child->GetElementID();
        childNode.elementName = child->GetName();
        childNode.elementType = to_string(child->GetType());
        childNode.relationLabel = "owns";
        root.children.push_back(childNode);
    }

    for (size_t i = 0; i < elem->GetConnectors().size(); ++i) {
        auto conn = elem->GetConnectors().GetAt(static_cast<short>(i));
        if (!conn) continue;

        bool isOutgoing = (conn->GetClientID() == elem->GetElementID());
        int otherId = isOutgoing ? conn->GetSupplierID() : conn->GetClientID();
        auto otherElem = repo.GetElementByID(otherId);
        if (!otherElem) continue;

        std::string relLabel = "associated with";
        switch (conn->GetType()) {
            case ConnectorType::Generalization:
                relLabel = isOutgoing ? "generalizes" : "specialized by";
                break;
            case ConnectorType::Realization:
                relLabel = isOutgoing ? "realizes" : "realized by";
                break;
            case ConnectorType::Dependency:
                relLabel = isOutgoing ? "depends on" : "needed by";
                break;
            case ConnectorType::Aggregation:
            case ConnectorType::Composition:
                relLabel = isOutgoing ? "part of" : "composed of";
                break;
            default:
                relLabel = isOutgoing ? "links to" : "linked from";
                break;
        }

        TraceNode relatedNode = (maxDepth > 1)
            ? BuildTraceabilityTree(otherElem->GetElementID(), repo, maxDepth - 1)
            : TraceNode{otherElem->GetElementID(), otherElem->GetName(), to_string(otherElem->GetType()), relLabel, {}};
        relatedNode.relationLabel = relLabel;
        root.children.push_back(std::move(relatedNode));
    }

    return root;
}

std::vector<InspectorCategoryGroup> DesktopAnalysisService::BuildInspectorDetails(
    const Element& elem,
    Repository& repo) {
    std::vector<InspectorCategoryGroup> groups = {
        {"Attributes", {}},
        {"Operations", {}},
        {"Tagged Values", {}},
        {"Relationships", {}},
        {"Requirements", {}},
        {"Constraints", {}},
        {"Scenarios", {}},
        {"Maintenance", {}},
        {"Tests", {}},
        {"Resources", {}},
        {"Files", {}}
    };

    for (size_t i = 0; i < elem.GetAttributes().size(); ++i) {
        if (auto a = elem.GetAttributes().GetAt(static_cast<short>(i))) {
            groups[0].items.push_back(a->GetName() + " : " + a->GetType());
        }
    }
    for (size_t i = 0; i < elem.GetMethods().size(); ++i) {
        if (auto m = elem.GetMethods().GetAt(static_cast<short>(i))) {
            groups[1].items.push_back(m->GetName() + "() : " + m->GetReturnType());
        }
    }
    for (size_t i = 0; i < elem.GetTaggedValues().size(); ++i) {
        if (auto tv = elem.GetTaggedValues().GetAt(static_cast<short>(i))) {
            groups[2].items.push_back(tv->GetName() + " = " + tv->GetValue());
        }
    }
    for (size_t i = 0; i < elem.GetConnectors().size(); ++i) {
        if (auto c = elem.GetConnectors().GetAt(static_cast<short>(i))) {
            int targetId = (c->GetClientID() == elem.GetElementID()) ? c->GetSupplierID() : c->GetClientID();
            auto target = repo.GetElementByID(targetId);
            std::string targetName = target ? target->GetName() : ("ID " + std::to_string(targetId));
            groups[3].items.push_back(to_string(c->GetType()) + " -> " + targetName);
        }
    }

    return groups;
}

std::vector<RelationshipRow> DesktopAnalysisService::BuildElementRelationships(
    const Element& elem,
    const Diagram* activeDiagram,
    Repository& repo) {
    std::vector<RelationshipRow> rows;

    std::set<int> diagramElementIds;
    std::map<int, bool> diagramLinkVisibility;
    if (activeDiagram) {
        for (size_t i = 0; i < activeDiagram->GetDiagramObjects().size(); ++i) {
            if (auto dobj = activeDiagram->GetDiagramObjects().GetAt(static_cast<short>(i))) {
                diagramElementIds.insert(dobj->GetElementID());
            }
        }
        for (size_t i = 0; i < activeDiagram->GetDiagramLinks().size(); ++i) {
            if (auto dlink = activeDiagram->GetDiagramLinks().GetAt(static_cast<short>(i))) {
                diagramLinkVisibility[dlink->GetConnectorID()] = !dlink->IsHidden();
            }
        }
    }

    for (size_t i = 0; i < elem.GetConnectors().size(); ++i) {
        auto c = elem.GetConnectors().GetAt(static_cast<short>(i));
        if (!c) continue;

        auto src = repo.GetElementByID(c->GetClientID());
        auto dst = repo.GetElementByID(c->GetSupplierID());

        std::string srcName = src ? src->GetName() : ("ID:" + std::to_string(c->GetClientID()));
        std::string dstName = dst ? dst->GetName() : ("ID:" + std::to_string(c->GetSupplierID()));

        if (activeDiagram && diagramElementIds.find(c->GetClientID()) == diagramElementIds.end()) {
            srcName = "(" + srcName + ")";
        }
        if (activeDiagram && diagramElementIds.find(c->GetSupplierID()) == diagramElementIds.end()) {
            dstName = "(" + dstName + ")";
        }

        std::string viewCol = "Hidden";
        if (activeDiagram) {
            auto itVis = diagramLinkVisibility.find(c->GetConnectorID());
            if (itVis != diagramLinkVisibility.end() && itVis->second) {
                viewCol = "Visible";
            }
        }

        RelationshipRow row;
        row.connectorId = c->GetConnectorID();
        row.relationshipType = to_string(c->GetType());
        row.direction = c->GetDirection();
        row.stereotype = c->GetStereotype();
        row.sourceName = srcName;
        row.targetName = dstName;
        row.targetType = dst ? to_string(dst->GetType()) : "Element";
        row.viewStatus = viewCol;
        rows.push_back(std::move(row));
    }

    return rows;
}

std::string DesktopAnalysisService::BuildSummaryText(const Element* elem,
                                                     const Package* pkg,
                                                     const Attribute* attr,
                                                     const Method* op) {
    std::ostringstream oss;
    if (elem) {
        oss << "Element: " << elem->GetName() << "\n"
            << "Type: " << to_string(elem->GetType()) << "\n"
            << "Stereotype: " << (elem->GetStereotype().empty() ? "<none>" : elem->GetStereotype()) << "\n"
            << "Status: " << elem->GetStatus() << " | Phase: " << elem->GetPhase()
            << " | Version: " << elem->GetVersion() << "\n"
            << "Author: " << elem->GetAuthor() << "\n"
            << "GUID: " << elem->GetElementGUID() << "\n"
            << "Attributes: " << elem->GetAttributes().size()
            << " | Operations: " << elem->GetMethods().size()
            << " | Connectors: " << elem->GetConnectors().size() << "\n\n"
            << (elem->GetNotes().empty() ? "No additional notes recorded." : elem->GetNotes());
    } else if (pkg) {
        oss << "Package: " << pkg->GetName() << "\n"
            << "Version: " << pkg->GetVersion() << " | UML: " << pkg->GetUMLVersion() << "\n"
            << "GUID: " << pkg->GetPackageGUID() << "\n"
            << "Child Packages: " << pkg->GetPackages().size()
            << " | Elements: " << pkg->GetElements().size()
            << " | Diagrams: " << pkg->GetDiagrams().size() << "\n\n"
            << pkg->GetNotes();
    } else if (attr) {
        oss << "Attribute: " << attr->GetName() << " : " << attr->GetType() << "\n"
            << "Default: " << attr->GetDefault() << "\n"
            << attr->GetNotes();
    } else if (op) {
        oss << "Operation: " << op->GetName() << "() : " << op->GetReturnType() << "\n"
            << op->GetNotes();
    } else {
        oss << "Select an Element, Package, Attribute, or Operation to view its read-only summary.";
    }
    return oss.str();
}

RelationshipMatrixResult DesktopAnalysisService::BuildRelationshipMatrix(
    int sourcePackageId,
    int targetPackageId,
    const std::string& sourceTypeFilter,
    const std::string& targetTypeFilter,
    const std::string& connectorTypeFilter,
    MatrixDirectionFilter directionFilter,
    const std::map<std::pair<int, int>, std::string>& customOverlays,
    Repository& repo) {
    RelationshipMatrixResult res;

    auto allElems = CollectAllElements(repo);
    for (const auto& e : allElems) {
        if (!e) continue;
        bool matchSrcPkg = (sourcePackageId <= 0 || e->GetPackageID() == sourcePackageId);
        bool matchDstPkg = (targetPackageId <= 0 || e->GetPackageID() == targetPackageId);
        bool matchSrcType = (sourceTypeFilter.empty() || sourceTypeFilter == "<All>" ||
                             to_string(e->GetType()) == sourceTypeFilter);
        bool matchDstType = (targetTypeFilter.empty() || targetTypeFilter == "<All>" ||
                             to_string(e->GetType()) == targetTypeFilter);

        if (matchSrcPkg && matchSrcType) res.sourceElements.push_back(e);
        if (matchDstPkg && matchDstType) res.targetElements.push_back(e);
    }

    res.cells.assign(res.sourceElements.size(),
                     std::vector<std::string>(res.targetElements.size(), ""));

    for (size_t r = 0; r < res.sourceElements.size(); ++r) {
        auto src = res.sourceElements[r];
        for (size_t c = 0; c < res.targetElements.size(); ++c) {
            auto dst = res.targetElements[c];

            auto ovIt = customOverlays.find({src->GetElementID(), dst->GetElementID()});
            if (ovIt != customOverlays.end() && !ovIt->second.empty()) {
                res.cells[r][c] = ovIt->second;
                continue;
            }

            bool hasForward = false;
            bool hasBackward = false;

            for (size_t k = 0; k < src->GetConnectors().size(); ++k) {
                auto conn = src->GetConnectors().GetAt(static_cast<short>(k));
                if (!conn) continue;
                if (!connectorTypeFilter.empty() && connectorTypeFilter != "<All>" &&
                    to_string(conn->GetType()) != connectorTypeFilter) {
                    continue;
                }
                if (conn->GetClientID() == src->GetElementID() &&
                    conn->GetSupplierID() == dst->GetElementID()) {
                    hasForward = true;
                }
                if (conn->GetClientID() == dst->GetElementID() &&
                    conn->GetSupplierID() == src->GetElementID()) {
                    hasBackward = true;
                }
            }

            if (directionFilter == MatrixDirectionFilter::SourceToTarget && hasForward) {
                res.cells[r][c] = "↑";
            } else if (directionFilter == MatrixDirectionFilter::TargetToSource && hasBackward) {
                res.cells[r][c] = "←";
            } else if (directionFilter == MatrixDirectionFilter::Both) {
                if (hasForward && hasBackward) res.cells[r][c] = "↔";
                else if (hasForward) res.cells[r][c] = "↑";
                else if (hasBackward) res.cells[r][c] = "←";
            }
        }
    }

    return res;
}

namespace {
void buildSpecRowsRecursive(const Element& elem,
                            const std::string& prefix,
                            bool enableLevelNumbering,
                            std::vector<SpecificationRow>& out) {
    SpecificationRow row;
    row.elementId = elem.GetElementID();
    row.levelNumber = enableLevelNumbering ? prefix : "";
    row.name = elem.GetName();
    row.type = to_string(elem.GetType());
    row.stereotype = elem.GetStereotype();
    row.status = elem.GetStatus();
    row.phase = elem.GetPhase();
    row.version = elem.GetVersion();
    row.author = elem.GetAuthor();
    row.notes = elem.GetNotes();
    out.push_back(row);

    for (size_t i = 0; i < elem.GetElements().size(); ++i) {
        if (auto child = elem.GetElements().GetAt(static_cast<short>(i))) {
            std::string childPrefix = prefix + "." + std::to_string(i + 1);
            buildSpecRowsRecursive(*child, childPrefix, enableLevelNumbering, out);
        }
    }
}
} // namespace

std::vector<SpecificationRow> DesktopAnalysisService::BuildPackageSpecification(
    const Package& pkg,
    bool enableLevelNumbering) {
    std::vector<SpecificationRow> rows;
    for (size_t i = 0; i < pkg.GetElements().size(); ++i) {
        if (auto elem = pkg.GetElements().GetAt(static_cast<short>(i))) {
            std::string prefix = std::to_string(i + 1) + ".0";
            buildSpecRowsRecursive(*elem, prefix, enableLevelNumbering, rows);
        }
    }
    return rows;
}

std::vector<SearchResultRow> DesktopAnalysisService::RunModelSearch(
    PredefinedSearchType searchType,
    const std::string& searchTerm,
    Repository& repo) {
    std::vector<SearchResultRow> results;
    std::string q = toLowerCopy(searchTerm);

    auto packages = CollectAllPackages(repo);
    std::map<int, std::string> pkgNames;
    for (const auto& p : packages) {
        if (p) pkgNames[p->GetPackageID()] = p->GetName();
    }

    if (searchType == PredefinedSearchType::RecentlyModifiedDiagrams) {
        for (const auto& d : CollectAllDiagrams(repo)) {
            if (!d) continue;
            if (!q.empty() && toLowerCopy(d->GetName()).find(q) == std::string::npos) continue;
            SearchResultRow row;
            row.id = d->GetDiagramID();
            row.isDiagram = true;
            row.name = d->GetName();
            row.objectType = to_string(d->GetType()) + " Diagram";
            row.stereotype = d->GetStereotype();
            row.status = "Active";
            row.author = d->GetAuthor();
            row.version = d->GetVersion();
            row.packageName = pkgNames[d->GetPackageID()];
            results.push_back(std::move(row));
        }
        return results;
    }

    for (const auto& e : CollectAllElements(repo)) {
        if (!e) continue;
        bool matchesTerm = q.empty() ||
                           toLowerCopy(e->GetName()).find(q) != std::string::npos ||
                           toLowerCopy(e->GetStereotype()).find(q) != std::string::npos ||
                           toLowerCopy(e->GetNotes()).find(q) != std::string::npos;
        if (!matchesTerm) continue;

        bool matchesType = true;
        switch (searchType) {
            case PredefinedSearchType::ClassesAndInterfaces:
                matchesType = (e->GetType() == ObjectType::Class ||
                               e->GetType() == ObjectType::Interface ||
                               e->GetType() == ObjectType::Enumeration);
                break;
            case PredefinedSearchType::ComponentsAndNodes:
                matchesType = (e->GetType() == ObjectType::Component ||
                               e->GetType() == ObjectType::Node ||
                               e->GetType() == ObjectType::Port ||
                               e->GetType() == ObjectType::Artifact);
                break;
            case PredefinedSearchType::UseCasesAndActors:
                matchesType = (e->GetType() == ObjectType::UseCase ||
                               e->GetType() == ObjectType::Actor);
                break;
            case PredefinedSearchType::UnconnectedElements:
                matchesType = e->GetConnectors().empty();
                break;
            case PredefinedSearchType::ElementsWithNotes:
                matchesType = !e->GetNotes().empty();
                break;
            default:
                matchesType = true;
                break;
        }
        if (!matchesType) continue;

        SearchResultRow row;
        row.id = e->GetElementID();
        row.isDiagram = false;
        row.name = e->GetName();
        row.objectType = to_string(e->GetType());
        row.stereotype = e->GetStereotype();
        row.status = e->GetStatus();
        row.author = e->GetAuthor();
        row.version = e->GetVersion();
        row.packageName = pkgNames[e->GetPackageID()];
        results.push_back(std::move(row));
    }

    return results;
}

std::vector<std::shared_ptr<Package>> DesktopAnalysisService::CollectAllPackages(Repository& repo) {
    std::vector<std::shared_ptr<Package>> out;
    for (size_t i = 0; i < repo.GetModels().size(); ++i) {
        collectPackagesRecursive(repo.GetModels().GetAt(static_cast<short>(i)), out);
    }
    if (out.empty() && repo.IsOpen()) {
        std::string xml = repo.SQLQuery("SELECT Package_ID FROM t_package ORDER BY Package_ID ASC;");
        size_t pos = 0;
        while ((pos = xml.find("<Package_ID>", pos)) != std::string::npos) {
            pos += 12;
            size_t end = xml.find("</Package_ID>", pos);
            if (end != std::string::npos) {
                int id = std::stoi(xml.substr(pos, end - pos));
                if (auto p = repo.GetPackageByID(id)) {
                    out.push_back(p);
                }
                pos = end + 13;
            }
        }
    }
    return out;
}

std::vector<std::shared_ptr<Element>> DesktopAnalysisService::CollectAllElements(Repository& repo) {
    std::vector<std::shared_ptr<Element>> out;
    for (const auto& p : CollectAllPackages(repo)) {
        if (!p) continue;
        for (size_t i = 0; i < p->GetElements().size(); ++i) {
            if (auto e = p->GetElements().GetAt(static_cast<short>(i))) {
                out.push_back(e);
            }
        }
    }
    if (out.empty() && repo.IsOpen()) {
        std::string xml = repo.SQLQuery("SELECT Object_ID FROM t_object ORDER BY Object_ID ASC;");
        size_t pos = 0;
        while ((pos = xml.find("<Object_ID>", pos)) != std::string::npos) {
            pos += 11;
            size_t end = xml.find("</Object_ID>", pos);
            if (end != std::string::npos) {
                int id = std::stoi(xml.substr(pos, end - pos));
                if (auto e = repo.GetElementByID(id)) {
                    out.push_back(e);
                }
                pos = end + 12;
            }
        }
    }
    return out;
}

std::vector<std::shared_ptr<Diagram>> DesktopAnalysisService::CollectAllDiagrams(Repository& repo) {
    std::vector<std::shared_ptr<Diagram>> out;
    for (const auto& p : CollectAllPackages(repo)) {
        if (!p) continue;
        for (size_t i = 0; i < p->GetDiagrams().size(); ++i) {
            if (auto d = p->GetDiagrams().GetAt(static_cast<short>(i))) {
                out.push_back(d);
            }
        }
    }
    if (out.empty() && repo.IsOpen()) {
        std::string xml = repo.SQLQuery("SELECT Diagram_ID FROM t_diagram ORDER BY Diagram_ID ASC;");
        size_t pos = 0;
        while ((pos = xml.find("<Diagram_ID>", pos)) != std::string::npos) {
            pos += 12;
            size_t end = xml.find("</Diagram_ID>", pos);
            if (end != std::string::npos) {
                int id = std::stoi(xml.substr(pos, end - pos));
                if (auto d = repo.GetDiagramByID(id)) {
                    out.push_back(d);
                }
                pos = end + 13;
            }
        }
    }
    return out;
}

} // namespace ea::desktop
