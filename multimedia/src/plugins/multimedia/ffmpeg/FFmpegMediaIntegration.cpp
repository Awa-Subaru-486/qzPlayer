#include <qzFFmpegMediaPluginImpl/private/FFmpegMediaIntegration_p.h>

#include <qzFFmpegMediaPluginImpl/private/FFmpegAudioInput_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegConverter_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegMediaFormatInfo_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegMediaPlayer_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegPreviewFrameProvider_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegResampler_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegVideoSink_p.h>

#include <qzMultimedia/private/PlatformMediaPlugin_p.h>

#ifdef Q_OS_WINDOWS
#  include <qzMultimedia/private/WindowsResampler_p.h>
#endif

#ifdef Q_OS_ANDROID
#  include <qzFFmpegMediaPluginImpl/private/FFmpegHwAccel_MediaCodec_p.h>
#  include <qzFFmpegMediaPluginImpl/private/AndroidSurfaceTexture_p.h>
#  include <jni.h>
extern "C" {
#  include <libavutil/log.h>
#  include <libavcodec/jni.h>
}
#endif

import qzLog;

QT_BEGIN_NAMESPACE

namespace ffmpeg {
qz::Log::LogCategory qLcFFmpegMediaIntegration("qz.multimedia.ffmpeg.integration");

MediaIntegration::MediaIntegration()
    : PlatformMediaIntegration(QLatin1String("ffmpeg"))
{
}

PlatformMediaFormatInfo *MediaIntegration::createFormatInfo()
{
    return new MediaFormatInfo;
}

std::expected<std::unique_ptr<PlatformAudioResampler>, QString>
MediaIntegration::createAudioResampler(const ::AudioFormat &inputFormat,
                                               const ::AudioFormat &outputFormat)
{
#ifdef Q_OS_WINDOWS
    Q_UNUSED(inputFormat);
    Q_UNUSED(outputFormat);
    return std::make_unique<WindowsResampler>();
#else
    return Resampler::createFromInputFormat(inputFormat, outputFormat);
#endif
}

std::expected<PlatformMediaPlayer *, QString>
MediaIntegration::createPlayer(::MediaPlayer *player)
{
    return new MediaPlayer(player);
}

std::expected<PlatformVideoSink *, QString> MediaIntegration::createVideoSink(::VideoSink *sink)
{
    return new VideoSink(sink);
}

PlatformPreviewFrameProvider *MediaIntegration::createPreviewFrameProvider()
{
    return new ffmpeg::PreviewFrameProvider();
}

std::expected<PlatformAudioInput *, QString> MediaIntegration::createAudioInput(::AudioInput *input)
{
    return new AudioInput(input);
}

::VideoFrame MediaIntegration::convertVideoFrame(::VideoFrame &srcFrame,
                                                       const VideoFrameFormat &destFormat)
{
    Q_UNUSED(srcFrame);
    Q_UNUSED(destFormat);
    return {};
}
}
QT_END_NAMESPACE

#ifdef Q_OS_ANDROID

#include <android/log.h>
#define LOG_TAG "FFmpegJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" Q_DECL_EXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void * /*reserved*/)
{
    static bool initialized = false;
    if (initialized)
        return JNI_VERSION_1_6;
    initialized = true;

    LOGI("JNI_OnLoad: start");

    QT_USE_NAMESPACE
    void *environment;
    if (vm->GetEnv(&environment, JNI_VERSION_1_6)) {
        LOGE("JNI_OnLoad: GetEnv failed");
        return JNI_ERR;
    }
    LOGI("JNI_OnLoad: GetEnv ok");

    // setting our javavm into ffmpeg
    if (av_jni_set_java_vm(vm, nullptr)) {
        LOGE("JNI_OnLoad: av_jni_set_java_vm failed");
        return JNI_ERR;
    }
    LOGI("JNI_OnLoad: av_jni_set_java_vm ok");

    if (!AndroidSurfaceTexture::registerNativeMethods()) {
        LOGE("JNI_OnLoad: registerNativeMethods failed");
        return JNI_ERR;
    }
    LOGI("JNI_OnLoad: registerNativeMethods ok");

    LOGI("JNI_OnLoad: success");
    return JNI_VERSION_1_6;
}

#endif
