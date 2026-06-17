add_subdirectory(${QZPLAYER_THEME_DIR} Theme_build)

add_subdirectory(${QZPLAYER_MD3CONTROLS_DIR} Controls_md3_build)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../player player_build)

set(EXAMPLE_APP_NAME qzPlayerExample)

qt6_add_executable(${EXAMPLE_APP_NAME} main.cpp)

# Platform-specific Main QML file
if(ANDROID)
    set(MAIN_QML_FILE AndroidMain.qml)
else()
    set(MAIN_QML_FILE WindowsMain.qml)
endif()

qt_add_qml_module(${EXAMPLE_APP_NAME}
    URI "qz.example"
    VERSION 0.1
    QML_FILES ${MAIN_QML_FILE}
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
    set_target_properties(${EXAMPLE_APP_NAME} PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
else()
    set_target_properties(${EXAMPLE_APP_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

target_link_libraries(${EXAMPLE_APP_NAME}
    PRIVATE
        Qt6::Quick
        Qt6::Core
        Qt6::Gui
        qzPlayer_consumer
        qzTheme_consumer
)

if(USE_QWINDOWKIT)
    target_compile_definitions(${EXAMPLE_APP_NAME} PRIVATE USE_QWINKIT)
    target_link_libraries(${EXAMPLE_APP_NAME} PRIVATE QWKCore QWKQuick)
endif()

if(WIN32)
    add_custom_command(TARGET ${EXAMPLE_APP_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CXX_COMPILER}/../libc++.dll"
            "$<TARGET_FILE_DIR:${EXAMPLE_APP_NAME}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CXX_COMPILER}/../libunwind.dll"
            "$<TARGET_FILE_DIR:${EXAMPLE_APP_NAME}>"
    )
endif()

# Android: ensure all dependencies are packaged into APK
if(ANDROID)
    # Copy deployment-settings.json to bin/ so Qt Creator can find it.
    # Qt Creator constructs output dir based on this file's location,
    # so output becomes bin/android-build-qzPlayerExample/.
    # We must also copy the app binary there for androiddeployqt to find it.
    set(_android_output_dir "${CMAKE_BINARY_DIR}/bin/android-build-${EXAMPLE_APP_NAME}")

    add_custom_command(TARGET ${EXAMPLE_APP_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/android-${EXAMPLE_APP_NAME}-deployment-settings.json"
            "${CMAKE_BINARY_DIR}/bin/android-${EXAMPLE_APP_NAME}-deployment-settings.json"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${_android_output_dir}/libs/${ANDROID_ABI}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${EXAMPLE_APP_NAME}>"
            "${_android_output_dir}/libs/${ANDROID_ABI}/$<TARGET_FILE_NAME:${EXAMPLE_APP_NAME}>"
    )

    # FFmpeg shared libraries
    if(EXISTS "${FFMPEG_DIR}/lib")
        file(GLOB _ffmpeg_android_libs "${FFMPEG_DIR}/lib/*.so")
        if(_ffmpeg_android_libs)
            set_property(TARGET ${EXAMPLE_APP_NAME} APPEND PROPERTY QT_ANDROID_EXTRA_LIBS
                ${_ffmpeg_android_libs}
            )
        endif()
    endif()

    # FFmpeg multimedia plugin - must use concrete path, not generator expression,
    # because qt6_generate_android_deployment_settings_file() runs at configure time
    set_property(TARGET ${EXAMPLE_APP_NAME} APPEND PROPERTY QT_ANDROID_LIB_DEPENDENCIES
        "${CMAKE_BINARY_DIR}/bin/plugins/multimedia/libplugins_multimedia_ffmpegmediaplugin_${ANDROID_ABI}.so"
    )

    # Also add as extra lib so androiddeployqt copies it to libs/<abi>/
    set_property(TARGET ${EXAMPLE_APP_NAME} APPEND PROPERTY QT_ANDROID_EXTRA_LIBS
        "${CMAKE_BINARY_DIR}/bin/plugins/multimedia/libplugins_multimedia_ffmpegmediaplugin_${ANDROID_ABI}.so"
    )

    # Qt6AndroidMultimedia.jar - androiddeployqt only reads JAR dependencies from
    # Qt's own AndroidDependencies.cmake files, not from custom targets.
    # So we must copy the JAR directly into the android-build output directory.
    get_target_property(_qzmultimedia_binary_dir qzMultimedia BINARY_DIR)
    set(_multimedia_jar_path "${_qzmultimedia_binary_dir}/Qt6AndroidMultimedia.jar")

    add_custom_command(TARGET ${EXAMPLE_APP_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_multimedia_jar_path}"
            "${_android_output_dir}/libs/Qt6AndroidMultimedia.jar"
    )

    # Qt6AndroidTheme.jar
    get_target_property(_qztheme_binary_dir qzTheme BINARY_DIR)
    set(_theme_jar_path "${_qztheme_binary_dir}/Qt6AndroidTheme.jar")

    add_custom_command(TARGET ${EXAMPLE_APP_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_theme_jar_path}"
            "${_android_output_dir}/libs/Qt6AndroidTheme.jar"
    )

    # Qt6AndroidPlayer.jar
    get_target_property(_qzplayer_binary_dir qzPlayer BINARY_DIR)
    set(_player_jar_path "${_qzplayer_binary_dir}/Qt6AndroidPlayer.jar")

    add_custom_command(TARGET ${EXAMPLE_APP_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_player_jar_path}"
            "${_android_output_dir}/libs/Qt6AndroidPlayer.jar"
    )

    # QML plugins - must be registered so androiddeployqt packages them into APK
    # qz.multimedia QML plugin (qzQuickMultimedia)
    set_property(TARGET ${EXAMPLE_APP_NAME} APPEND PROPERTY QT_ANDROID_EXTRA_LIBS
        "${CMAKE_BINARY_DIR}/bin/qz/multimedia/libqml_qz_multimedia_qzQuickMultimedia_${ANDROID_ABI}.so"
    )
    # qz.theme QML plugin (qzThemeplugin)
    set_property(TARGET ${EXAMPLE_APP_NAME} APPEND PROPERTY QT_ANDROID_EXTRA_LIBS
        "${CMAKE_BINARY_DIR}/bin/qz/theme/libqml_qz_theme_qzThemeplugin_${ANDROID_ABI}.so"
    )
    # qz.player QML plugin (qzPlayerplugin)
    set_property(TARGET ${EXAMPLE_APP_NAME} APPEND PROPERTY QT_ANDROID_EXTRA_LIBS
        "${CMAKE_BINARY_DIR}/bin/qz/player/libqml_qz_player_qzPlayerplugin_${ANDROID_ABI}.so"
    )
    # qz.controls.md3 QML plugin (Md3Controlsplugin)
    set_property(TARGET ${EXAMPLE_APP_NAME} APPEND PROPERTY QT_ANDROID_EXTRA_LIBS
        "${CMAKE_BINARY_DIR}/bin/qz/controls/md3/libqml_qz_controls_md3_Md3Controlsplugin_${ANDROID_ABI}.so"
    )

    # Android package source directory (contains AndroidManifest.xml, res/, etc.)
    set_property(TARGET ${EXAMPLE_APP_NAME} PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/android"
    )

    # SDK versions (AGP 9.0+ requires these in build.gradle, not AndroidManifest)
    set_property(TARGET ${EXAMPLE_APP_NAME} PROPERTY QT_ANDROID_MIN_SDK_VERSION 28)
    set_property(TARGET ${EXAMPLE_APP_NAME} PROPERTY QT_ANDROID_TARGET_SDK_VERSION 36)
endif()
