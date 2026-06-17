find_package(Qt6 COMPONENTS ShaderTools)
find_package(Vulkan)

set(MULTIMEDIA_SOURCES
    audio/AutoResetEvent_p.h
    audio/PmrEmulation_p.h
    audio/MemoryResourceTlsf_p.h
    audio/MemoryResourceTlsf.cpp
    audio/QtAudio.cpp audio/QtAudio.h audio/Audio.h
    audio/AudioBuffer.cpp audio/AudioBuffer.h
    audio/AudioBufferSupport_p.h
    audio/AudioDevice.cpp audio/AudioDevice.h audio/AudioDevice_p.h
    audio/AudioDeviceMonitor.cpp audio/AudioDeviceMonitor_p.h
    audio/AudioInput.cpp audio/AudioInput.h
    audio/AudioBufferOutput.cpp audio/AudioBufferOutput.h audio/AudioBufferOutput_p.h
    audio/AudioSpectrumAnalyzer.cpp audio/AudioSpectrumAnalyzer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/kissfft-131.2.0/kiss_fft.cpp
    audio/AudioOutput.cpp audio/AudioOutput.h
    audio/AudioFormat.cpp audio/AudioFormat.h audio/AudioFormat_p.h
    audio/AudioHelpers.cpp audio/AudioHelpers_p.h
    audio/AudioRingBuffer_p.h
    audio/AudioSource.cpp audio/AudioSource.h
    audio/AudioSink.cpp audio/AudioSink.h
    audio/AudioSystem.cpp audio/AudioSystem_p.h
    audio/AudioSystemPlatformStreamSupport.cpp audio/AudioSystemPlatformStreamSupport_p.h
    audio/AudioAlignmentSupport_p.h
    audio/AudioPlatformImplementationSupport_p.h
    audio/AudioQIODeviceSupport_p.h
    audio/AudioQSpanSupport_p.h
    audio/AudioRtsanSupport_p.h
    audio/RtAudioEngine.cpp audio/RtAudioEngine_p.h
    platform/PlatformAudioInput_p.h
    platform/PlatformAudioOutput_p.h
    platform/PlatformAudioResampler_p.h
    platform/PlatformAudioDevices.cpp platform/PlatformAudioDevices_p.h
    platform/PlatformMediaFormatInfo.cpp  platform/PlatformMediaFormatInfo_p.h
    platform/PlatformMediaIntegration.cpp platform/PlatformMediaIntegration_p.h
    platform/PlatformMediaPlayer.cpp platform/PlatformMediaPlayer_p.h
    platform/PlatformVideoSink.cpp platform/PlatformVideoSink_p.h
    platform/PlatformVideoSource.cpp platform/PlatformVideoSource_p.h
    platform/PlatformMediaPlugin.cpp platform/PlatformMediaPlugin_p.h
    platform/PlatformPreviewFrameProvider_p.h
    playback/MediaPlayer.cpp playback/MediaPlayer.h playback/MediaPlayer_p.h
    playback/PreviewFrameProvider.cpp playback/PreviewFrameProvider.h playback/PreviewFrameProvider_p.h
    ChapterInfo.h
    TrackInfo.h
    playback/PlaybackOptions.cpp playback/PlaybackOptions.h
    MultimediaEnumToStringConverter_p.h
    MediaDevices.cpp MediaDevices.h
    MediaFormat.cpp  MediaFormat.h
    MediaMetadata.cpp MediaMetadata.h
    MediaStorageLocation.cpp MediaStorageLocation_p.h
    MediaTimeRange.cpp MediaTimeRange.h
    MultimediaUtils.cpp MultimediaUtils_p.h
    MultimediaRanges_p.h
    VideoTransformation.cpp VideoTransformation_p.h
    MultimediaAssume_p.h
    MultimediaGlobal.h MultimediaGlobal_p.h
    ErrorInfo_p.h
    CachedValue_p.h
    MediaInputEncoderInterface_p.h
    RhiValueMapper.cpp  RhiValueMapper_p.h
    ThreadLocalRhi.cpp  ThreadLocalRhi_p.h
    SharedHandle_p.h
    TaggedTime_p.h
    video/AbstractVideoBuffer.cpp video/AbstractVideoBuffer.h
    video/HwVideoBuffer.cpp video/HwVideoBuffer_p.h
    video/MemoryVideoBuffer.cpp video/MemoryVideoBuffer_p.h
    video/ImageVideoBuffer.cpp video/ImageVideoBuffer_p.h
    video/VideoFrame.cpp video/VideoFrame.h video/VideoFrame_p.h
    video/VideoSink.cpp video/VideoSink.h
    video/SubtitleStyle.cpp video/SubtitleStyle.h
    video/VideoTextureHelper.cpp video/VideoTextureHelper_p.h
    video/VideoFrameTextureFromSource.cpp video/VideoFrameTextureFromSource_p.h
    video/VideoFrameTexturePool.cpp video/VideoFrameTexturePool_p.h
    video/VideoFrameConversionHelper.cpp video/VideoFrameConversionHelper_p.h
    video/VideoOutputOrientationHandler.cpp video/VideoOutputOrientationHandler_p.h
    video/VideoFrameConverter.cpp video/VideoFrameConverter_p.h
    video/VideoFrameFormat.cpp video/VideoFrameFormat.h
    video/QtVideo.cpp video/QtVideo.h
    shaders/RhiTextureFormats_p.h
)

qt_add_library(qzMultimedia SHARED ${MULTIMEDIA_SOURCES})

set(QTMULTIMEDIA_FORWARDING_HEADERS_DIR "${CMAKE_BINARY_DIR}/qzMultimedia")
file(MAKE_DIRECTORY ${QTMULTIMEDIA_FORWARDING_HEADERS_DIR})
file(MAKE_DIRECTORY ${QTMULTIMEDIA_FORWARDING_HEADERS_DIR}/private)

# Generate config header
function(qt_mm_feature FEATURE_NAME FEATURE_VALUE)
    string(APPEND QTMULTIMEDIA_CONFIG_CONTENT
        "#ifdef ${FEATURE_NAME}\n"
        "#undef ${FEATURE_NAME}\n"
        "#endif\n"
        "#define ${FEATURE_NAME} ${FEATURE_VALUE}\n"
    )
    set(QTMULTIMEDIA_CONFIG_CONTENT "${QTMULTIMEDIA_CONFIG_CONTENT}" PARENT_SCOPE)
endfunction()

string(APPEND QTMULTIMEDIA_CONFIG_CONTENT
    "#ifndef QTMULTIMEDIA_CONFIG_H\n"
    "#define QTMULTIMEDIA_CONFIG_H\n"
)

qt_mm_feature(QT_FEATURE_thread 1)
qt_mm_feature(QT_FEATURE_network 1)
qt_mm_feature(QT_FEATURE_temporaryfile 1)
qt_mm_feature(QT_FEATURE_mimetype 1)
qt_mm_feature(QT_FEATURE_opengl -1)

if(ENABLE_FFMPEG_PLUGIN)
    qt_mm_feature(QT_FEATURE_ffmpeg 1)
else()
    qt_mm_feature(QT_FEATURE_ffmpeg -1)
endif()

if(Vulkan_FOUND)
    qt_mm_feature(QT_FEATURE_vulkan 1)
else()
    qt_mm_feature(QT_FEATURE_vulkan -1)
endif()

if(WIN32)
    qt_mm_feature(QT_FEATURE_wasapi 1)
    qt_mm_feature(QT_FEATURE_native_windows_backend 1)
endif()

if(ANDROID)
    qt_mm_feature(QT_FEATURE_aaudio 1)
endif()

string(APPEND QTMULTIMEDIA_CONFIG_CONTENT "#endif\n")

file(WRITE "${QTMULTIMEDIA_FORWARDING_HEADERS_DIR}/qtmultimedia-config.h" ${QTMULTIMEDIA_CONFIG_CONTENT})
file(WRITE "${QTMULTIMEDIA_FORWARDING_HEADERS_DIR}/private/qtmultimedia-config_p.h" ${QTMULTIMEDIA_CONFIG_CONTENT})

file(WRITE "${QTMULTIMEDIA_FORWARDING_HEADERS_DIR}/qtmultimediaexports.h"
"#ifndef QTMULTIMEDIAEXPORTS_H
#define QTMULTIMEDIAEXPORTS_H
#include <QtCore/qtconfigmacros.h>
#if defined(QT_SHARED) || !defined(QT_STATIC)
#  if defined(QT_BUILD_MULTIMEDIA_LIB)
#    define QZ_MULTIMEDIA_EXPORT Q_DECL_EXPORT
#  else
#    define QZ_MULTIMEDIA_EXPORT Q_DECL_IMPORT
#  endif
#else
#  define QZ_MULTIMEDIA_EXPORT
#endif
#endif
")

set(QTMULTIMEDIA_PUBLIC_HEADERS
    MultimediaGlobal.h
    MediaDevices.h
    MediaFormat.h
    MediaMetadata.h
    MediaTimeRange.h
    audio/Audio.h
    audio/QtAudio.h
    audio/AudioBuffer.h
    audio/AudioBufferOutput.h
    audio/AudioSpectrumAnalyzer.h
    audio/AudioDevice.h
    audio/AudioFormat.h
    audio/AudioInput.h
    audio/AudioOutput.h
    audio/AudioSink.h
    audio/AudioSource.h
    playback/MediaPlayer.h
    playback/PreviewFrameProvider.h
    ChapterInfo.h
    TrackInfo.h
    playback/PlaybackOptions.h
    video/AbstractVideoBuffer.h
    video/QtVideo.h
    video/VideoFrame.h
    video/VideoFrameFormat.h
    video/VideoSink.h
    video/SubtitleStyle.h
)

foreach(_header ${QTMULTIMEDIA_PUBLIC_HEADERS})
    get_filename_component(_header_name ${_header} NAME)
    file(WRITE "${QTMULTIMEDIA_FORWARDING_HEADERS_DIR}/${_header_name}"
         "#include \"multimedia/${_header}\"\n")
endforeach()

file(GLOB_RECURSE QTMULTIMEDIA_PRIVATE_HEADERS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "*_p.h"
)

foreach(_header ${QTMULTIMEDIA_PRIVATE_HEADERS})
    get_filename_component(_header_name ${_header} NAME)
    file(WRITE "${QTMULTIMEDIA_FORWARDING_HEADERS_DIR}/private/${_header_name}"
         "#include \"multimedia/${_header}\"\n")
endforeach()

target_include_directories(qzMultimedia
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/qzMultimedia>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        audio
        platform
        playback
        video
        ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/kissfft-131.2.0
)

target_compile_definitions(qzMultimedia
    PRIVATE
        QT_NO_CAST_FROM_ASCII # 禁止从 const char* 隐式转换为 QString
        QT_NO_CAST_TO_ASCII # 禁止从 QString 隐式转换为 const char*
        QT_BUILD_MULTIMEDIA_LIB
)

target_link_libraries(qzMultimedia
    PUBLIC
        Qt6::Core
        Qt6::Gui
    PRIVATE
        Qt6::CorePrivate
        Qt6::GuiPrivate
        BundledTLSF
        Qt6::Concurrent
        qzLog
)

set_target_properties(qzMultimedia PROPERTIES
    PLUGIN_TYPES multimedia
)

if(Vulkan_FOUND)
    add_subdirectory(../vulkancontext ${CMAKE_BINARY_DIR}/vulkancontext)
    target_link_libraries(qzMultimedia PRIVATE qzVulkanContext)
endif()

target_compile_features(qzMultimedia PUBLIC cxx_std_23)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND WIN32)
    target_compile_options(qzMultimedia PRIVATE -faligned-allocation -include new)
endif()

if(TARGET Qt6::Network)
    target_link_libraries(qzMultimedia PUBLIC Qt6::Network)
endif()

if(NOT MSVC AND NOT ANDROID)
    set_source_files_properties(video/VideoFrameConversionHelperSse2.cpp PROPERTIES COMPILE_FLAGS "-msse2")
    set_source_files_properties(video/VideoFrameConversionHelperSsse3.cpp PROPERTIES COMPILE_FLAGS "-mssse3")
    if(NOT APPLE OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set_source_files_properties(video/VideoFrameConversionHelperAvx2.cpp PROPERTIES COMPILE_FLAGS "-march=haswell")
    endif()
    target_sources(qzMultimedia PRIVATE
        video/VideoFrameConversionHelperSse2.cpp
        video/VideoFrameConversionHelperSsse3.cpp
        video/VideoFrameConversionHelperAvx2.cpp
    )
elseif(ANDROID AND ANDROID_ABI STREQUAL "x86_64")
    set_source_files_properties(video/VideoFrameConversionHelperSse2.cpp PROPERTIES COMPILE_FLAGS "-msse2")
    set_source_files_properties(video/VideoFrameConversionHelperSsse3.cpp PROPERTIES COMPILE_FLAGS "-mssse3")
    target_sources(qzMultimedia PRIVATE
        video/VideoFrameConversionHelperSse2.cpp
        video/VideoFrameConversionHelperSsse3.cpp
    )
endif()

if(UNIX OR ANDROID)
    target_sources(qzMultimedia PRIVATE
        audio/AutoResetEventPipe_p.h audio/AutoResetEventPipe.cpp
    )
endif()

if(WIN32)
    target_sources(qzMultimedia PRIVATE
        audio/AutoResetEventWin32_p.h audio/AutoResetEventWin32.cpp
    )
endif()

if(LINUX OR ANDROID)
    target_sources(qzMultimedia PRIVATE
        audio/AutoResetEventLinux_p.h audio/AutoResetEventLinux.cpp
    )
endif()

# Windows platform sources
if(WIN32)
    target_sources(qzMultimedia PRIVATE
        windows/ComInitializer.cpp windows/ComInitializer_p.h
        windows/ComTaskResource_p.h
        windows/WindowsPropertystore.cpp windows/WindowsPropertystore_p.h
        windows/WindowsWasapiWarmupClient.cpp windows/WindowsWasapiWarmupClient_p.h
        windows/WindowsAudioDevice.cpp windows/WindowsAudioDevice_p.h
        windows/WindowsAudioDevices.cpp windows/WindowsAudioDevices_p.h
        windows/WindowsAudioSink.cpp windows/WindowsAudioSink_p.h
        windows/WindowsAudioSource.cpp windows/WindowsAudioSource_p.h
        windows/WindowsAudioUtils.cpp windows/WindowsAudioUtils_p.h
        windows/WindowsMediaFoundation.cpp windows/WindowsMediaFoundation_p.h
        windows/WindowsMultimediaUtils.cpp windows/WindowsMultimediaUtils_p.h
        windows/WindowsResampler.cpp windows/WindowsResampler_p.h
        windows/WmfSupport.cpp windows/WmfSupport_p.h
        windows/WindowsSmtcControl.cpp windows/WindowsSmtcControl_p.h
        windows/WindowsSmtcManager.cpp windows/WindowsSmtcManager_p.h
        windows/WindowsPlatformSpecificInterface.cpp windows/WindowsPlatformSpecificInterface_p.h
    )
    target_link_libraries(qzMultimedia PRIVATE avrt ksuser propsys uuid)
    set_source_files_properties(
        windows/WindowsWasapiWarmupClient.cpp
        windows/WindowsAudioDevices.cpp
        PROPERTIES COMPILE_DEFINITIONS INITGUID SKIP_PRECOMPILE_HEADERS TRUE
    )
    if(MINGW)
        set_source_files_properties(windows/WindowsAudioDevices.cpp PROPERTIES COMPILE_DEFINITIONS INITGUID)
    endif()

    # Windows SDK paths for C++/WinRT (SMTC) support
    if(NOT WIN_SDK_ROOT)
        set(WIN_SDK_ROOT "C:/Program Files (x86)/Windows Kits/10" CACHE PATH "Windows 10 SDK root directory")
    endif()
    file(GLOB WIN_SDK_VERSIONS LIST_DIRECTORIES true "${WIN_SDK_ROOT}/Include/*/um")
    list(SORT WIN_SDK_VERSIONS ORDER DESCENDING)
    list(GET WIN_SDK_VERSIONS 0 _WIN_SDK_LATEST_UM)
    get_filename_component(_WIN_SDK_INC_DIR "${_WIN_SDK_LATEST_UM}" DIRECTORY)
    get_filename_component(WIN_SDK_VERSION "${_WIN_SDK_INC_DIR}" NAME)

    set(WIN_SDK_INCLUDE "${WIN_SDK_ROOT}/Include/${WIN_SDK_VERSION}")

    if(EXISTS "${WIN_SDK_INCLUDE}/cppwinrt")
        qt_mm_feature(QT_FEATURE_smtc 1)

        target_include_directories(qzMultimedia PRIVATE "${WIN_SDK_INCLUDE}/cppwinrt")
        target_link_libraries(qzMultimedia PRIVATE WindowsApp)

        if(NOT MSVC)
            target_compile_options(qzMultimedia PRIVATE
                -Wno-unused-command-line-argument
                -fno-strict-aliasing
                -Wno-nonportable-include-path
            )
        endif()
    else()
        qt_mm_feature(QT_FEATURE_smtc -1)
    endif()
endif()

# Android platform sources
if(ANDROID)
    target_sources(qzMultimedia PRIVATE
        android/AAudioStream.cpp android/AAudioStream_p.h
        android/AndroidAudioDevice.cpp android/AndroidAudioDevice_p.h
        android/AndroidAudioDevices.cpp android/AndroidAudioDevices_p.h
        android/AndroidAudioJniTypes_p.h
        android/AndroidAudioSink.cpp android/AndroidAudioSink_p.h
        android/AndroidAudioSource.cpp android/AndroidAudioSource_p.h
        android/AndroidAudioUtil.cpp android/AndroidAudioUtil_p.h
    )
    target_link_libraries(qzMultimedia PRIVATE aaudio)

    # Build Android jar containing QtAudioDeviceManager Java class
    set(_multimedia_jar_dir ${CMAKE_CURRENT_BINARY_DIR}/Qt6AndroidMultimedia)
    set(_multimedia_jar ${CMAKE_CURRENT_BINARY_DIR}/Qt6AndroidMultimedia.jar)
    set(_multimedia_java_src
        ${CMAKE_CURRENT_SOURCE_DIR}/../android/jar/src/qz/multimedia/android/QtAudioDeviceManager.java
        ${CMAKE_CURRENT_SOURCE_DIR}/../android/jar/src/qz/multimedia/android/QtSurfaceTextureListener.java
        ${CMAKE_CURRENT_SOURCE_DIR}/../android/jar/src/qz/multimedia/android/QtSurfaceTextureHolder.java
        ${CMAKE_CURRENT_SOURCE_DIR}/../android/jar/src/qz/multimedia/android/QtContentUriResolver.java
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
        OUTPUT ${_multimedia_jar}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_multimedia_jar_dir}"
        COMMAND ${JAVAC_EXECUTABLE} -source 11 -target 11
                -classpath "${QT_ANDROID_JAR};${_qt_android_jar}"
                -d "${_multimedia_jar_dir}"
                ${_multimedia_java_src}
        COMMAND ${JAR_EXECUTABLE} cf "${_multimedia_jar}" -C "${_multimedia_jar_dir}" .
        DEPENDS ${_multimedia_java_src}
        COMMENT "Building Qt6AndroidMultimedia.jar"
    )
    add_custom_target(Qt6AndroidMultimedia_jar ALL DEPENDS ${_multimedia_jar})
    set_property(TARGET qzMultimedia APPEND PROPERTY
        QT_ANDROID_BUNDLED_JAR_DEPENDENCIES "${_multimedia_jar}"
    )
    set_property(TARGET qzMultimedia APPEND PROPERTY
        INTERFACE_QT_ANDROID_BUNDLED_JAR_DEPENDENCIES "${_multimedia_jar}"
    )
endif()

# PortAudio (Windows)
if(WIN32 AND NOT PORTAUDIO_ROOT)
    set(PORTAUDIO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/portaudio-19.7.0/windows")
endif()

if(WIN32)
    target_sources(qzMultimedia PRIVATE
        portaudio/PortAudioDevice.cpp portaudio/PortAudioDevice_p.h
        portaudio/PortAudioDevices.cpp portaudio/PortAudioDevices_p.h
        portaudio/PortAudioSink.cpp portaudio/PortAudioSink_p.h
    )
    target_include_directories(qzMultimedia PRIVATE "${PORTAUDIO_ROOT}/include")
    target_link_libraries(qzMultimedia PRIVATE
        "${PORTAUDIO_ROOT}/lib/libportaudio.dll.a"
        winmm
    )
endif()

# Video shaders
set(VIDEO_VERTEX_SHADERS
    "shaders/vertex.vert"
    "shaders/externalsampler.vert"
    "shaders/subtitle_index.vert"
)

set(VIDEO_SHADERS
    "shaders/externalsampler.frag"
    "shaders/abgr.frag"
    "shaders/argb.frag"
    "shaders/rgba.frag"
    "shaders/bgra.frag"
    "shaders/y.frag"
    "shaders/y16.frag"
    "shaders/nv12.frag"
    "shaders/nv21.frag"
    "shaders/p016.frag"
    "shaders/imc2.frag"
    "shaders/imc4.frag"
    "shaders/uyvy.frag"
    "shaders/yuv_triplanar.frag"
    "shaders/yuv_triplanar_p10.frag"
    "shaders/yvu_triplanar.frag"
    "shaders/yuyv.frag"
    "shaders/ayuv.frag"
    "shaders/nv12_bt2020_pq.frag"
    "shaders/nv12_bt2020_hlg.frag"
    "shaders/subtitle_index.frag"
)

set(VIDEO_SHADERS_UTILS
    "shaders/texturecomponent.glsl"
    "shaders/colortransfer.glsl"
    "shaders/colorconvert.glsl"
    "shaders/uniformbuffer.glsl"
    "shaders/hdrtonemapper.glsl"
    "shaders/roundedrect.glsl"
)

set_source_files_properties(${VIDEO_SHADERS_UTILS} PROPERTIES HEADER_FILE_ONLY ON)
target_sources(qzMultimedia PRIVATE ${VIDEO_SHADERS_UTILS})

if(TARGET Qt6::ShaderTools)
    qt_add_shaders(qzMultimedia "qtmultimedia_shaders"
        SILENT
        BATCHABLE
        PRECOMPILE
        OPTIMIZED
        PREFIX "/qz/multimedia"
        FILES ${VIDEO_VERTEX_SHADERS} ${VIDEO_SHADERS}
    )

    string(REPLACE ".frag" "_linear.frag.qsb" LINEAR_VIDEO_SHADERS "${VIDEO_SHADERS}")

    qt_add_shaders(qzMultimedia "qtmultimedia_shaders_linear"
        SILENT
        BATCHABLE
        PRECOMPILE
        OPTIMIZED
        PREFIX "/qz/multimedia"
        FILES ${VIDEO_SHADERS}
        OUTPUTS ${LINEAR_VIDEO_SHADERS}
        DEFINES QMM_OUTPUTSURFACE_LINEAR
    )
endif()

if(DEFINED QT_DEFAULT_MEDIA_BACKEND)
    target_compile_definitions(qzMultimedia PRIVATE QT_DEFAULT_MEDIA_BACKEND="${QT_DEFAULT_MEDIA_BACKEND}")
endif()
