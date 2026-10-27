#pragma once

#include <QString>
#include <QColor>

namespace ea::gui {

// Colors directly sampled and matched from the QtUML EA-Lite prototype (proto2.jpeg):
inline const QColor ColorCanvasBg(22, 28, 40);         // #161c28
inline const QColor ColorCanvasGrid(36, 46, 66);       // #242e42
inline const QColor ColorPanelBg(26, 31, 43);          // #1a1f2b
inline const QColor ColorCardBg(32, 38, 52);           // #202634
inline const QColor ColorCardBorder(40, 48, 66);       // #283042
inline const QColor ColorCardHeader(22, 26, 36);       // #161a24
inline const QColor ColorAccentBlue(0, 122, 204);      // #007acc
inline const QColor ColorAccentCyan(56, 189, 248);     // #38bdf8
inline const QColor ColorAccentGreen(16, 185, 129);    // #10b981
inline const QColor ColorAccentYellow(245, 158, 11);   // #f59e0b
inline const QColor ColorAccentPurple(139, 92, 246);   // #8b5cf6
inline const QColor ColorTextPrimary(243, 244, 246);   // #f3f4f6
inline const QColor ColorTextSecondary(156, 163, 175); // #9ca3af

inline QString GetStudioDarkStyleSheet() {
    return QString(R"(
        /* Global Window and Base */
        QMainWindow, QDialog, QWidget#CentralHost {
            background-color: #171c26;
            color: #f3f4f6;
            font-family: 'Segoe UI', 'Inter', -apple-system, sans-serif;
            font-size: 12px;
        }

        /* Top Workspace Mode Selector Bar */
        QTabBar#WorkspaceTabBar {
            background-color: #13161e;
            border-bottom: 2px solid #262c3a;
            qproperty-drawBase: 0;
        }
        QTabBar#WorkspaceTabBar::tab {
            background-color: #1a1e28;
            color: #9ca3af;
            font-size: 12px;
            font-weight: bold;
            padding: 8px 18px;
            margin-right: 4px;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            border: 1px solid #262c3a;
            border-bottom: none;
        }
        QTabBar#WorkspaceTabBar::tab:hover {
            background-color: #242936;
            color: #ffffff;
        }
        QTabBar#WorkspaceTabBar::tab:selected {
            background-color: #262c3a;
            color: #38bdf8;
            border: 1px solid #38bdf8;
            border-bottom: 2px solid #38bdf8;
        }

        /* Standard TabWidget */
        QTabWidget::pane {
            border: 1px solid #2b3140;
            background-color: #1a1e28;
            border-radius: 4px;
        }
        QTabBar::tab {
            background-color: #181b24;
            color: #9ca3af;
            padding: 6px 14px;
            margin-right: 2px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            border: 1px solid #2b3140;
            border-bottom: none;
        }
        QTabBar::tab:hover {
            background-color: #222734;
            color: #f3f4f6;
        }
        QTabBar::tab:selected {
            background-color: #222734;
            color: #38bdf8;
            border-top: 2px solid #007acc;
            font-weight: bold;
        }

        /* Menu Bar */
        QMenuBar {
            background-color: #13161e;
            color: #d1d5db;
            border-bottom: 1px solid #262c3a;
            padding: 2px 6px;
        }
        QMenuBar::item {
            background: transparent;
            padding: 5px 10px;
            border-radius: 4px;
        }
        QMenuBar::item:selected {
            background-color: #242936;
            color: #ffffff;
        }
        QMenu {
            background-color: #1e222d;
            color: #f3f4f6;
            border: 1px solid #343c4e;
            border-radius: 6px;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 24px 6px 20px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background-color: #007acc;
            color: #ffffff;
        }
        QMenu::separator {
            height: 1px;
            background-color: #2e3544;
            margin: 4px 8px;
        }

        /* ToolBar */
        QToolBar {
            background-color: #181b24;
            border-bottom: 1px solid #262c3a;
            spacing: 4px;
            padding: 3px 6px;
        }
        QToolBar::separator {
            width: 1px;
            background-color: #2e3544;
            margin: 4px 6px;
        }
        QToolButton {
            background-color: transparent;
            color: #e5e7eb;
            border-radius: 4px;
            padding: 5px 9px;
            font-size: 11px;
        }
        QToolButton:hover {
            background-color: #242936;
            border: 1px solid #343c4e;
        }
        QToolButton:pressed {
            background-color: #007acc;
            color: white;
        }

        /* Dock Widgets */
        QDockWidget {
            color: #f3f4f6;
            font-weight: bold;
            titlebar-close-icon: url();
            titlebar-normal-icon: url();
        }
        QDockWidget::title {
            background-color: #1c202b;
            padding: 6px 10px;
            border-bottom: 1px solid #2b3140;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
        }

        /* Tree Views & Lists */
        QTreeView, QListView, QTableWidget {
            background-color: #1a1e28;
            color: #f3f4f6;
            border: 1px solid #2b3140;
            border-radius: 4px;
            gridline-color: #242936;
            selection-background-color: #007acc;
            selection-color: #ffffff;
            outline: none;
        }
        QTreeView::item, QListView::item {
            padding: 4px;
            border-radius: 3px;
        }
        QTreeView::item:hover, QListView::item:hover {
            background-color: #242936;
        }
        QTreeView::item:selected, QListView::item:selected {
            background-color: #007acc;
            color: white;
        }
        QHeaderView::section {
            background-color: #1c202b;
            color: #9ca3af;
            padding: 5px 8px;
            border: none;
            border-right: 1px solid #2b3140;
            border-bottom: 1px solid #2b3140;
            font-weight: bold;
            font-size: 11px;
        }

        /* Input Controls */
        QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox {
            background-color: #151821;
            color: #f3f4f6;
            border: 1px solid #323848;
            border-radius: 5px;
            padding: 5px 8px;
            selection-background-color: #007acc;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QComboBox:focus {
            border: 1px solid #38bdf8;
            background-color: #181b24;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QComboBox QAbstractItemView {
            background-color: #1e222d;
            border: 1px solid #343c4e;
            selection-background-color: #007acc;
        }

        /* Buttons */
        QPushButton {
            background-color: #252a38;
            color: #f3f4f6;
            border: 1px solid #343c4e;
            border-radius: 5px;
            padding: 6px 14px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #2e3546;
            border: 1px solid #38bdf8;
            color: #ffffff;
        }
        QPushButton:pressed {
            background-color: #007acc;
            color: white;
        }
        QPushButton#PrimaryBtn {
            background-color: #007acc;
            color: white;
            border: 1px solid #0062a3;
            font-weight: bold;
        }
        QPushButton#PrimaryBtn:hover {
            background-color: #0088e2;
        }

        /* Status Bar */
        QStatusBar {
            background-color: #13161e;
            color: #9ca3af;
            border-top: 1px solid #262c3a;
            font-size: 11px;
        }

        /* Scrollbars */
        QScrollBar:vertical {
            background: #14161f;
            width: 10px;
            margin: 0;
            border-radius: 5px;
        }
        QScrollBar::handle:vertical {
            background: #2e3546;
            min-height: 20px;
            border-radius: 5px;
        }
        QScrollBar::handle:vertical:hover {
            background: #3b82f6;
        }
        QScrollBar:horizontal {
            background: #14161f;
            height: 10px;
            margin: 0;
            border-radius: 5px;
        }
        QScrollBar::handle:horizontal {
            background: #2e3546;
            min-width: 20px;
            border-radius: 5px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #3b82f6;
        }
        QScrollBar::add-line, QScrollBar::sub-line {
            width: 0;
            height: 0;
        }

        /* Progress Bar */
        QProgressBar {
            background-color: #181b24;
            border: 1px solid #2b3140;
            border-radius: 5px;
            text-align: center;
            color: white;
        }
        QProgressBar::chunk {
            background-color: #007acc;
            border-radius: 4px;
        }
    )");
}

} // namespace ea::gui
