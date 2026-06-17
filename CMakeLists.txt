cmake_minimum_required(VERSION 4.2.1)

project(Player_test VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include(cmake/BasicFunc.cmake)
include(cmake/CppModule.cmake)

auto_add_debug()

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_CXX_FLAGS_INIT -DQT_QML_DEBUG)
endif()

find_package(Qt6 REQUIRED COMPONENTS Quick Core Gui Network Concurrent)
find_package(Qt6 REQUIRED COMPONENTS CorePrivate GuiPrivate QuickPrivate)
find_package(Qt6 QUIET OPTIONAL_COMPONENTS ShaderTools)

qt_standard_project_setup()

set(QT_QML_GENERATE_QMLLS_INI ON)
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0002 NEW)
qt_policy(SET QTP0004 NEW)
qt_policy(SET QTP0005 NEW)

add_subdirectory(examples)
