cmake_minimum_required(VERSION 3.16)

project(qzMultimedia
    VERSION "6.11.1"
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Suppress Qt private module version-tied warning
set(QT_NO_PRIVATE_MODULE_WARNING ON CACHE BOOL "" FORCE)

# Mark VULKAN_SDK as used to suppress unused variable warning
# (find_package(Vulkan) uses it internally via environment variable)
if(DEFINED VULKAN_SDK)
    set(ENV{VULKAN_SDK} "${VULKAN_SDK}")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Plugin build options (controlled by CMakeUserPresets.json)
option(ENABLE_FFMPEG_PLUGIN "Build FFmpeg multimedia plugin" ON)
option(ENABLE_WINDOWS_PLUGIN "Build Windows multimedia plugin" ON)

find_package(Qt6 6.11.1 CONFIG REQUIRED COMPONENTS Core Gui)
find_package(Qt6 6.11.1 QUIET CONFIG OPTIONAL_COMPONENTS
    Network
    Quick
    Qml
    Concurrent
    ShaderTools
    QuickPrivate
)

if(NOT TARGET Qt6::Gui)
    message(NOTICE "Skipping the build as Qt6::Gui is not available.")
    return()
endif()

add_subdirectory(src)
