#ifndef PLAYBACKENGINE_FFMPEGTIME_P_H
#define PLAYBACKENGINE_FFMPEGTIME_P_H

#include "qglobal.h"

#include <qzMultimedia/private/TaggedTime_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpeg_p.h>

#include <chrono>

QT_BEGIN_NAMESPACE

namespace ffmpeg {

using namespace std::chrono_literals;

// 轨道时间标签
struct TrackTimeTag;

// 轨道位置和时长类型
using TrackPosition = TaggedTimePoint<qint64, TrackTimeTag>;
using TrackDuration = TaggedDuration<qint64, TrackTimeTag>;

// 用户轨道时间标签
struct UserTrackTimeTag;

using UserTrackPosition = TaggedTimePoint<qint64, UserTrackTimeTag>;
using UserTrackDuration = TaggedDuration<qint64, UserTrackTimeTag>;

// AV 流时间标签
struct AVStreamTimeTag;

using AVStreamPosition = TaggedTimePoint<qint64, AVStreamTimeTag>;
using AVStreamDuration = TaggedDuration<qint64, AVStreamTimeTag>;

// AV 上下文时间标签
struct AVContextTimeTag;

using AVContextPosition = TaggedTimePoint<qint64, AVContextTimeTag>;
using AVContextDuration = TaggedDuration<qint64, AVContextTimeTag>;

// 稳定时钟
using SteadyClock = std::chrono::steady_clock;

inline AVContextDuration contextStartOffset(const AVFormatContext *formatContext)

{
    return AVContextDuration(
            formatContext->start_time == AV_NOPTS_VALUE ? 0 : formatContext->start_time);
}

inline UserTrackPosition toUserPosition(TrackPosition trackPosition)
{
    return UserTrackPosition(trackPosition.get() / 1000);
}

inline UserTrackDuration toUserDuration(TrackDuration trackDuration)
{
    return UserTrackDuration(trackDuration.get() / 1000);
}

inline TrackDuration toTrackDuration(AVContextDuration contextDuration)
{
    return TrackDuration(contextDuration.get() * 1'000'000 / AV_TIME_BASE);
}

inline TrackPosition toTrackPosition(UserTrackPosition userTrackPosition)
{
    return TrackPosition(userTrackPosition.get() * 1000);
}

inline TrackDuration toTrackDuration(UserTrackDuration userTrackDuration)
{
    return TrackDuration(userTrackDuration.get() * 1000);
}

inline TrackDuration toTrackDuration(AVStreamDuration streamDuration, const AVStream *avStream)
{
    return TrackDuration(timeStampUs(streamDuration.get(), avStream->time_base).value_or(0));
}

inline TrackPosition toTrackPosition(AVStreamPosition streamPosition, const AVStream *avStream,
                                     const AVFormatContext *formatContext)
{
    const auto duration = toTrackDuration(streamPosition.asDuration(), avStream)
            - toTrackDuration(contextStartOffset(formatContext));
    return duration.asTimePoint();
}

inline AVContextPosition toContextPosition(TrackPosition trackPosition,
                                           const AVFormatContext *formatContext)
{

    return AVContextPosition(trackPosition.get() * AV_TIME_BASE / 1'000'000)
            + contextStartOffset(formatContext);
}

}

QT_END_NAMESPACE

#endif
