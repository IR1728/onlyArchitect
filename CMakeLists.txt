cmake_minimum_required(VERSION 3.20)
project(OpenEA VERSION 17.2.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Compiler flags
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -O2)
endif()

# Dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
pkg_check_modules(CAIRO REQUIRED cairo)

find_package(Qt6 COMPONENTS Widgets Svg SvgWidgets REQUIRED)

# Core Library
set(OPENEA_SOURCES
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
    src/export/Exporters.cpp
)

add_library(openea_core STATIC ${OPENEA_SOURCES})

target_include_directories(openea_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SQLITE3_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
)

target_link_libraries(openea_core PUBLIC
    ${SQLITE3_LIBRARIES}
    ${CAIRO_LIBRARIES}
)

# CLI Binary
add_executable(openea cli/main.cpp)
target_link_libraries(openea PRIVATE openea_core)

# Tests Binary
add_executable(test_openea tests/test_repo.cpp)
target_link_libraries(test_openea PRIVATE openea_core)

# GUI Binary
set(OPENEA_GUI_CORE_SOURCES
    gui/DarkTheme.hpp
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
    gui/DiagramElementItem.hpp
    gui/DiagramElementItem.cpp
    gui/DiagramConnectorItem.hpp
    gui/DiagramConnectorItem.cpp
    gui/PropertiesWidget.hpp
    gui/PropertiesWidget.cpp
    gui/EditElementDialog.hpp
    gui/EditElementDialog.cpp
    gui/EapToQeaTransferDialog.hpp
    gui/EapToQeaTransferDialog.cpp
    gui/SqlConsoleWidget.hpp
    gui/SqlConsoleWidget.cpp
)

add_executable(openea-gui gui/main.cpp ${OPENEA_GUI_CORE_SOURCES})
target_include_directories(openea-gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/gui)
target_link_libraries(openea-gui PRIVATE
    openea_core
    Qt6::Widgets
    Qt6::Svg
    Qt6::SvgWidgets
)

# GUI Tests
add_executable(test_gui tests/test_gui.cpp ${OPENEA_GUI_CORE_SOURCES})
target_include_directories(test_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/gui)
target_link_libraries(test_gui PRIVATE
    openea_core
    Qt6::Widgets
    Qt6::Svg
    Qt6::SvgWidgets
)

add_executable(test_views tests/test_views.cpp ${OPENEA_GUI_CORE_SOURCES})
target_include_directories(test_views PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/gui)
target_link_libraries(test_views PRIVATE
    openea_core
    Qt6::Widgets
    Qt6::Svg
    Qt6::SvgWidgets
)

add_executable(test_manual_compliance tests/test_manual_compliance.cpp)
target_include_directories(test_manual_compliance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(test_manual_compliance PRIVATE openea_core)

# Installation
install(TARGETS openea openea-gui openea_core
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/ea DESTINATION include)
