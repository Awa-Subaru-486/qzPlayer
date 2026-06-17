#include "playbackengine/FFmpegRenderer_p.h"
import qzLog;

QT_BEGIN_NAMESPACE

namespace ffmpeg {

static qz::Log::LogCategory qLcRenderer("qz.multimedia.ffmpeg.renderer");

QAtomicInteger<qint64> Renderer::s_lastVideoPtsUs = 0;

Renderer::Renderer(const PlaybackEngineObjectID &id, const TimeController &tc)
    : PlaybackEngineObject(id),
      m_timeController(tc),
      m_lastFrameEnd(tc.currentPosition()),
      m_lastPosition(m_lastFrameEnd.get()),
      m_seekPos(tc.currentPosition().get())
{
    // =========================================================================
    // 渲染器基类构造函数
    // 初始化时间控制器和位置追踪
    // =========================================================================
}

TrackPosition Renderer::seekPosition() const
{
    // 获取当前跳转位置
    return TrackPosition(m_seekPos);
}

TrackPosition Renderer::lastPosition() const
{
    // 获取上次渲染位置
    return TrackPosition(m_lastPosition);
}

void Renderer::setPlaybackRate(float rate)
{
    // =========================================================================
    // 设置播放速率
    // 通过时间控制器调整播放速度
    // =========================================================================
    invokePriorityMethod([this, rate]() {
        m_timeController.setPlaybackRate(rate);
        onPlaybackRateChanged();
        scheduleNextStep();
    });
}

void Renderer::doForceStep()
{
    // =========================================================================
    // 强制执行一步(用于逐帧播放)
    // =========================================================================
    if (m_isStepForced.testAndSetOrdered(false, true))
        invokePriorityMethod([this]() {

            if (isAtEnd()) {
                // 如果已到达末尾，标记强制步完成
                setForceStepDone();
            }
            else {
                // 设置显式下一帧时间，立即执行
                m_explicitNextFrameTime = SteadyClock::now();
                scheduleNextStep();
            }
        });
}

bool Renderer::isStepForced() const
{
    // 检查是否处于强制步模式
    return m_isStepForced;
}

void Renderer::setTimeController(const TimeController &tc)
{
    // =========================================================================
    // 设置时间控制器
    // 用于同步多个渲染器(音频和视频)
    // =========================================================================
    Q_ASSERT(tc.isStarted());
    invokePriorityMethod([this, tc]() {
        m_timeController = tc;
        scheduleNextStep();
    });
}

void Renderer::onFinalFrameReceived(PlaybackEngineObjectID sourceID)
{
    // 最终帧接收回调，渲染空帧表示结束
    if (checkSessionID(sourceID.sessionID))
        render({});
}

// 来自 StreamDecoder 的解码帧入口函数。
// 此函数实现帧队列的生产者端:
// 1. 验证帧会话ID，丢弃旧播放会话的帧
// 2. 检查帧是否过期(已过跳转位置)，丢弃过期帧
// 3. 将有效帧加入 m_frames 队列
// 4. 如果队列为空(首帧到达)，触发调度
void Renderer::render(Frame frame)
{
    // =========================================================================
    // 渲染入口函数 - 来自 StreamDecoder 的解码帧
    // 这是帧队列的生产者端:
    //   1. 验证帧会话ID，丢弃旧播放会话的帧
    //   2. 检查帧是否过期(已过跳转位置)，丢弃过期帧
    //   3. 将有效帧加入队列
    //   4. 如果队列为空(首帧到达)，触发调度
    // =========================================================================
    if (frame.isValid() && !checkSessionID(frame.sourceID().sessionID)) {
        qz::Log::cat_debug(qLcRenderer, "Frame session outdated. Source id:[session:{},object:{}] current id:[session:{},object:{}]", frame.sourceID().sessionID, frame.sourceID().objectID, id().sessionID, id().objectID);

        return;
    }

    // 检查帧是否过期
    const bool frameOutdated = frame.isValid() && frame.absoluteEnd() < seekPosition();

    if (frameOutdated) {
        qz::Log::cat_debug(qLcRenderer, "frame outdated! absEnd:{} absPts:{} seekPos:{}", frame.absoluteEnd().get(), frame.absolutePts().get(), seekPosition().get());

        // 通知解码器帧已处理
        emit frameProcessed(std::move(frame));
        return;
    }

    // 诊断日志：字幕帧到达渲染器
    if (frame.isValid() && frame.hasSubtitleImage()) {
        qz::Log::cat_debug(qLcRenderer, "Subtitle frame received by renderer: imageSize={}x{} absPts={} absEnd={} seekPos={} timeControllerStarted={}",
                            frame.subtitleImage().width(), frame.subtitleImage().height(),
                            frame.absolutePts().get(), frame.absoluteEnd().get(),
                            seekPosition().get(), m_timeController.isStarted());
    } else if (frame.isValid() && !frame.text().isEmpty()) {
        qz::Log::cat_debug(qLcRenderer, "Text subtitle frame received by renderer: text='{}' absPts={} absEnd={} seekPos={}",
                            frame.text().left(30).toStdString(), frame.absolutePts().get(),
                            frame.absoluteEnd().get(), seekPosition().get());
    }

    // 将帧加入队列
    m_frames.enqueue(std::move(frame));

    // 如果这是第一个帧，触发调度并通知首帧已到达
    if (m_frames.size() == 1) {
        emit firstFrameReceived();
        scheduleNextStep();
    }
}

void Renderer::onPauseChanged()
{
    // 暂停状态改变时，更新时间控制器
    m_timeController.setPaused(isPaused());
    PlaybackEngineObject::onPauseChanged();
}

bool Renderer::canDoNextStep() const
{
    // =========================================================================
    // 检查是否可以执行下一步
    // 条件:
    //   1. 帧队列不为空
    //   2. 处于强制步模式 或 时间控制器已启动
    //   3. 父类允许执行
    // =========================================================================
    if (m_frames.empty())
        return false;

    if (m_isStepForced)
        return true;
    if (!m_timeController.isStarted())
        return false;
    return PlaybackEngineObject::canDoNextStep();
}

float Renderer::playbackRate() const
{
    return m_timeController.playbackRate();
}

TrackPosition Renderer::currentTimelinePosition(TimePoint tp) const
{
    return m_timeController.positionFromTime(tp);
}

Renderer::TimePoint Renderer::timeFromPosition(TrackPosition pos) const
{
    return m_timeController.timeFromPosition(pos);
}

Renderer::TimePoint Renderer::nextTimePoint() const
{
    // =========================================================================
    // 确定下一帧应该渲染的时间
    // 这是音视频同步的核心时序函数:
    //   - 返回队首帧的 PTS 应该显示的墙上时钟时间
    //   - 使用 TimeController 将媒体时间戳转换为真实时间
    //   - 处理逐帧播放的强制步模式
    // 调度器使用此函数设置下一次渲染周期的定时器
    // =========================================================================
    using namespace std::chrono_literals;

    if (m_frames.empty())
        return PlaybackEngineObject::nextTimePoint();

    // 如果有显式的下一帧时间，使用它
    if (m_explicitNextFrameTime)
        return *m_explicitNextFrameTime;

    // 如果队首帧有效，计算其显示时间
    if (m_frames.front().isValid())
        return m_timeController.timeFromPosition(m_frames.front().absolutePts());

    // 使用上一帧结束时间
    if (m_lastFrameEnd > TrackPosition(0))
        return m_timeController.timeFromPosition(m_lastFrameEnd);

    return PlaybackEngineObject::nextTimePoint();
}

bool Renderer::setForceStepDone()
{
    // 标记强制步完成
    if (!m_isStepForced.testAndSetOrdered(true, false))
        return false;

    m_explicitNextFrameTime.reset();
    emit forceStepDone();
    return true;
}

// 执行队列中队首帧的渲染。
// 这是帧队列的消费者端:
// 1. 获取队首帧(查看，尚未移除)
// 2. 调用子类实现的 renderInternal() (VideoRenderer/AudioRenderer)
// 3. 如果 renderInternal 返回 done=true，出队帧
// 4. 更新位置追踪 (m_lastPosition, m_lastFrameEnd, m_seekPos)
// 5. 发射 frameProcessed 信号通知解码器继续
// 6. 调度下一次渲染周期
void Renderer::doNextStep()
{
    // =========================================================================
    // 渲染线程主循环 - 执行队列中帧的渲染
    // 这是帧队列的消费者端:
    //   1. 获取队首帧(查看，尚未移除)
    //   2. 调用子类实现的 renderInternal() (VideoRenderer/AudioRenderer)
    //   3. 如果 renderInternal 返回 done=true，出队帧
    //   4. 更新位置追踪 (m_lastPosition, m_lastFrameEnd, m_seekPos)
    //   5. 发射 frameProcessed 信号通知解码器继续
    //   6. 调度下一次渲染周期
    // =========================================================================
    // 获取队首帧
    Frame frame = m_frames.front();

    // 处理强制步完成
    if (setForceStepDone()) {

    }

    // 调用子类的渲染实现
    const auto result = renderInternal(frame);
    const bool frameIsValid = frame.isValid();

    // 如果渲染完成，出队帧
    if (result.done) {
        m_explicitNextFrameTime.reset();
        m_frames.dequeue();

        if (frameIsValid) {
            // 更新位置追踪
            m_lastPosition.storeRelease(std::max(frame.absolutePts(), lastPosition()).get());

            m_lastFrameEnd = frame.absoluteEnd();
            m_seekPos.storeRelaxed(m_lastFrameEnd.get());

            // 处理循环播放
            const auto loopIndex = frame.loopOffset().loopIndex;
            if (m_loopIndex < loopIndex) {
                m_loopIndex = loopIndex;
                emit loopChanged(id(), frame.loopOffset().loopStartTimeUs, m_loopIndex);
            }

            // 通知解码器帧已处理
            emit frameProcessed(std::move(frame));
        } else {
            // 无效帧，更新位置为上一帧结束位置
            m_lastPosition.storeRelease(std::max(m_lastFrameEnd, lastPosition()).get());
        }
    } else {
        // 渲染未完成，设置重新检查时间
        m_explicitNextFrameTime = SteadyClock::now() + result.recheckInterval;
    }

    // 设置结束标志
    setAtEnd(result.done && !frameIsValid);

    // 调度下一次渲染
    scheduleNextStep();
}

// 计算当前时间与帧应显示时间之间的延迟。
// 用于 AudioRenderer 确定音频是超前还是滞后。
// 正延迟表示帧提前(应该等待)，负延迟表示帧延迟。
std::chrono::microseconds Renderer::frameDelay(const Frame &frame, TimePoint timePoint) const
{
    // =========================================================================
    // 计算帧延迟
    // 用于 AudioRenderer 确定音频是超前还是滞后
    // 正延迟表示帧提前(应该等待)，负延迟表示帧延迟
    // =========================================================================
    return std::chrono::duration_cast<std::chrono::microseconds>(
            timePoint - m_timeController.timeFromPosition(frame.absolutePts()));
}

// 按指定偏移量调整渲染器的时间参考。
// 这是音视频同步的关键函数:
// - 当音频缓冲过满或过空时，由 AudioRenderer 调用
// - 移动时间控制器以加速或减慢播放
// - 视频渲染器自动跟随调整后的时间线
// 这创建了一个反馈循环，音频驱动同步。
void Renderer::changeRendererTime(std::chrono::microseconds offset)
{
    // =========================================================================
    // 调整渲染器的时间参考
    // 这是音视频同步的关键函数:
    //   - 当音频缓冲过满或过空时，由 AudioRenderer 调用
    //   - 移动时间控制器以加速或减慢播放
    //   - 视频渲染器自动跟随调整后的时间线
    // 这创建了一个反馈循环，音频驱动同步
    // =========================================================================
    const auto now = SteadyClock::now();
    const auto pos = m_timeController.positionFromTime(now);
    m_timeController.sync(now + offset, pos);
    emit synchronized(id(), now + offset, pos);
}

}

QT_END_NAMESPACE

#include "moc_FFmpegRenderer_p.cpp"
