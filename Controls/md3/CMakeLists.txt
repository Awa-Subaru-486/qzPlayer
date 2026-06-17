qt_add_library(Md3Controls SHARED)

get_files_by_extensions("${CMAKE_CURRENT_SOURCE_DIR}/src" QML_FILES .qml)

qt_add_qml_module(Md3Controls
    URI "qz.controls.md3"
    OUTPUT_DIRECTORY qz/controls/md3
    QML_FILES
        ${QML_FILES}
    DEPENDENCIES
        TARGET qzTheme
)

set_target_properties(Md3Controlsplugin PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/qz/controls/md3"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/qz/controls/md3"
)

add_custom_command(TARGET Md3Controlsplugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_BINARY_DIR}/qz/controls/md3"
        "${CMAKE_BINARY_DIR}/bin/qz/controls/md3"
    COMMAND ${CMAKE_COMMAND}
        -DINPUT_FILE="${CMAKE_CURRENT_BINARY_DIR}/qz/controls/md3/qmldir"
        -DOUTPUT_FILE="${CMAKE_BINARY_DIR}/bin/qz/controls/md3/qmldir"
        -P "${CMAKE_SOURCE_DIR}/cmake/strip_prefer.cmake"
)

qt_add_shaders(Md3Controls "md3_shaders"
    SILENT BATCHABLE PRECOMPILE OPTIMIZED
    PREFIX "/qz/controls/md3"
    FILES
        "src/shaders/Loader.frag"
)

target_link_libraries(Md3Controls PUBLIC
    Qt6::Quick
    Qt6::Core
    Qt6::Gui
)
