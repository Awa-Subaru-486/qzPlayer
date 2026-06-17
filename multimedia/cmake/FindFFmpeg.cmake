include(FindPackageHandleStandardArgs)

if(NOT FFmpeg_FIND_COMPONENTS)
    set(FFmpeg_FIND_COMPONENTS AVCODEC AVFORMAT AVUTIL)
endif()

# --- Find each component ---
foreach(_component ${FFmpeg_FIND_COMPONENTS})
    string(TOLOWER ${_component} _lib)

    # Save state for Android NDK cross-compilation support
    set(_saved_find_root_path "${CMAKE_FIND_ROOT_PATH}")
    set(_saved_inc_mode "${CMAKE_FIND_ROOT_PATH_MODE_INCLUDE}")
    set(_saved_lib_mode "${CMAKE_FIND_ROOT_PATH_MODE_LIBRARY}")

    if(FFMPEG_DIR)
        list(APPEND CMAKE_FIND_ROOT_PATH "${FFMPEG_DIR}")
    endif()
    if(FFMPEG_ROOT)
        list(APPEND CMAKE_FIND_ROOT_PATH "${FFMPEG_ROOT}")
    endif()
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE "BOTH")
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY "BOTH")

    find_path(${_component}_INCLUDE_DIR lib${_lib}/${_lib}.h
        PATHS ${FFMPEG_DIR} ${FFMPEG_ROOT}
        PATH_SUFFIXES include
        NO_DEFAULT_PATH
    )
    find_path(${_component}_INCLUDE_DIR lib${_lib}/${_lib}.h
        PATH_SUFFIXES include
    )

    if(WIN32 AND NOT FFMPEG_SHARED_LIBRARIES)
        set(_saved_suffixes "${CMAKE_FIND_LIBRARY_SUFFIXES}")
        set(CMAKE_FIND_LIBRARY_SUFFIXES "${CMAKE_STATIC_LIBRARY_SUFFIX}")
    endif()

    find_library(${_component}_LIBRARY ${_lib}
        PATHS ${FFMPEG_DIR} ${FFMPEG_ROOT}
        PATH_SUFFIXES lib bin
        NO_DEFAULT_PATH
    )
    find_library(${_component}_LIBRARY ${_lib}
        PATH_SUFFIXES lib bin
    )

    if(WIN32 AND NOT FFMPEG_SHARED_LIBRARIES)
        set(CMAKE_FIND_LIBRARY_SUFFIXES "${_saved_suffixes}")
    endif()

    # Restore state
    set(CMAKE_FIND_ROOT_PATH "${_saved_find_root_path}")
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE "${_saved_inc_mode}")
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY "${_saved_lib_mode}")

    if(${_component}_LIBRARY)
        get_filename_component(${_component}_LIBRARY_DIR ${${_component}_LIBRARY} DIRECTORY)
        set(${_component}_FOUND TRUE)
        set(FFmpeg_${_component}_FOUND TRUE)
    endif()
endforeach()

# --- Resolve dependencies from .pc files ---
function(_ffmpeg_resolve_pc_dependencies _component)
    string(TOLOWER ${_component} _lib)
    set(_lib_dir "${${_component}_LIBRARY_DIR}")

    set(_pc_file "${_lib_dir}/pkgconfig/lib${_lib}.pc")
    if(NOT EXISTS "${_pc_file}")
        set(_pc_file "${_lib_dir}/../lib/pkgconfig/lib${_lib}.pc")
    endif()
    if(NOT EXISTS "${_pc_file}")
        return()
    endif()

    file(READ "${_pc_file}" _pc_content)
    string(REGEX REPLACE ".*Libs:([^\n\r]+).*" "\\1" _libs_line "${_pc_content}")
    string(REGEX MATCHALL "(^| )-l[^ ]+" _deps "${_libs_line}")

    foreach(_dep ${_deps})
        string(REGEX REPLACE "(^| )-l" "" _dep_name "${_dep}")
        if(_dep_name STREQUAL _lib)
            continue()
        endif()

        # On Windows static linking, prefer companion .a files in same directory
        if(WIN32 AND NOT FFMPEG_SHARED_LIBRARIES)
            set(_dep_lib_path "${_lib_dir}/lib${_dep_name}.a")
            if(EXISTS "${_dep_lib_path}")
                target_link_libraries(FFmpeg::${_lib} INTERFACE "${_dep_lib_path}")
                continue()
            endif()
        endif()

        target_link_libraries(FFmpeg::${_lib} INTERFACE "${_dep_name}")
    endforeach()
endfunction()

# --- Create imported targets ---
foreach(_component ${FFmpeg_FIND_COMPONENTS})
    if(NOT ${_component}_FOUND)
        continue()
    endif()

    string(TOLOWER ${_component} _lib)

    if(NOT TARGET FFmpeg::${_lib})
        add_library(FFmpeg::${_lib} INTERFACE IMPORTED)
        target_include_directories(FFmpeg::${_lib} INTERFACE "${${_component}_INCLUDE_DIR}")
        target_link_libraries(FFmpeg::${_lib} INTERFACE "${${_component}_LIBRARY}")
        _ffmpeg_resolve_pc_dependencies(${_component})

        if(UNIX)
            target_link_options(FFmpeg::${_lib} INTERFACE "-Wl,--exclude-libs=lib${_lib}")
        endif()
    endif()

    list(APPEND FFMPEG_INCLUDE_DIRS "${${_component}_INCLUDE_DIR}")
    list(APPEND FFMPEG_LIBRARIES "${${_component}_LIBRARY}")
    list(APPEND FFMPEG_LIBRARY_DIRS "${${_component}_LIBRARY_DIR}")
endforeach()

if(FFMPEG_INCLUDE_DIRS)
    list(REMOVE_DUPLICATES FFMPEG_INCLUDE_DIRS)
endif()
if(FFMPEG_LIBRARY_DIRS)
    list(REMOVE_DUPLICATES FFMPEG_LIBRARY_DIRS)
endif()

find_package_handle_standard_args(FFmpeg
    REQUIRED_VARS FFMPEG_INCLUDE_DIRS FFMPEG_LIBRARIES
    HANDLE_COMPONENTS
)
