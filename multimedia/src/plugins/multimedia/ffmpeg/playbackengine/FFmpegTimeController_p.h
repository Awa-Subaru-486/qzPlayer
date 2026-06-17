#ifndef PLAYBACKENGINE_FFMPEGTIMECONTROLLER_P_H
#define PLAYBACKENGINE_FFMPEGTIMECONTROLLER_P_H

#include "qglobal.h"
#include <qzFFmpegMediaPluginImpl/private/FFmpegTime_p.h>

#include <chrono>
#include <optional>

QT_BEGIN_NAMESPACE

namespace ffmpeg {

// 时间控制器，管理播放时间同步和速率控制
class TimeController
{
public:
    using TimePoint = SteadyClock::time_point;
    using PlaybackRate = float;

    TimeController();

    PlaybackRate playbackRate() const;

    void setPlaybackRate(PlaybackRate playbackRate);

    void sync(TrackPosition trackPos = TrackPosition(0));

    void sync(TimePoint tp, TrackPosition pos);

    void syncSoft(TimePoint tp, TrackPosition pos,
                  SteadyClock::duration fixingTime = std::chrono::seconds(4));

    TrackPosition currentPosition(SteadyClock::duration offset = SteadyClock::duration{ 0 }) const;

    void start();
    void setPaused(bool paused);
    void deactivate();

    TrackPosition positionFromTime(TimePoint tp, bool ignoreInactive = false) const;

    TimePoint timeFromPosition(TrackPosition pos, bool ignoreInactive = false) const;

    bool isStarted() const { return m_started; }
    bool isActive() const { return m_active; }

private:
    struct SoftSyncData
    {
        TimePoint srcTimePoint;
        TrackPosition srcPosition = 0;
        TimePoint dstTimePoint;
        TrackDuration srcPosOffest = 0;
        TrackPosition dstPosition = 0;
        PlaybackRate internalRate = 1;
    };

    void updateActive();

    SoftSyncData makeSoftSyncData(const TimePoint &srcTp, const TrackPosition &srcPos,
                                  const TimePoint &dstTp) const;

    TrackPosition positionFromTimeInternal(const TimePoint &tp) const;

    TimePoint timeFromPositionInternal(const TrackPosition &pos) const;

    void scrollTimeTillNow();

    static SteadyClock::duration toClockDuration(TrackDuration trackDuration,
                                               PlaybackRate rate = 1.f);

    static TrackDuration toTrackDuration(SteadyClock::duration clockDuration, PlaybackRate rate);

private:
    bool m_paused = true;
    bool m_started = false;
    bool m_active = false;
    PlaybackRate m_playbackRate = 1;
    TrackPosition m_position = 0;
    TimePoint m_timePoint;
    std::optional<SoftSyncData> m_softSyncData;
};

}

QT_END_NAMESPACE

#endif
