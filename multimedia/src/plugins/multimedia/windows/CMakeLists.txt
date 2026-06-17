qt_add_plugin(WindowsMediaPlugin
    CLASS_NAME WindowsIntegration
    OUTPUT_NAME windowsmediaplugin
    PLUGIN_TYPE multimedia
)

target_sources(WindowsMediaPlugin PRIVATE
    common/mfmetadata.cpp common/mfmetadata_p.h
    decoder/mfdecodersourcereader.cpp decoder/mfdecodersourcereader_p.h
    decoder/mfdecodersourcereadercallback.cpp decoder/mfdecodersourcereadercallback_p.h
    mfstream.cpp mfstream_p.h
    WindowsFormatInfo.cpp WindowsFormatInfo_p.h
    WindowsIntegration.cpp WindowsIntegration_p.h
    sourceresolver.cpp sourceresolver_p.h
)

target_include_directories(WindowsMediaPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    audio
    common
    decoder
)

target_compile_definitions(WindowsMediaPlugin PRIVATE
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
)

target_link_libraries(WindowsMediaPlugin PRIVATE
    qzMultimedia
    Qt6::CorePrivate
    Qt6::GuiPrivate
    Qt6::Concurrent
    uuid
    mf
    mfcore
    mfplat
    mfreadwrite
    mfuuid
    ole32
    oleaut32
    propsys
    shlwapi
    user32
    winmm
    wmcodecdspuuid
    qzLog
)

set_target_properties(WindowsMediaPlugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/multimedia"
)
