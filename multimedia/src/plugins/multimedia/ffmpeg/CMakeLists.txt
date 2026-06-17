find_package(Vulkan)

add_library(FFmpegMediaPluginImpl STATIC
    FFmpeg_pch.h
    FFmpeg.cpp FFmpeg_p.h
    FFmpegDefs_p.h
    FFmpegCodecStorage.cpp FFmpegCodecStorage_p.h
    FFmpegIoUtils.cpp FFmpegIoUtils_p.h
    FFmpegAvAudioFormat.cpp FFmpegAvAudioFormat_p.h
    FFmpegAudioInput.cpp FFmpegAudioInput_p.h
    FFmpegCodec.cpp FFmpegCodec_p.h
    FFmpegConverter.cpp FFmpegConverter_p.h
    FFmpegHwAccel.cpp FFmpegHwAccel_p.h
    FFmpegTextureConverter.cpp FFmpegTextureConverter_p.h
    FFmpegMediaMetadata.cpp FFmpegMediaMetadata_p.h
    FFmpegMediaPlayer.cpp FFmpegMediaPlayer_p.h
    FFmpegPreviewFrameProvider.cpp FFmpegPreviewFrameProvider_p.h
    FFmpegVideoSink.cpp FFmpegVideoSink_p.h
    FFmpegMediaFormatInfo.cpp FFmpegMediaFormatInfo_p.h
    FFmpegMediaIntegration.cpp FFmpegMediaIntegration_p.h
    FFmpegVideoBuffer.cpp FFmpegVideoBuffer_p.h
    FFmpegThread.cpp FFmpegThread_p.h
    FFmpegResampler.cpp FFmpegResampler_p.h
    FFmpegPlaybackEngine.cpp FFmpegPlaybackEngine_p.h
    playbackengine/FFmpegPlaybackEngineDefs_p.h
    playbackengine/FFmpegPlaybackEngineObject.cpp playbackengine/FFmpegPlaybackEngineObject_p.h
    playbackengine/FFmpegDemuxer.cpp playbackengine/FFmpegDemuxer_p.h
    playbackengine/FFmpegStreamDecoder.cpp playbackengine/FFmpegStreamDecoder_p.h
    playbackengine/FFmpegRenderer.cpp playbackengine/FFmpegRenderer_p.h
    playbackengine/FFmpegAudioFrameConverter.cpp playbackengine/FFmpegAudioFrameConverter_p.h
    playbackengine/FFmpegAudioRenderer.cpp playbackengine/FFmpegAudioRenderer_p.h
    playbackengine/FFmpegVideoRenderer.cpp playbackengine/FFmpegVideoRenderer_p.h
    playbackengine/FFmpegSubtitleRenderer.cpp playbackengine/FFmpegSubtitleRenderer_p.h
    playbackengine/FFmpegTime_p.h
    playbackengine/FFmpegTimeController.cpp playbackengine/FFmpegTimeController_p.h
    playbackengine/FFmpegMediaDataHolder.cpp playbackengine/FFmpegMediaDataHolder_p.h
    playbackengine/FFmpegCodecContext.cpp playbackengine/FFmpegCodecContext_p.h
    playbackengine/FFmpegPacket_p.h
    playbackengine/FFmpegFrame_p.h
    playbackengine/FFmpegPlaybackUtils_p.h
)

target_compile_definitions(FFmpegMediaPluginImpl PRIVATE
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
)

target_include_directories(FFmpegMediaPluginImpl SYSTEM PRIVATE
    ../../../3rdparty/signalsmith-stretch/
)

target_include_directories(FFmpegMediaPluginImpl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    playbackengine
)

target_compile_features(FFmpegMediaPluginImpl PUBLIC cxx_std_23)

set(FFMPEG_FWD_DIR "${CMAKE_BINARY_DIR}/qzFFmpegMediaPluginImpl")
set(FFMPEG_FWD_PRIVATE_DIR "${FFMPEG_FWD_DIR}/private")
file(MAKE_DIRECTORY ${FFMPEG_FWD_PRIVATE_DIR})

file(GLOB_RECURSE FFMPEG_PRIVATE_HEADERS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "*_p.h"
)

foreach(_header ${FFMPEG_PRIVATE_HEADERS})
    get_filename_component(_header_name ${_header} NAME)
    file(WRITE "${FFMPEG_FWD_PRIVATE_DIR}/${_header_name}"
         "#include \"${CMAKE_CURRENT_SOURCE_DIR}/${_header}\"\n")
endforeach()

target_include_directories(FFmpegMediaPluginImpl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

target_link_libraries(FFmpegMediaPluginImpl
    PUBLIC
        qzMultimedia
        Qt6::CorePrivate
        Qt6::GuiPrivate
    PRIVATE
        qzLog
)

if(WIN32)
    target_sources(FFmpegMediaPluginImpl PRIVATE
        d3d11va/FFmpegHwAccel_d3d11.cpp d3d11va/FFmpegHwAccel_d3d11_p.h
        d3d11va/FFmpegHwAccel_d3d11sw.cpp d3d11va/FFmpegHwAccel_d3d11sw_p.h
    )
    target_link_libraries(FFmpegMediaPluginImpl PRIVATE mf mfplat mfreadwrite mfuuid)
endif()

if(Vulkan_FOUND AND TARGET qzVulkanContext)
    target_sources(FFmpegMediaPluginImpl PRIVATE
        vkvideo/FFmpegHwAccel_vulkan.cpp vkvideo/FFmpegHwAccel_vulkan_p.h
        vkvideo/FFmpegHwAccel_vulkan_utils.cpp vkvideo/FFmpegHwAccel_vulkan_utils_p.h
        vkvideo/FFmpegHwAccel_vulkan_single.cpp vkvideo/FFmpegHwAccel_vulkan_single_p.h
        vkvideo/FFmpegHwAccel_vulkan_cross.cpp vkvideo/FFmpegHwAccel_vulkan_cross_p.h
        vkvideo/FFmpegHwAccel_vulkansw.cpp vkvideo/FFmpegHwAccel_vulkansw_p.h
        vkvideo/private/VkDispatch_p.h vkvideo/private/VkDispatch_p.cpp
        vkvideo/private/VkMemory_p.h vkvideo/private/VkMemory_p.cpp
        vkvideo/private/VkFormatUtils_p.h vkvideo/private/VkFormatUtils_p.cpp
        vkvideo/private/VkDeviceContext_p.h vkvideo/private/VkDeviceContext_p.cpp
        vkvideo/private/VkCommandContext_p.h vkvideo/private/VkCommandContext_p.cpp
        vkvideo/private/VkImage_p.h vkvideo/private/VkImage_p.cpp
        vkvideo/private/VkExternalMemory_p.h vkvideo/private/VkExternalMemory_p.cpp
    )
    if(WIN32)
        target_compile_definitions(FFmpegMediaPluginImpl PRIVATE
            VK_USE_PLATFORM_WIN32_KHR VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
    elseif(ANDROID)
        target_compile_definitions(FFmpegMediaPluginImpl PRIVATE
            VK_USE_PLATFORM_ANDROID_KHR VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
        target_link_libraries(FFmpegMediaPluginImpl PRIVATE android)
    else()
        target_compile_definitions(FFmpegMediaPluginImpl PRIVATE
            VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
    endif()
    target_compile_options(FFmpegMediaPluginImpl PRIVATE -include vulkan/vulkan.h)
    target_link_libraries(FFmpegMediaPluginImpl PRIVATE qzVulkanContext_consumer Vulkan::Vulkan)
endif()

if(ANDROID)
    target_sources(FFmpegMediaPluginImpl PRIVATE
        android/FFmpegHwAccel_MediaCodec.cpp android/FFmpegHwAccel_MediaCodec_p.h
        android/AndroidSurfaceTexture.cpp android/AndroidSurfaceTexture_p.h
    )
    if(FFMPEG_DIR)
        target_include_directories(FFmpegMediaPluginImpl PRIVATE
            ${FFMPEG_DIR}/include
        )
    endif()
    target_link_libraries(FFmpegMediaPluginImpl PRIVATE
        mediandk
        android
    )
endif()

if(GCC OR CLANG)
    set_source_files_properties(playbackengine/FFmpegAudioFrameConverter.cpp PROPERTIES
        COMPILE_OPTIONS "$<$<CONFIG:Debug>:-O3>"
    )
elseif(MSVC)
    set_source_files_properties(playbackengine/FFmpegAudioFrameConverter.cpp PROPERTIES
        COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:QT_MM_OPTIMIZE_DEBUG>"
    )
endif()

set(ffmpeg_libs
    FFmpeg::avformat
    FFmpeg::avcodec
    FFmpeg::avutil
    FFmpeg::swresample
    FFmpeg::swscale
)
target_link_libraries(FFmpegMediaPluginImpl PUBLIC ${ffmpeg_libs})

qt_add_plugin(FFmpegMediaPlugin
    CLASS_NAME FFmpegPlugin
    OUTPUT_NAME ffmpegmediaplugin
    PLUGIN_TYPE multimedia
)

target_sources(FFmpegMediaPlugin PRIVATE
    FFmpegPlugin.cpp
    ffmpeg.json
)

target_link_libraries(FFmpegMediaPlugin PRIVATE
    FFmpegMediaPluginImpl
    qzMultimedia
)

set_target_properties(FFmpegMediaPlugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/multimedia"
)

if(ANDROID)
    # Include FFmpeg shared libraries in APK
    if(EXISTS "${FFMPEG_DIR}/lib")
        file(GLOB FFMPEG_ANDROID_LIBS "${FFMPEG_DIR}/lib/*.so")
        if(FFMPEG_ANDROID_LIBS)
            set_property(TARGET qzMultimedia APPEND PROPERTY QT_ANDROID_EXTRA_LIBS
                ${FFMPEG_ANDROID_LIBS}
            )
            set_property(TARGET qzMultimedia APPEND PROPERTY INTERFACE_QT_ANDROID_EXTRA_LIBS
                ${FFMPEG_ANDROID_LIBS}
            )
        endif()
    endif()

    # Register FFmpeg plugin for Android APK deployment
    # Must use concrete path, not generator expression, because
    # qt6_generate_android_deployment_settings_file() runs at configure time
    set(_ffmpeg_plugin_path "${CMAKE_BINARY_DIR}/bin/plugins/multimedia/libplugins_multimedia_ffmpegmediaplugin_${ANDROID_ABI}.so")

    set_property(TARGET qzMultimedia APPEND PROPERTY QT_ANDROID_LIB_DEPENDENCIES
        "${_ffmpeg_plugin_path}"
    )
    set_property(TARGET qzMultimedia APPEND PROPERTY INTERFACE_QT_ANDROID_LIB_DEPENDENCIES
        "${_ffmpeg_plugin_path}"
    )

    # Also add as extra lib so androiddeployqt copies it to libs/<abi>/
    set_property(TARGET qzMultimedia APPEND PROPERTY QT_ANDROID_EXTRA_LIBS
        "${_ffmpeg_plugin_path}"
    )
    set_property(TARGET qzMultimedia APPEND PROPERTY INTERFACE_QT_ANDROID_EXTRA_LIBS
        "${_ffmpeg_plugin_path}"
    )
endif()
