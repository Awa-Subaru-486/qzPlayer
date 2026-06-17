#include "playbackengine/FFmpegTimeController_p.h"

#include "qglobal.h"
#include "qdebug.h"

#include <algorithm>

QT_BEGIN_NAMESPACE

namespace ffmpeg {

TimeController::TimeController()
{
    sync();
}

TimeController::PlaybackRate TimeController::playbackRate() const
{
    return m_playbackRate;
}

void TimeController::setPlaybackRate(PlaybackRate playbackRate)
{
    if (playbackRate == m_playbackRate)
        return;

    Q_ASSERT(playbackRate > 0.f);

    scrollTimeTillNow();
    m_playbackRate = playbackRate;

    if (m_softSyncData)
        m_softSyncData = makeSoftSyncData(m_timePoint, m_position, m_softSyncData->dstTimePoint);
}

void TimeController::sync(TrackPosition trackPos)
{
    sync(SteadyClock::now(), trackPos);
}

// 使用指定的时间点和轨道位置同步时间控制器。
// 执行硬同步，重置所有软同步数据。
void TimeController::sync(TimePoint tp, TrackPosition pos)
{
    m_softSyncData.reset();
    m_position = pos;
    m_timePoint = tp;
}

// 在指定的修正时间内执行渐进式软同步。
// 当播放位置需要校正时，允许平滑过渡，
// 避免用户可察觉的时间线突然跳变。
void TimeController::syncSoft(TimePoint tp, TrackPosition pos, SteadyClock::duration fixingTime)
{
    const auto srcTime = SteadyClock::now();
    const auto srcPos = positionFromTime(srcTime, true);
    const auto dstTime = srcTime + fixingTime;

    m_position = pos;
    m_timePoint = tp;

    m_softSyncData = makeSoftSyncData(srcTime, srcPos, dstTime);
}

TrackPosition TimeController::currentPosition(SteadyClock::duration offset) const
{
    return positionFromTime(SteadyClock::now() + offset);
}
void TimeController::start()
{
    m_started = true;
    updateActive();
}

void TimeController::deactivate()
{
    m_started = false;
    m_paused = true;
    updateActive();
}

void TimeController::setPaused(bool paused)
{
    m_paused = paused;
    updateActive();
}

void TimeController::updateActive()
{
    const bool active = !m_paused && m_started;
    if (m_active == active)
        return;

    scrollTimeTillNow();

    m_active = active;
}

// 将真实世界时间点转换为轨道位置(媒体时间戳)。
// 这是音视频同步的核心函数:
// - 将墙上时钟时间映射到媒体时间线位置
// - 处理软同步以实现平滑的位置过渡
// - 考虑播放速率(速度)调整
// 公式: position = m_position + (tp - m_timePoint) * playbackRate
TrackPosition TimeController::positionFromTime(TimePoint tp, bool ignoreInactive) const
{
    tp = !m_active && !ignoreInactive ? m_timePoint : tp;

    if (m_softSyncData && tp < m_softSyncData->dstTimePoint) {
        const PlaybackRate rate =
                tp > m_softSyncData->srcTimePoint ? m_softSyncData->internalRate : m_playbackRate;

        return m_softSyncData->srcPosition
                + toTrackDuration(tp - m_softSyncData->srcTimePoint, rate);
    }

    return positionFromTimeInternal(tp);
}

// 将轨道位置(媒体时间戳)转换为真实世界时间点。
// 这是 positionFromTime 的逆函数，用于确定特定帧应何时渲染:
// - 将媒体时间线位置映射到墙上时钟时间
// - 由渲染器用于调度帧显示
// - 对帧精确的视频同步至关重要
// 公式: timePoint = m_timePoint + (pos - m_position) / playbackRate
TimeController::TimePoint TimeController::timeFromPosition(TrackPosition pos,
                                                           bool ignoreInactive) const
{
    auto position = !m_active && !ignoreInactive ? m_position : TrackPosition(pos);

    if (m_softSyncData && position < m_softSyncData->dstPosition) {
        const auto rate = position > m_softSyncData->srcPosition ? m_softSyncData->internalRate
                                                                 : m_playbackRate;
        return m_softSyncData->srcTimePoint
                + toClockDuration(position - m_softSyncData->srcPosition, rate);
    }

    return timeFromPositionInternal(position);
}

TimeController::SoftSyncData TimeController::makeSoftSyncData(const TimePoint &srcTp,
                                                              const TrackPosition &srcPos,
                                                              const TimePoint &dstTp) const
{
    SoftSyncData result;
    result.srcTimePoint = srcTp;
    result.srcPosition = srcPos;
    result.dstTimePoint = dstTp;
    result.srcPosOffest = srcPos - positionFromTimeInternal(srcTp);
    result.dstPosition = positionFromTimeInternal(dstTp);
    result.internalRate =
            static_cast<PlaybackRate>(toClockDuration(result.dstPosition - srcPos).count())
            / (dstTp - srcTp).count();

    return result;
}

// 时间到位置转换的内部实现。
// 应用核心公式，不处理软同步。
// 这是驱动整个同步系统的基础计算。
TrackPosition TimeController::positionFromTimeInternal(const TimePoint &tp) const
{
    return m_position + toTrackDuration(tp - m_timePoint, m_playbackRate);
}

// 位置到时间转换的内部实现。
// 应用逆公式，不处理软同步。
// 用于计算帧应何时显示。
TimeController::TimePoint TimeController::timeFromPositionInternal(const TrackPosition &pos) const
{
    return m_timePoint + toClockDuration(pos - m_position, m_playbackRate);
}

// 将内部时间参考更新到当前时刻。
// 当时间控制器状态改变时(暂停/恢复、跳转等)调用此函数，
// 以确保时间计算准确。它将时间"滚动"到当前并相应地更新位置，
// 维持真实时间与媒体时间之间的关系。
void TimeController::scrollTimeTillNow()
{
    const auto now = SteadyClock::now();
    if (m_active) {
        m_position = positionFromTimeInternal(now);

        if (m_softSyncData && m_softSyncData->dstTimePoint <= now)
            m_softSyncData.reset();
    } else if (m_softSyncData) {
        m_softSyncData->dstTimePoint += now - m_timePoint;
        m_softSyncData->srcTimePoint += now - m_timePoint;
    }

    m_timePoint = now;
}

SteadyClock::duration TimeController::toClockDuration(TrackDuration trackDuration, PlaybackRate rate)
{
    return std::chrono::duration_cast<SteadyClock::duration>(
            std::chrono::microseconds(trackDuration.get()) / rate);
}

TrackDuration TimeController::toTrackDuration(SteadyClock::duration clockDuration, PlaybackRate rate)
{
    return TrackDuration(
            std::chrono::duration_cast<std::chrono::microseconds>(clockDuration * rate).count());
}

}

QT_END_NAMESPACE
