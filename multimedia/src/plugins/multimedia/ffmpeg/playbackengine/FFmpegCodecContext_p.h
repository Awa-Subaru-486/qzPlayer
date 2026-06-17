#ifndef PLAYBACKENGINE_FFMPEGCODECCONTEXT_P_H
#define PLAYBACKENGINE_FFMPEGCODECCONTEXT_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpeg_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegHwAccel_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegTime_p.h>

#include <QtCore/qshareddata.h>
#include <expected>
#include <qzMultimedia/PlaybackOptions.h>

QT_BEGIN_NAMESPACE

class QRhi;

namespace ffmpeg {

// 编解码器上下文管理，支持硬件解码自动选择与回退
class CodecContext
{
    // CodecContext 内部共享数据，存储 AVCodecContext 和 HWAccel
    struct Data : QSharedData
    {
        Data(AVCodecContextUPtr context, AVStream *avStream, AVFormatContext *formatContext,
             std::unique_ptr<HWAccel> hwAccel);
        AVCodecContextUPtr context;
        AVStream *stream = nullptr;
        AVFormatContext *formatContext = nullptr;
        AVRational pixelAspectRatio = { 0, 1 };
        std::unique_ptr<HWAccel> hwAccel;
    };

public:
    // 创建编解码器上下文
    static std::expected<CodecContext, QString> create(AVStream *stream, AVFormatContext *formatContext,
                                       const ::PlaybackOptions &options, QRhi *rhi = nullptr);

    // 像素宽高比
    AVRational pixelAspectRatio(AVFrame *frame) const;

    [[nodiscard]] AVCodecContext *context() const { return d->context.get(); }
    [[nodiscard]] AVStream *stream() const { return d->stream; }
    [[nodiscard]] AVFormatContext *formatContext() const { return d->formatContext; }
    [[nodiscard]] uint streamIndex() const { return d->stream->index; }
    [[nodiscard]] HWAccel *hwAccel() const { return d->hwAccel.get(); }
    [[nodiscard]] TrackDuration toTrackDuration(AVStreamDuration duration) const
    {
        return ffmpeg::toTrackDuration(duration, d->stream);
    }

    [[nodiscard]] TrackPosition toTrackPosition(AVStreamPosition streamPosition) const
    {
        return ffmpeg::toTrackPosition(streamPosition, d->stream, d->formatContext);
    }

private:
    static std::expected<CodecContext, QString> tryCreateDecoder(
        AVStream *stream, AVFormatContext *formatContext,
        const ::PlaybackOptions &options,
        ::PlaybackOptions::VideoDecoderPolicy policy,
        QRhi *rhi);

    static std::expected<CodecContext, QString> createSoftwareDecoder(
        AVStream *stream, AVFormatContext *formatContext,
        const ::PlaybackOptions &options, QRhi *rhi = nullptr);

    static std::expected<CodecContext, QString> createHardwareDecoder(
        AVStream *stream, AVFormatContext *formatContext,
        const ::PlaybackOptions &options,
        ::PlaybackOptions::VideoDecoderPolicy policy,
        QRhi *rhi);

    static std::expected<CodecContext, QString> createInternal(
        AVStream *stream, AVFormatContext *formatContext,
        const ::PlaybackOptions &options,
        bool useHardware,
        AVHWDeviceType hwDeviceType,
        QRhi *rhi);

    CodecContext(Data *data) : d(data) { }

    static CodecContext make(AVCodecContextUPtr context, AVStream *avStream,
                             AVFormatContext *formatContext, std::unique_ptr<HWAccel> hwAccel)
    {
        return {new Data(std::move(context), avStream, formatContext, std::move(hwAccel))};
    }

    QExplicitlySharedDataPointer<Data> d;
};

}

QT_END_NAMESPACE

#endif
