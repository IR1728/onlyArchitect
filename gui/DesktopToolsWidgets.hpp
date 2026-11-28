#pragma once

#include <QWidget>
#include <QDialog>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QCheckBox>
#include <functional>
#include <map>
#include "ea/desktop/DesktopAnalysisService.hpp"
#include "ea/repository/Repository.hpp"

namespace ea::gui {

// 1. Relationship Matrix View
class RelationshipMatrixWidget : public QWidget {
    Q_OBJECT
public:
    explicit RelationshipMatrixWidget(ea::Repository& repo, QWidget* parent = nullptr);
    void refreshMatrix();

signals:
    void elementSelected(int elementId);

private slots:
    void onCellContextMenu(const QPoint& pos);

private:
    ea::Repository& m_repo;
    QComboBox* m_srcPkgCombo{nullptr};
    QComboBox* m_dstPkgCombo{nullptr};
    QComboBox* m_srcTypeCombo{nullptr};
    QComboBox* m_dstTypeCombo{nullptr};
    QComboBox* m_connTypeCombo{nullptr};
    QComboBox* m_dirCombo{nullptr};
    QComboBox* m_overlayCombo{nullptr};
    QTableWidget* m_matrixTable{nullptr};
    ea::desktop::RelationshipMatrixResult m_lastResult;
    std::map<std::pair<int, int>, std::string> m_overlays;
};

// 2. Gap Analysis Matrix View
class GapAnalysisMatrixWidget : public QWidget {
    Q_OBJECT
public:
    explicit GapAnalysisMatrixWidget(ea::Repository& repo, QWidget* parent = nullptr);
    void refreshMatrix();

private:
    ea::Repository& m_repo;
    QComboBox* m_targetPkgCombo{nullptr};
    QComboBox* m_baselinePkgCombo{nullptr};
    QTableWidget* m_gapTable{nullptr};
};

// 3. Specification Manager & Package Browser View
class SpecificationManagerWidget : public QWidget {
    Q_OBJECT
public:
    explicit SpecificationManagerWidget(ea::Repository& repo, QWidget* parent = nullptr);
    void setPackage(int packageId);
    void refreshSpecification();

signals:
    void elementSelected(int elementId);

private:
    ea::Repository& m_repo;
    int m_packageId{0};
    bool m_levelNumbering{true};
    QComboBox* m_pkgCombo{nullptr};
    QLineEdit* m_filterBarEdit{nullptr};
    QCheckBox* m_chkLevelNumbering{nullptr};
    QTableWidget* m_specTable{nullptr};
};

// 4. Model Search Window - Find in Project
class ModelSearchWidget : public QWidget {
    Q_OBJECT
public:
    explicit ModelSearchWidget(ea::Repository& repo, QWidget* parent = nullptr);
    void runSearch(ea::desktop::PredefinedSearchType type, const QString& term = "");

signals:
    void elementDoubleClicked(int elementId);
    void diagramDoubleClicked(int diagramId);

private:
    ea::Repository& m_repo;
    QComboBox* m_searchTypeCombo{nullptr};
    QLineEdit* m_searchTermEdit{nullptr};
    QTableWidget* m_resultsTable{nullptr};
};

// 5. Focus, Traceability, Relationships & Summary Dock
class FocusAndTraceabilityWidget : public QWidget {
    Q_OBJECT
public:
    explicit FocusAndTraceabilityWidget(ea::Repository& repo, QWidget* parent = nullptr);
    void showSummary();
    void inspectElement(int elementId, const ea::Diagram* activeDiagram = nullptr);
    void recordRevisitDiagram(int diagramId, const QString& name);

signals:
    void elementActivated(int elementId);
    void diagramActivated(int diagramId);

private:
    void populateTraceSubtree(QTreeWidgetItem* parentItem, const ea::desktop::TraceNode& node);

    ea::Repository& m_repo;
    QTabWidget* m_tabs{nullptr};
    QTreeWidget* m_traceTree{nullptr};
    QTableWidget* m_relationshipsTable{nullptr};
    QTextEdit* m_summaryEdit{nullptr};
    QTreeWidget* m_focusModelViewsTree{nullptr};
    QLineEdit* m_quickFindEdit{nullptr};
    QTreeWidget* m_revisitTree{nullptr};
};

// 6. Portals Bar Widget
class PortalsBarWidget : public QWidget {
    Q_OBJECT
public:
    explicit PortalsBarWidget(QWidget* parent = nullptr);

signals:
    void portalActionTriggered(const QString& category, const QString& actionName);

private:
    void loadPortalItems(int portalIndex);
    QComboBox* m_portalSelector{nullptr};
    QTreeWidget* m_portalTree{nullptr};
};

// 7. Window Selectors Dialog (Alt+1 .. Alt+7)
class WindowSelectorDialog : public QDialog {
    Q_OBJECT
public:
    enum Selector { Design = 0, Properties, Collaborate, Execute, Construct, Publish, Workspace, SelectorCount };

    // Commands understood by MainWindow::runWindowCommand; "workspace:<name>" applies a layout
    WindowSelectorDialog(int initialTabIndex, const QStringList& userLayouts, QWidget* parent = nullptr);

signals:
    void commandSelected(const QString& commandId);
};

// 8. User Preferences Dialog (Ctrl+F9)
class UserPreferencesDialog : public QDialog {
    Q_OBJECT
public:
    explicit UserPreferencesDialog(QWidget* parent = nullptr);

signals:
    void preferencesApplied(bool showStereotypes,
                            bool sortDiagramsAbovePackages,
                            bool hideStatusBar,
                            int diagramMode);
};

} // namespace ea::gui
