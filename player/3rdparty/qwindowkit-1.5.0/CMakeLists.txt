cmake_minimum_required(VERSION 3.19)

project(QWindowKit
    VERSION 1.5.0.0
    LANGUAGES CXX
    HOMEPAGE_URL "https://github.com/stdware/qwindowkit"
    DESCRIPTION "Cross-platform window customization framework"
)

# ----------------------------------
# Build Options
# ----------------------------------
option(QWINDOWKIT_BUILD_STATIC "Build static libraries" OFF)
option(QWINDOWKIT_BUILD_WIDGETS "Build widgets module" ON)
option(QWINDOWKIT_BUILD_QUICK "Build quick module" OFF)

option(QWINDOWKIT_FORCE_QT_WINDOW_CONTEXT "Force use Qt Window Context" OFF)
option(QWINDOWKIT_ENABLE_WINDOWS_SYSTEM_BORDERS "Enable system borders on Windows" ON)
option(QWINDOWKIT_ENABLE_STYLE_AGENT "Enable building style agent" ON)

#[[

Detailed Introcuction to Configure Options:

`QWINDOWKIT_ENABLE_WINDOWS_SYSTEM_BORDERS`
  - If you don't want the system borders on Windows 10/11, you can DISABLE this option.
  - If so, the Windows 10 top border issue will disappear. However, part of the client edge
    area will be occupied as the resizing margins.

`QWINDOWKIT_FORCE_QT_WINDOW_CONTEXT`
  - If you want to use pure Qt emulated frameless implementation, you can ENABLE this option.
  - If so, all system native features will be lost.
  - The Qt Window Context is supported on all platforms.

`QWINDOWKIT_ENABLE_STYLE_AGENT`
  - Select whether to exclude the style component by DISABLING this option according to your
    requirements and your Qt version.

#]]

# ----------------------------------
# CMake Settings
# ----------------------------------
if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /manifest:no")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /manifest:no")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /manifest:no")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")

    if(NOT DEFINED CMAKE_DEBUG_POSTFIX)
        set(CMAKE_DEBUG_POSTFIX "d")
    endif()
elseif(MINGW)
    if(NOT DEFINED CMAKE_STATIC_LIBRARY_PREFIX)
        set(CMAKE_STATIC_LIBRARY_PREFIX "")
    endif()

    if(NOT DEFINED CMAKE_SHARED_LIBRARY_PREFIX)
        set(CMAKE_SHARED_LIBRARY_PREFIX "")
    endif()

    if(NOT DEFINED CMAKE_IMPORT_LIBRARY_PREFIX)
        set(CMAKE_IMPORT_LIBRARY_PREFIX "")
    endif()
endif()

if(NOT DEFINED CMAKE_BUILD_TYPE AND NOT DEFINED CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_C_VISIBILITY_PRESET "hidden")
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")

# ----------------------------------
# Project Variables
# ----------------------------------
set(QWINDOWKIT_VERSION ${PROJECT_VERSION})

# ----------------------------------
# Find basic dependencies
# ----------------------------------
find_package(qmsetup REQUIRED)

qm_import(Filesystem)
qm_init_directories()

# ----------------------------------
# Add source modules
# ----------------------------------
add_subdirectory(src)
