#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include "ea/core/Types.hpp"
#include "ea/model/Connector.hpp"
#include "ea/model/Diagram.hpp"
#include "ea/model/Element.hpp"
#include "ea/model/Package.hpp"
#include "ea/repository/Repository.hpp"

namespace ea::desktop {

// ============================================================================
// 1. Start Page Project Manager
// ============================================================================
struct ProjectLinkEntry {
    std::string name;
    std::string connectionString;
    std::string lastOpenedIsoDate;
    bool isWatched{false};
    bool isCloud{false};
};

enum class RecentSortOrder {
    ChronologicalMostRecentFirst,
    Alphabetical
};

class StartPageProjectCatalog {
public:
    static constexpr size_t kMaxRecentProjects = 10;
    static constexpr size_t kMaxPinnedProjects = 10;
    static constexpr size_t kMinTotalProjectsToRemoveLink = 2;

    void AddOrTouchRecent(const std::string& name,
                          const std::string& connectionString,
                          const std::string& isoDate = "2026-05-28");
    bool PinProject(const std::string& connectionString);
    bool RenameProject(const std::string& connectionString, const std::string& newName);
    bool UpdateConnectionString(const std::string& oldConn, const std::string& newConn);
    bool SetWatched(const std::string& connectionString, bool watched);

    bool RemoveFromRecent(const std::string& connectionString);
    bool RemoveFromPinned(const std::string& connectionString);

    [[nodiscard]] std::vector<ProjectLinkEntry> GetRecentProjects(
        RecentSortOrder order = RecentSortOrder::ChronologicalMostRecentFirst,
        bool onlyWatched = false) const;
    [[nodiscard]] const std::vector<ProjectLinkEntry>& GetPinnedProjects() const noexcept { return m_pinned; }
    [[nodiscard]] size_t GetTotalProjectLinksCount() const noexcept { return m_recent.size() + m_pinned.size(); }

private:
    std::vector<ProjectLinkEntry> m_recent;
    std::vector<ProjectLinkEntry> m_pinned;
};

// ============================================================================
// 2. Diagram Filter Engine
// ============================================================================
enum class DiagramFilterField {
    FilteringOff,
    Relationships,
    Hierarchy,
    NameOrAlias,
    Alias,
    Author,
    DateCreated,
    DateModified,
    Keywords,
    Name,
    Notes,
    ObjectType,
    Phase,
    Version,
    Scope,
    Status,
    Stereotype,
    Selectable,
    Printable
};

enum class DiagramFilterOperator {
    Contains,
    EqualTo,
    NotEqualTo,
    InRange
};

struct DiagramFilterCriteria {
    DiagramFilterField field{DiagramFilterField::FilteringOff};
    DiagramFilterOperator op{DiagramFilterOperator::Contains};
    std::string value;
    int referenceElementId{0};
};

// ============================================================================
// 3. Inspector, Traceability, Relationships & Summary
// ============================================================================
struct TraceNode {
    int elementId{0};
    std::string elementName;
    std::string elementType;
    std::string relationLabel;
    std::vector<TraceNode> children;
};

struct InspectorCategoryGroup {
    std::string categoryName;
    std::vector<std::string> items;
};

struct RelationshipRow {
    int connectorId{0};
    std::string relationshipType;
    std::string direction;
    std::string stereotype;
    std::string sourceName;
    std::string targetName;
    std::string targetType;
    std::string viewStatus;
};

// ============================================================================
// 4. Relationship Matrix & Gap Analysis Matrix
// ============================================================================
enum class MatrixDirectionFilter {
    SourceToTarget,
    TargetToSource,
    Both
};

struct RelationshipMatrixResult {
    std::vector<std::shared_ptr<Element>> sourceElements;
    std::vector<std::shared_ptr<Element>> targetElements;
    std::vector<std::vector<std::string>> cells;
};

struct GapCellEntry {
    std::string gapElementName;
    std::string gapStereotype;
    std::string gapNotes;
};

struct GapAnalysisMatrixResult {
    std::vector<std::shared_ptr<Element>> baselineElements;
    std::vector<std::shared_ptr<Element>> targetElements;
    std::map<std::pair<int, int>, GapCellEntry> gapCells;
};

// ============================================================================
// 5. Specification Manager & Model Search
// ============================================================================
struct SpecificationRow {
    int elementId{0};
    std::string levelNumber;
    std::string name;
    std::string type;
    std::string stereotype;
    std::string status;
    std::string phase;
    std::string version;
    std::string author;
    std::string notes;
};

enum class PredefinedSearchType {
    Simple,
    RecentlyModifiedElements,
    RecentlyModifiedDiagrams,
    ClassesAndInterfaces,
    ComponentsAndNodes,
    UseCasesAndActors,
    UnconnectedElements,
    ElementsWithNotes
};

struct SearchResultRow {
    int id{0};
    bool isDiagram{false};
    std::string name;
    std::string objectType;
    std::string stereotype;
    std::string status;
    std::string author;
    std::string version;
    std::string packageName;
};

class DesktopAnalysisService {
public:
    static bool MatchesDiagramFilter(const Element& elem,
                                     const DiagramFilterCriteria& criteria,
                                     Repository& repo);

    static TraceNode BuildTraceabilityTree(int rootElementId,
                                           Repository& repo,
                                           int maxDepth = 2);

    static std::vector<InspectorCategoryGroup> BuildInspectorDetails(const Element& elem,
                                                                     Repository& repo);

    static std::vector<RelationshipRow> BuildElementRelationships(const Element& elem,
                                                                  const Diagram* activeDiagram,
                                                                  Repository& repo);

    static std::string BuildSummaryText(const Element* elem,
                                        const Package* pkg = nullptr,
                                        const Attribute* attr = nullptr,
                                        const Method* op = nullptr);

    static RelationshipMatrixResult BuildRelationshipMatrix(
        int sourcePackageId,
        int targetPackageId,
        const std::string& sourceTypeFilter,
        const std::string& targetTypeFilter,
        const std::string& connectorTypeFilter,
        MatrixDirectionFilter directionFilter,
        const std::map<std::pair<int, int>, std::string>& customOverlays,
        Repository& repo);

    static std::vector<SpecificationRow> BuildPackageSpecification(
        const Package& pkg,
        bool enableLevelNumbering = true);

    static std::vector<SearchResultRow> RunModelSearch(
        PredefinedSearchType searchType,
        const std::string& searchTerm,
        Repository& repo);

    static std::vector<std::shared_ptr<Package>> CollectAllPackages(Repository& repo);
    static std::vector<std::shared_ptr<Element>> CollectAllElements(Repository& repo);
    static std::vector<std::shared_ptr<Diagram>> CollectAllDiagrams(Repository& repo);
};

} // namespace ea::desktop
