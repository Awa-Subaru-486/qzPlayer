if(TARGET qzPlayer)
    return()
endif()

if(NOT TARGET lunasvg)
    set(LUNASVG_BUILD_EXAMPLES OFF)
    set(PLUTOVG_BUILD_EXAMPLES OFF)
    set(BUILD_SHARED_LIBS ON)
    add_subdirectory(3rdparty/lunasvg-3.5.0)
endif()

option(USE_QWINDOWKIT "Enable QWindowKit support (primarily for Windows)" ON)

# Android does not need QWindowKit
if(ANDROID)
    set(USE_QWINDOWKIT OFF CACHE BOOL "Disable QWindowKit on Android" FORCE)
endif()

if(USE_QWINDOWKIT AND NOT TARGET QWKCore)
    set(_qmsetup_src_dir "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/qwindowkit-1.5.0/qmsetup")
    set(_qmsetup_build_dir "${CMAKE_BINARY_DIR}/_qmsetup_build")
    set(_qmsetup_install_dir "${CMAKE_BINARY_DIR}/_qmsetup_install")

    if(NOT EXISTS "${_qmsetup_install_dir}/lib/cmake/qmsetup/qmsetupConfig.cmake")
        message(STATUS "Configuring qmsetup...")
        set(_qmsetup_cmake_args
            -S "${_qmsetup_src_dir}"
            -B "${_qmsetup_build_dir}"
            "-DCMAKE_INSTALL_PREFIX:PATH=${_qmsetup_install_dir}"
            "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
            "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
            -G "${CMAKE_GENERATOR}"
        )
        execute_process(
            COMMAND ${CMAKE_COMMAND} ${_qmsetup_cmake_args}
            RESULT_VARIABLE _qmsetup_result
            OUTPUT_VARIABLE _qmsetup_output
            ERROR_VARIABLE _qmsetup_error
        )
        if(NOT _qmsetup_result EQUAL 0)
            message(FATAL_ERROR "Failed to configure qmsetup:\n${_qmsetup_output}\n${_qmsetup_error}")
        endif()

        message(STATUS "Building qmsetup...")
        execute_process(
            COMMAND ${CMAKE_COMMAND} --build "${_qmsetup_build_dir}" --config ${CMAKE_BUILD_TYPE}
            RESULT_VARIABLE _qmsetup_result
        )
        if(NOT _qmsetup_result EQUAL 0)
            message(FATAL_ERROR "Failed to build qmsetup")
        endif()

        message(STATUS "Installing qmsetup...")
        execute_process(
            COMMAND ${CMAKE_COMMAND} --install "${_qmsetup_build_dir}"
            RESULT_VARIABLE _qmsetup_result
        )
        if(NOT _qmsetup_result EQUAL 0)
            message(FATAL_ERROR "Failed to install qmsetup")
        endif()
        message(STATUS "qmsetup built and installed successfully")
    endif()

    set(qmsetup_DIR "${_qmsetup_install_dir}/lib/cmake/qmsetup")

    set(QWINDOWKIT_BUILD_STATIC OFF)
    set(QWINDOWKIT_BUILD_WIDGETS OFF)
    set(QWINDOWKIT_BUILD_QUICK ON)
    set(QWINDOWKIT_ENABLE_STYLE_AGENT OFF)
    set(QWINDOWKIT_BUILD_EXAMPLES OFF)
    set(QWINDOWKIT_BUILD_DOCUMENTATIONS OFF)
    set(QWINDOWKIT_INSTALL OFF)
    add_subdirectory(3rdparty/qwindowkit-1.5.0)
endif()

if(NOT TARGET qzMultimedia)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../multimedia/cmake")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../multimedia multimedia_build)
endif()

set(QZPLAYER_THEME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../qzTheme" CACHE PATH "Path to qzTheme source directory")
set(QZPLAYER_MD3CONTROLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Controls/md3" CACHE PATH "Path to Md3Controls source directory")

add_library(qzPlayer SHARED)

get_res_path(
    RESOURCE_FILES
    res
    "*.png" "*.svg" "*.jpg" "*.gif"
)

# Platform-specific PlayerWindow.qml
if(ANDROID)
    set(PLAYER_WINDOW_QML mobile/PlayerWindow.qml)
    set(PLATFORM_QML_FILES
        mobile/PlayerControl.qml
        mobile/PlayerTitleBar.qml
    )
    set(ANDROID_UTILS_SOURCES
        utils/AndroidUtils.cpp
        utils/AndroidUtils.hpp
    )
    set(DESKTOP_UTILS_SOURCES)
else()
    set(PLAYER_WINDOW_QML desktop/PlayerWindow.qml)
    set(PLATFORM_QML_FILES
        desktop/PlayerControl.qml
        desktop/PlayerTitleBar.qml
    )
    set(ANDROID_UTILS_SOURCES)
    set(DESKTOP_UTILS_SOURCES
        utils/HotkeyManager.hpp
        utils/HotkeyManager.cpp
        utils/FileDropArea.hpp
        utils/FileDropArea.cpp
    )
endif()

qt_add_qml_module(qzPlayer
    URI "qz.player"
    OUTPUT_DIRECTORY qz/player
    QML_FILES
        ${PLAYER_WINDOW_QML}
        controls/IconButton.qml
        controls/BubbleButton.qml
        controls/KeyButton.qml
        controls/BubbleTip.qml
        controls/PlayerSlider.qml
        controls/SeekFrame.qml
        controls/RoundedImage.qml
        controls/MiniModeOverlay.qml
        controls/LoadingOverlay.qml
        controls/PlayerState.qml
        controls/SlidePanel.qml
        controls/SettingsBubble.qml
        controls/PlaylistPanel.qml
        controls/PlaylistDelegate.qml
        controls/ChapterPanel.qml
        controls/ChapterDelegate.qml
        controls/NotificationBar.qml
        ${PLATFORM_QML_FILES}
    RESOURCES
        ${RESOURCE_FILES}
    DEPENDENCIES
        TARGET qzTheme
        TARGET Md3Controls
        TARGET qzMultimediaQuick
)

# Android: Build jar containing QtAndroidUtils Java class
if(ANDROID)
    set(_player_jar_dir ${CMAKE_CURRENT_BINARY_DIR}/Qt6AndroidPlayer)
    set(_player_jar ${CMAKE_CURRENT_BINARY_DIR}/Qt6AndroidPlayer.jar)
    set(_player_java_src
        ${CMAKE_CURRENT_SOURCE_DIR}/android/jar/src/qz/player/android/QtAndroidUtils.java
    )

    if(NOT QT_ANDROID_JAR)
        set(QT_ANDROID_JAR "${ANDROID_SDK_ROOT}/platforms/android-36/android.jar")
    endif()

    set(_qt_android_jar "")
    if(TARGET Qt6::Core)
        get_target_property(_qt_core_location Qt6::Core LOCATION)
        get_filename_component(_qt_lib_dir "${_qt_core_location}" DIRECTORY)
        get_filename_component(_qt_prefix "${_qt_lib_dir}/.." ABSOLUTE)
        set(_qt_android_jar "${_qt_prefix}/jar/Qt6Android.jar")
    endif()
    if(NOT EXISTS "${_qt_android_jar}")
        string(REPLACE ";" "/" _prefix_first "${CMAKE_PREFIX_PATH}")
        get_filename_component(_qt_prefix "${_prefix_first}" ABSOLUTE)
        set(_qt_android_jar "${_qt_prefix}/jar/Qt6Android.jar")
    endif()

    find_program(JAVAC_EXECUTABLE javac HINTS "${JDK_HOME}/bin" "$ENV{JAVA_HOME}/bin")
    find_program(JAR_EXECUTABLE jar HINTS "${JDK_HOME}/bin" "$ENV{JAVA_HOME}/bin")
    if(NOT JAR_EXECUTABLE)
        message(FATAL_ERROR "jar executable not found. Please set JDK_HOME or JAVA_HOME, or add JDK to PATH.")
    endif()

    add_custom_command(
        OUTPUT ${_player_jar}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_player_jar_dir}"
        COMMAND ${JAVAC_EXECUTABLE} -source 11 -target 11
                -classpath "${QT_ANDROID_JAR};${_qt_android_jar}"
                -d "${_player_jar_dir}"
                ${_player_java_src}
        COMMAND ${JAR_EXECUTABLE} cf "${_player_jar}" -C "${_player_jar_dir}" .
        DEPENDS ${_player_java_src}
        COMMENT "Building Qt6AndroidPlayer.jar"
    )
    add_custom_target(Qt6AndroidPlayer_jar ALL DEPENDS ${_player_jar})
    add_dependencies(qzPlayer Qt6AndroidPlayer_jar)
    set_property(TARGET qzPlayer APPEND PROPERTY
        QT_ANDROID_BUNDLED_JAR_DEPENDENCIES "${_player_jar}"
    )
    set_property(TARGET qzPlayer APPEND PROPERTY
        INTERFACE_QT_ANDROID_BUNDLED_JAR_DEPENDENCIES "${_player_jar}"
    )
endif()

set_target_properties(qzPlayerplugin PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/qz/player"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/qz/player"
)

add_custom_command(TARGET qzPlayerplugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_BINARY_DIR}/qz/player"
        "${CMAKE_BINARY_DIR}/bin/qz/player"
    COMMAND ${CMAKE_COMMAND}
        -DINPUT_FILE="${CMAKE_CURRENT_BINARY_DIR}/qz/player/qmldir"
        -DOUTPUT_FILE="${CMAKE_BINARY_DIR}/bin/qz/player/qmldir"
        -P "${CMAKE_SOURCE_DIR}/cmake/strip_prefer.cmake"
)

target_sources(qzPlayer
    PUBLIC FILE_SET CXX_MODULES
    BASE_DIRS utils
    FILES
        utils/qml_register_types.cppm
        utils/KmeansColors.cppm
        # utils/test_module/Test1Item.cppm
        # utils/test_module/Test2Item.cppm
    PRIVATE
        utils/CoverArtItem.hpp
        utils/CoverArtItem.cpp
        utils/InputMonitor.hpp
        utils/InputMonitor.cpp
        utils/NotificationManager.hpp
        utils/NotificationManager.cpp
        utils/PlayerSet.hpp
        utils/PlayerSet.cpp
        utils/Settings.hpp
        utils/Settings.cpp
        utils/PlaylistModel.hpp
        utils/PlaylistModel.cpp
        utils/SvgRenderer.hpp
        utils/SvgRenderer.cpp
        utils/VulkanWindowInitializer.hpp
        utils/VulkanWindowInitializer.cpp
        ${ANDROID_UTILS_SOURCES}
        ${DESKTOP_UTILS_SOURCES}
)

qt_add_shaders(qzPlayer "player_shaders"
    SILENT BATCHABLE PRECOMPILE OPTIMIZED
    PREFIX "/qz/player"
    FILES
        "shaders/RoundedImage.vert"
        "shaders/RoundedImage.frag"
)

target_include_directories(qzPlayer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

target_include_directories(qzPlayer SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/lunasvg-3.5.0/include
)

# Vulkan SDK include for vulkan.hpp (needed by VulkanWindowInitializer)
if(ANDROID AND DEFINED ENV{VULKAN_SDK})
    target_include_directories(qzPlayer SYSTEM PRIVATE
        $ENV{VULKAN_SDK}/include
    )
endif()

target_compile_definitions(qzPlayer
    PRIVATE
        QZ_PLAYER_BUILD_LIB
)

# 生成模块初始化器 workaround（解决 Windows DLL + C++20/23 模块符号导出问题）
add_cpp_module_consumer(qzPlayer
    MODULES
        qml_register_types_qzPlayer
        KmeansColors
)

target_link_libraries(qzPlayer PUBLIC
    Qt6::Quick
    Qt6::Core
    Qt6::Gui
    Qt6::Concurrent
    lunasvg
    qzTheme_consumer
    Md3Controls
    qzMultimedia
    qzMultimediaQuick_consumer
    qzVulkanContext_consumer
    qzLog
)

if(USE_QWINDOWKIT)
    target_link_libraries(qzPlayer PUBLIC QWKCore QWKQuick)
endif()
