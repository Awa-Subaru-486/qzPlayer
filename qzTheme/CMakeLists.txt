qt_add_library(qzTheme SHARED)

get_files_by_extensions("${CMAKE_CURRENT_SOURCE_DIR}" QML_FILES .qml)

set_source_files_properties(${QML_FILES} PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(qzTheme
    URI "qz.theme"
    OUTPUT_DIRECTORY qz/theme
    QML_FILES
        ${QML_FILES}
)

set_target_properties(qzThemeplugin PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/qz/theme"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/qz/theme"
)

add_custom_command(TARGET qzThemeplugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_BINARY_DIR}/qz/theme"
        "${CMAKE_BINARY_DIR}/bin/qz/theme"
    COMMAND ${CMAKE_COMMAND}
        -DINPUT_FILE="${CMAKE_CURRENT_BINARY_DIR}/qz/theme/qmldir"
        -DOUTPUT_FILE="${CMAKE_BINARY_DIR}/bin/qz/theme/qmldir"
        -P "${CMAKE_SOURCE_DIR}/cmake/strip_prefer.cmake"
)

target_sources(qzTheme
    PUBLIC
    FILE_SET CXX_MODULES
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/module/qzTheme.cppm
        ${CMAKE_CURRENT_SOURCE_DIR}/module/ThemeTypes.cppm
        ${CMAKE_CURRENT_SOURCE_DIR}/module/backend/IBackend.cppm
        ${CMAKE_CURRENT_SOURCE_DIR}/module/backend/Backend.cppm
        ${CMAKE_CURRENT_SOURCE_DIR}/module/backend/ThemeBackend.cppm
        ${CMAKE_CURRENT_SOURCE_DIR}/module/qml_register_types.cppm
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/module/ThemeTypes.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/module/ThemeConfig.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/module/ThemeConfig.cpp
)

if(WIN32)
    target_sources(qzTheme
        PUBLIC
        FILE_SET CXX_MODULES
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/module/backend/WindowsBackend.cppm
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/module/core/WindowsBackend.cpp
    )
    target_link_libraries(qzTheme PRIVATE dwmapi)
elseif(ANDROID)
    target_sources(qzTheme
        PUBLIC
        FILE_SET CXX_MODULES
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/module/backend/AndroidBackend.cppm
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/module/core/AndroidBackend.cpp
    )

    # Build Android jar containing QtThemeBackend Java class
    set(_theme_jar_dir ${CMAKE_CURRENT_BINARY_DIR}/Qt6AndroidTheme)
    set(_theme_jar ${CMAKE_CURRENT_BINARY_DIR}/Qt6AndroidTheme.jar)
    set(_theme_java_src
        ${CMAKE_CURRENT_SOURCE_DIR}/android/jar/src/qz/theme/android/QtThemeBackend.java
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
        OUTPUT ${_theme_jar}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_theme_jar_dir}"
        COMMAND ${JAVAC_EXECUTABLE} -source 11 -target 11
                -classpath "${QT_ANDROID_JAR};${_qt_android_jar}"
                -d "${_theme_jar_dir}"
                ${_theme_java_src}
        COMMAND ${JAR_EXECUTABLE} cf "${_theme_jar}" -C "${_theme_jar_dir}" .
        DEPENDS ${_theme_java_src}
        COMMENT "Building Qt6AndroidTheme.jar"
    )
    add_custom_target(Qt6AndroidTheme_jar ALL DEPENDS ${_theme_jar})
    set_property(TARGET qzTheme APPEND PROPERTY
        QT_ANDROID_BUNDLED_JAR_DEPENDENCIES "${_theme_jar}"
    )
    set_property(TARGET qzTheme APPEND PROPERTY
        INTERFACE_QT_ANDROID_BUNDLED_JAR_DEPENDENCIES "${_theme_jar}"
    )
elseif(LINUX)
    target_sources(qzTheme
        PUBLIC
        FILE_SET CXX_MODULES
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/module/backend/LinuxBackend.cppm
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/module/core/LinuxBackend.cpp
    )
endif()

target_include_directories(qzTheme PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/module)

target_compile_definitions(qzTheme PRIVATE QZTHEME_BUILD_LIB)

target_link_libraries(qzTheme PUBLIC
    Qt6::Quick
    Qt6::Core
    Qt6::Gui
)

# 生成模块初始化器 workaround（解决 Windows DLL + C++20/23 模块符号导出问题）
set(_QZTHEME_MODULES
    qzTheme
    qzTheme:ThemeTypes
    qzTheme:ThemeBackend
    qzTheme:IBackend
    qzTheme:Backend
    qml_register_types_qzTheme
)
if(WIN32)
    list(APPEND _QZTHEME_MODULES qzTheme:WindowsBackend)
elseif(ANDROID)
    list(APPEND _QZTHEME_MODULES qzTheme:AndroidBackend)
elseif(LINUX)
    list(APPEND _QZTHEME_MODULES qzTheme:LinuxBackend)
endif()

add_cpp_module_consumer(qzTheme
    MODULES ${_QZTHEME_MODULES}
    INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/module
)
