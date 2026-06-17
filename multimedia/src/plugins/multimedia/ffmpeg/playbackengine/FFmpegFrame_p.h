#ifndef PLAYBACKENGINE_FFMPEGFRAME_P_H
#define PLAYBACKENGINE_FFMPEGFRAME_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpeg_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegCodecContext_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegPlaybackUtils_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegTime_p.h>
#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtCore/qshareddata.h>
#include <QtGui/qimage.h>
#include <VideoFrame.h>

#include <optional>

QT_BEGIN_NAMESPACE

namespace ffmpeg {

// 帧数据结构：封装 AVFrame 或字幕文本，包含时间戳和循环偏移信息
struct Frame
{
    // Frame 内部共享数据，支持视频帧、文本和图片字幕
    struct Data
    {
        Data(const LoopOffset &offset, AVFrameUPtr f, const CodecContext &codecContext,
             const PlaybackEngineObjectID &sourceID)
            : loopOffset(offset),
              codecContext(codecContext),
              frame(std::move(f)),
              sourceID(sourceID)
        {
            Q_ASSERT(frame);
            if (frame->pts != AV_NOPTS_VALUE)
                startTime = codecContext.toTrackPosition(AVStreamPosition(frame->pts));
            else
                startTime = codecContext.toTrackPosition(
                        AVStreamPosition(frame->best_effort_timestamp));

            if (auto frameDuration = getAVFrameDuration(*frame)) {
                duration = codecContext.toTrackDuration(AVStreamDuration(frameDuration));
            } else {

                if (codecContext.context()->codec_type == AVMEDIA_TYPE_AUDIO) {
                    if (frame->sample_rate)
                        duration = TrackDuration(qint64(1000000) * frame->nb_samples
                                                 / frame->sample_rate);
                    else
                        duration = TrackDuration(0);
                } else {

                    const auto &avgFrameRate = codecContext.stream()->avg_frame_rate;
                    duration = TrackDuration(
                            mul(qint64(1000000), { avgFrameRate.den, avgFrameRate.num })
                                    .value_or(0));
                }
            }
        }
        Data(const LoopOffset &offset, const QString &text, TrackPosition pts,
             TrackDuration duration, const PlaybackEngineObjectID &sourceID,
             bool hasDuration = true)
            : loopOffset(offset), text(text), startTime(pts), duration(duration),
              hasDuration(hasDuration), sourceID(sourceID)
        {
        }

        Data(const LoopOffset &offset, const QImage &image, const QRect &rect,
             TrackPosition pts, TrackDuration duration, const PlaybackEngineObjectID &sourceID,
             bool hasDuration = true)
            : loopOffset(offset),
              subtitleImage(image),
              subtitleRect(rect),
              startTime(pts),
              duration(duration),
              hasDuration(hasDuration),
              sourceID(sourceID)
        {
        }

        Data(const LoopOffset &offset, const SubtitleBitmapData &bitmapData,
             TrackPosition pts, TrackDuration duration, const PlaybackEngineObjectID &sourceID,
             bool hasDuration = true)
            : loopOffset(offset),
              subtitleBitmapData(bitmapData),
              startTime(pts),
              duration(duration),
              hasDuration(hasDuration),
              sourceID(sourceID)
        {
        }

        QAtomicInt ref;
        LoopOffset loopOffset;
        std::optional<CodecContext> codecContext;
        AVFrameUPtr frame;
        QString text;
        QImage subtitleImage;
        QRect subtitleRect;
        SubtitleBitmapData subtitleBitmapData;
        TrackPosition startTime = 0;
        TrackDuration duration = 0;
        bool hasDuration = true;
        PlaybackEngineObjectID sourceID;
    };
    Frame() = default;

    Frame(const LoopOffset &offset, AVFrameUPtr f, const CodecContext &codecContext,
          const PlaybackEngineObjectID &sourceId)
        : d(new Data(offset, std::move(f), codecContext, sourceId))
    {
    }
    Frame(const LoopOffset &offset, const QString &text, TrackPosition pts, TrackDuration duration,
          const PlaybackEngineObjectID &sourceId, bool hasDuration = true)
        : d(new Data(offset, text, pts, duration, sourceId, hasDuration))
    {
    }
    Frame(const LoopOffset &offset, const QImage &image, const QRect &rect,
          TrackPosition pts, TrackDuration duration, const PlaybackEngineObjectID &sourceId,
          bool hasDuration = true)
        : d(new Data(offset, image, rect, pts, duration, sourceId, hasDuration))
    {
    }
    Frame(const LoopOffset &offset, const SubtitleBitmapData &bitmapData,
          TrackPosition pts, TrackDuration duration, const PlaybackEngineObjectID &sourceId,
          bool hasDuration = true)
        : d(new Data(offset, bitmapData, pts, duration, sourceId, hasDuration))
    {
    }
    bool isValid() const { return !!d; }

    AVFrame *avFrame() const { return data().frame.get(); }
    AVFrameUPtr takeAVFrame() { return std::move(data().frame); }
    const CodecContext *codecContext() const
    {
        return data().codecContext ? &data().codecContext.value() : nullptr;
    }
    TrackPosition startTime() const { return data().startTime; }
    TrackDuration duration() const { return data().duration; }
    TrackPosition endTime() const { return data().startTime + data().duration; }
    bool hasDuration() const { return data().hasDuration; }
    QString text() const { return data().text; }
    QImage subtitleImage() const { return data().subtitleImage; }
    QRect subtitleRect() const { return data().subtitleRect; }
    bool hasSubtitleImage() const { return !data().subtitleImage.isNull(); }
    SubtitleBitmapData subtitleBitmapData() const { return data().subtitleBitmapData; }
    bool hasSubtitleBitmapData() const { return data().subtitleBitmapData.isValid(); }
    const PlaybackEngineObjectID &sourceID() const { return data().sourceID; };
    const LoopOffset &loopOffset() const { return data().loopOffset; };
    TrackPosition absolutePts() const
    {
        return startTime() + loopOffset().loopStartTimeUs.asDuration();
    }
    TrackPosition absoluteEnd() const
    {
        return endTime() + loopOffset().loopStartTimeUs.asDuration();
    }

private:
    Data &data() const
    {
        Q_ASSERT(d);
        return *d;
    }

private:
    QExplicitlySharedDataPointer<Data> d;
};

}

QT_END_NAMESPACE

Q_DECLARE_METATYPE(ffmpeg::Frame);

#endif
