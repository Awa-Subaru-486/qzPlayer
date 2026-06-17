qt_add_library(qzMultimediaQuick SHARED)

qt_add_qml_module(qzMultimediaQuick
    URI "qz.multimedia"
    VERSION "${PROJECT_VERSION}"
    OUTPUT_DIRECTORY qz/multimedia
    PAST_MAJOR_VERSIONS 5
    CLASS_NAME qzMultimediaQuickModule
    PLUGIN_TARGET qzQuickMultimedia
    NO_GENERATE_PLUGIN_SOURCE
    NO_PLUGIN_OPTIONAL
    DEPENDENCIES QtQuick
)

set_target_properties(qzQuickMultimedia PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/qz/multimedia"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/qz/multimedia"
)

add_custom_command(TARGET qzQuickMultimedia POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_BINARY_DIR}/qz/multimedia"
        "${CMAKE_BINARY_DIR}/bin/qz/multimedia"
    COMMAND ${CMAKE_COMMAND}
        -DINPUT_FILE="${CMAKE_CURRENT_BINARY_DIR}/qz/multimedia/qmldir"
        -DOUTPUT_FILE="${CMAKE_BINARY_DIR}/bin/qz/multimedia/qmldir"
        -P "${CMAKE_SOURCE_DIR}/cmake/strip_prefer.cmake"
)

target_sources(qzMultimediaQuick
    PUBLIC
    FILE_SET CXX_MODULES
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/module/qzMultimediaQuick.cppm
        ${CMAKE_CURRENT_SOURCE_DIR}/module/qml_register_types.cppm
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/QuickVideoOutput.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/QuickVideoOutput_p.h
        ${CMAKE_CURRENT_SOURCE_DIR}/QuickAudioVisualizer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/QuickAudioVisualizer_p.h
        ${CMAKE_CURRENT_SOURCE_DIR}/SGVideoNode_p.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/SGVideoNode_p.h
        ${CMAKE_CURRENT_SOURCE_DIR}/SGAudioVisualizerNode.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/SGAudioVisualizerNode_p.h
        ${CMAKE_CURRENT_SOURCE_DIR}/SGVideoTexture.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/SGVideoTexture_p.h
        ${CMAKE_CURRENT_SOURCE_DIR}/QuickPreviewFrame.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/QuickPreviewFrame_p.h
)

target_sources(qzQuickMultimedia PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/MultimediaPlugin.cpp
)

target_link_libraries(qzQuickMultimedia PRIVATE
    qzMultimediaQuick_consumer
)

target_include_directories(qzMultimediaQuick PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/module
)

target_include_directories(qzQuickMultimedia PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(qzMultimediaQuick PRIVATE
    QT_BUILD_MULTIMEDIAQUICK_LIB
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
)

target_link_libraries(qzMultimediaQuick PUBLIC
    Qt6::Core
    Qt6::CorePrivate
    Qt6::GuiPrivate
    Qt6::Quick
    Qt6::QuickPrivate
    qzMultimedia
    qzLog
)

if(TARGET Qt6::ShaderToolsPrivate)
    target_link_libraries(qzMultimediaQuick PRIVATE Qt6::ShaderToolsPrivate)
endif()

qt_add_shaders(qzMultimediaQuick "multimediaquick_shaders"
    SILENT BATCHABLE PRECOMPILE OPTIMIZED
    PREFIX "/qz/multimedia"
    FILES
        "shaders/PreviewFrame.vert"
        "shaders/PreviewFrame.frag"
)

# 生成模块初始化器 workaround（解决 Windows DLL + C++20/23 模块符号导出问题）
set(_QZMULTIMEDIAQUICK_MODULES
    qzMultimediaQuick
    qml_register_types_qzMultimedia
)

add_cpp_module_consumer(qzMultimediaQuick
    MODULES ${_QZMULTIMEDIAQUICK_MODULES}
    INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/module
)
