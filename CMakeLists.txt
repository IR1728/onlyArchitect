cmake_minimum_required(VERSION 3.20)
project(onlyArchitect VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Compiler flags
if (MSVC)
    add_compile_options(/W4 /utf-8 /EHsc)
    add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(-Wall -Wextra -Wpedantic -O2)
endif()

option(ONLYARCHITECT_ASAN "Build with AddressSanitizer (memory-safety checks for the tests)" OFF)
if (ONLYARCHITECT_ASAN AND NOT MSVC)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
endif()

# Cross-platform dependencies (Linux, macOS Homebrew, Windows vcpkg / MSYS2)
find_package(PkgConfig QUIET)

# SQLite3
find_package(SQLite3 QUIET)
if (SQLite3_FOUND)
    set(ONLYARCHITECT_SQLITE3_TARGET SQLite::SQLite3)
elseif (PkgConfig_FOUND)
    pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)
    set(ONLYARCHITECT_SQLITE3_TARGET PkgConfig::SQLITE3)
else()
    message(FATAL_ERROR "SQLite3 not found. Install libsqlite3-dev (Linux), sqlite (Homebrew), or sqlite3 (vcpkg/MSYS2).")
endif()

# Cairo
find_package(unofficial-cairo CONFIG QUIET)
if (TARGET unofficial::cairo::cairo)
    set(ONLYARCHITECT_CAIRO_TARGET unofficial::cairo::cairo)
elseif (PkgConfig_FOUND)
    pkg_check_modules(CAIRO QUIET IMPORTED_TARGET cairo)
    if (CAIRO_FOUND)
        set(ONLYARCHITECT_CAIRO_TARGET PkgConfig::CAIRO)
    endif()
endif()
if (NOT ONLYARCHITECT_CAIRO_TARGET)
    find_path(CAIRO_INCLUDE_DIR cairo.h PATH_SUFFIXES cairo)
    find_library(CAIRO_LIBRARY NAMES cairo libcairo)
    if (CAIRO_INCLUDE_DIR AND CAIRO_LIBRARY)
        add_library(onlyarchitect_cairo INTERFACE)
        target_include_directories(onlyarchitect_cairo INTERFACE ${CAIRO_INCLUDE_DIR})
        target_link_libraries(onlyarchitect_cairo INTERFACE ${CAIRO_LIBRARY})
        set(ONLYARCHITECT_CAIRO_TARGET onlyarchitect_cairo)
    else()
        message(FATAL_ERROR "Cairo not found. Install libcairo2-dev (Linux), cairo (Homebrew), or cairo (vcpkg/MSYS2).")
    endif()
endif()

find_package(Qt6 COMPONENTS Widgets Svg SvgWidgets Network REQUIRED)

# Core Library
set(ONLYARCHITECT_SOURCES
    src/core/Types.cpp
    src/core/Utils.cpp
    src/model/Diagram.cpp
    src/model/Element.cpp
    src/model/Package.cpp
    src/repository/SqliteStorageEngine.cpp
    src/repository/EapxImporter.cpp
    src/repository/Repository.cpp
    src/diagram/DiagramRenderer.cpp
    src/diagram/DiagramLayout.cpp
    src/diagram/UmlDiagramKinds.cpp
    src/diagram/DiagramPalette.cpp
    src/diagram/SequenceModel.cpp
    src/xml/Xml.cpp
    src/xmi/XmiMapping.cpp
    src/xmi/XmiExport.cpp
    src/xmi/XmiImport.cpp
    src/editor/ElementSnapshot.cpp
    src/editor/FeatureText.cpp
    src/editor/CanvasGeometry.cpp
    src/editor/EditCommands.cpp
    src/editor/ModelPatternService.cpp
    src/simulation/SimulationEngine.cpp
    src/sync/SyncProtocol.cpp
    src/validation/ConnectionRules.cpp
    src/validation/ModelValidator.cpp
    src/export/Exporters.cpp
    src/export/ModelDocExporter.cpp
    src/desktop/DesktopAnalysisService.cpp
    src/desktop/CollaborationService.cpp
)

add_library(onlyarchitect_core STATIC ${ONLYARCHITECT_SOURCES})

target_include_directories(onlyarchitect_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(onlyarchitect_core PUBLIC
    ${ONLYARCHITECT_SQLITE3_TARGET}
    ${ONLYARCHITECT_CAIRO_TARGET}
)

# CLI Binary
add_executable(onlyarchitect cli/main.cpp)
target_link_libraries(onlyarchitect PRIVATE onlyarchitect_core)

# Tests Binary
add_executable(test_onlyarchitect tests/test_repo.cpp)
target_link_libraries(test_onlyarchitect PRIVATE onlyarchitect_core)

# Editor engine tests: core only, no Qt (enforces that ea::editor does not depend on the GUI)
add_executable(test_editor tests/test_editor.cpp)
target_link_libraries(test_editor PRIVATE onlyarchitect_core)

add_executable(test_validation tests/test_validation.cpp)
target_link_libraries(test_validation PRIVATE onlyarchitect_core)

add_executable(test_palette tests/test_palette.cpp)
target_link_libraries(test_palette PRIVATE onlyarchitect_core)

add_executable(test_features tests/test_features.cpp)
target_link_libraries(test_features PRIVATE onlyarchitect_core)

add_executable(test_sequence tests/test_sequence.cpp)
target_link_libraries(test_sequence PRIVATE onlyarchitect_core)

add_executable(test_xmi tests/test_xmi.cpp)
target_link_libraries(test_xmi PRIVATE onlyarchitect_core)

add_executable(test_canvas_geometry tests/test_canvas_geometry.cpp)
target_link_libraries(test_canvas_geometry PRIVATE onlyarchitect_core)

add_executable(test_simulation_and_patterns tests/test_simulation_and_patterns.cpp)
target_link_libraries(test_simulation_and_patterns PRIVATE onlyarchitect_core)

add_executable(test_collaboration tests/test_collaboration.cpp)
target_link_libraries(test_collaboration PRIVATE onlyarchitect_core)

# GUI Binary
set(ONLYARCHITECT_GUI_CORE_SOURCES
    gui/ElementDrag.hpp
    gui/SequenceSymbols.hpp
    gui/SequenceSymbols.cpp
    gui/ModelEditSession.hpp
    gui/ModelEditSession.cpp
    gui/DarkTheme.hpp
    gui/DarkTheme.cpp
    gui/M3Icons.hpp
    gui/M3Icons.cpp
    gui/UmlIcons.hpp
    gui/UmlIcons.cpp
    gui/UmlSymbolPainter.hpp
    gui/UmlSymbolPainter.cpp
    gui/resources/icons.qrc
    gui/MainWindow.hpp
    gui/MainWindow.cpp
    gui/RibbonBarWidget.hpp
    gui/RibbonBarWidget.cpp
    gui/BreadcrumbBarWidget.hpp
    gui/BreadcrumbBarWidget.cpp
    gui/StartPageWidget.hpp
    gui/StartPageWidget.cpp
    gui/ToolboxWidget.hpp
    gui/ToolboxWidget.cpp
    gui/NotesWidget.hpp
    gui/NotesWidget.cpp
    gui/ModelBuilderWidget.hpp
    gui/ModelBuilderWidget.cpp
    gui/WelcomeWidget.hpp
    gui/WelcomeWidget.cpp
    gui/CodeGeneratorWidget.hpp
    gui/CodeGeneratorWidget.cpp
    gui/SimulationWidget.hpp
    gui/SimulationWidget.cpp
    gui/WhiteboardWidget.hpp
    gui/WhiteboardWidget.cpp
    gui/ModelTreeWidget.hpp
    gui/ModelTreeWidget.cpp
    gui/DiagramView.hpp
    gui/DiagramView.cpp
    gui/UndoableEdit.hpp
    gui/UndoableEdit.cpp
    gui/NewDiagramDialog.hpp
    gui/NewDiagramDialog.cpp
    gui/ValidationResultsWidget.hpp
    gui/ValidationResultsWidget.cpp
    gui/DiagramElementItem.hpp
    gui/DiagramElementItem.cpp
    gui/DiagramConnectorItem.hpp
    gui/DiagramConnectorItem.cpp
    gui/PropertiesWidget.hpp
    gui/PropertiesWidget.cpp
    gui/EditElementDialog.hpp
    gui/EditElementDialog.cpp
    gui/EditConnectorDialog.hpp
    gui/EditConnectorDialog.cpp
    gui/EapToQeaTransferDialog.hpp
    gui/EapToQeaTransferDialog.cpp
    gui/SqlConsoleWidget.hpp
    gui/SqlConsoleWidget.cpp
    gui/DesktopToolsWidgets.hpp
    gui/DesktopToolsWidgets.cpp
    gui/TeamSyncServer.hpp
    gui/TeamSyncServer.cpp
    gui/TeamSyncClient.hpp
    gui/TeamSyncClient.cpp
    gui/ServerConnectionDialog.hpp
    gui/ServerConnectionDialog.cpp
    gui/CollaborationWidgets.hpp
    gui/CollaborationWidgets.cpp
    gui/WorkspaceLayouts.hpp
    gui/WorkspaceLayouts.cpp
)

# Material Symbols Rounded variable font (FILL, GRAD, opsz, wght), loaded at runtime by
# gui/M3Icons.cpp from <exe dir>/fonts or <prefix>/share/onlyarchitect/fonts. Kept out of the
# qrc because every GUI test target compiles the resources and the font is ~15 MB.
set(ONLYARCHITECT_FONT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/gui/resources/fonts)
file(COPY ${ONLYARCHITECT_FONT_DIR}/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/fonts)

add_library(onlyarchitect_gui_core STATIC ${ONLYARCHITECT_GUI_CORE_SOURCES})
target_include_directories(onlyarchitect_gui_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/gui
)
target_link_libraries(onlyarchitect_gui_core PUBLIC
    onlyarchitect_core
    Qt6::Widgets
    Qt6::Svg
    Qt6::SvgWidgets
    Qt6::Network
)

add_executable(onlyarchitect-gui gui/main.cpp)
target_link_libraries(onlyarchitect-gui PRIVATE onlyarchitect_gui_core)
if (WIN32)
    set_target_properties(onlyarchitect-gui PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
add_custom_command(TARGET onlyarchitect-gui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${ONLYARCHITECT_FONT_DIR} $<TARGET_FILE_DIR:onlyarchitect-gui>/fonts
)

# GUI Tests
add_executable(test_gui tests/test_gui.cpp)
target_link_libraries(test_gui PRIVATE onlyarchitect_gui_core)

add_executable(test_theme tests/test_theme.cpp)
target_link_libraries(test_theme PRIVATE onlyarchitect_gui_core)

add_executable(test_views tests/test_views.cpp)
target_link_libraries(test_views PRIVATE onlyarchitect_gui_core)

add_executable(test_undo tests/test_undo.cpp)
target_link_libraries(test_undo PRIVATE onlyarchitect_gui_core)

add_executable(test_new_diagram tests/test_new_diagram.cpp)
target_link_libraries(test_new_diagram PRIVATE onlyarchitect_gui_core)

add_executable(test_toolbox tests/test_toolbox.cpp)
target_link_libraries(test_toolbox PRIVATE onlyarchitect_gui_core)

add_executable(test_property_editor tests/test_property_editor.cpp)
target_link_libraries(test_property_editor PRIVATE onlyarchitect_gui_core)

add_executable(test_sequence_gui tests/test_sequence_gui.cpp)
target_link_libraries(test_sequence_gui PRIVATE onlyarchitect_gui_core)

add_executable(test_xmi_gui tests/test_xmi_gui.cpp)
target_link_libraries(test_xmi_gui PRIVATE onlyarchitect_gui_core)

add_executable(test_canvas tests/test_canvas.cpp)
target_link_libraries(test_canvas PRIVATE onlyarchitect_gui_core)

add_executable(test_design_workflows tests/test_design_workflows.cpp)
target_link_libraries(test_design_workflows PRIVATE onlyarchitect_gui_core)

add_executable(onlyarchitect-server
    server/main.cpp
    gui/TeamSyncServer.hpp
    gui/TeamSyncServer.cpp
)
target_include_directories(onlyarchitect-server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/gui)
target_link_libraries(onlyarchitect-server PRIVATE onlyarchitect_core Qt6::Network)

add_executable(test_team_sync
    tests/test_team_sync.cpp
    gui/TeamSyncServer.hpp
    gui/TeamSyncServer.cpp
    gui/TeamSyncClient.hpp
    gui/TeamSyncClient.cpp
)
target_include_directories(test_team_sync PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/gui)
target_link_libraries(test_team_sync PRIVATE onlyarchitect_core Qt6::Network)

add_executable(test_manual_compliance tests/test_manual_compliance.cpp)
target_include_directories(test_manual_compliance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(test_manual_compliance PRIVATE onlyarchitect_core)

# CTest integration (runs identically on Linux, Windows, and macOS)
enable_testing()
set(ONLYARCHITECT_ALL_TESTS
    test_onlyarchitect
    test_editor
    test_validation
    test_palette
    test_features
    test_sequence
    test_xmi
    test_canvas_geometry
    test_simulation_and_patterns
    test_collaboration
    test_team_sync
    test_manual_compliance
    test_gui
    test_theme
    test_views
    test_undo
    test_new_diagram
    test_toolbox
    test_property_editor
    test_sequence_gui
    test_xmi_gui
    test_canvas
    test_design_workflows
)
foreach(_test ${ONLYARCHITECT_ALL_TESTS})
    if (MSVC)
        target_compile_options(${_test} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${_test} PRIVATE -UNDEBUG)
    endif()
    add_test(NAME ${_test} COMMAND ${_test})
    set_tests_properties(${_test} PROPERTIES
        ENVIRONMENT "ONLYARCHITECT_FONT_DIR=${ONLYARCHITECT_FONT_DIR}"
    )
endforeach()

# Installation
install(TARGETS onlyarchitect onlyarchitect-gui onlyarchitect_core
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/ea DESTINATION include)
install(DIRECTORY gui/resources/fonts/ DESTINATION share/onlyarchitect/fonts)
