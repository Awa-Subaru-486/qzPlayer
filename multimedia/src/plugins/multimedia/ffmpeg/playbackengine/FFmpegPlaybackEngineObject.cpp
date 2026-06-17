#include "playbackengine/FFmpegPlaybackEngineObject_p.h"

#include "QtCore/qchronotimer.h"
#include "QtCore/qdebug.h"
#include "QtCore/qscopedvaluerollback.h"

import qzLog;

QT_BEGIN_NAMESPACE

namespace ffmpeg {

PlaybackEngineObject::PlaybackEngineObject(const PlaybackEngineObjectID &id) : m_id{ id } { }

PlaybackEngineObject::~PlaybackEngineObject()
{
    if (!thread()->isCurrentThread())
        qz::Log::warn("The playback engine object is being removed in an unexpected thread");
}

bool PlaybackEngineObject::isPaused() const
{
    return m_paused;
}

void PlaybackEngineObject::setAtEnd(bool isAtEnd)
{
    if (m_atEnd.testAndSetRelease(!isAtEnd, isAtEnd) && isAtEnd)
        emit atEnd(id());
}

bool PlaybackEngineObject::isAtEnd() const
{
    return m_atEnd;
}

void PlaybackEngineObject::setPaused(bool isPaused)
{
    if (m_paused.testAndSetRelease(!isPaused, isPaused))
        invokePriorityMethod([this]() { onPauseChanged(); });
}

void PlaybackEngineObject::kill()
{
    m_invalidateCounter.fetch_add(1, std::memory_order_relaxed);

    disconnect();
    deleteLater();
}

bool PlaybackEngineObject::canDoNextStep() const
{
    return !m_paused;
}

QChronoTimer &PlaybackEngineObject::timer()
{
    if (!m_timer) {
        m_timer = std::make_unique<QChronoTimer>();
        m_timer->setTimerType(Qt::PreciseTimer);
        m_timer->setSingleShot(true);
        connect(m_timer.get(), &QChronoTimer::timeout, this, &PlaybackEngineObject::onTimeout);
    }

    return *m_timer;
}

// 定时器回调 - 当调度时间到达时调用。
// 触发下一步的实际执行。
void PlaybackEngineObject::onTimeout()
{
    Q_ASSERT(m_timePoint && !m_nextTimePoint && m_stepType == StepType::None);

    m_timePoint.reset();
    if (isValid() && canDoNextStep())
        doNextStep(StepType::Timeout);
}

// 返回下一步的时间点。
// 子类重写此函数以提供各自的时序:
// - Renderer: 返回下一帧应显示的时间
// - Demuxer: 返回何时读取更多数据
// 默认返回 TimePoint::min()(无特定时序)。
PlaybackEngineObject::TimePoint PlaybackEngineObject::nextTimePoint() const
{
    return TimePoint::min();
}

void PlaybackEngineObject::onPauseChanged()
{
    scheduleNextStep();
}

// 播放引擎的核心调度函数。
// 实现了精确的基于定时器的调度系统:
//
// 1. 从子类获取下一个时间点(如帧显示时间)
// 2. 如果时间已过，立即执行(StepType::Immediate)
// 3. 否则，设置精确的定时器在计算的时间触发
//
// 调度状态机:
// - StepType::None: 当前没有正在处理的步骤
// - StepType::Immediate: 步骤正在立即执行(绕过定时器)
// - StepType::Timeout: 步骤因定时器触发而执行
//
// 此设计确保音频和视频的帧精确时序。
void PlaybackEngineObject::scheduleNextStep()
{
    using std::chrono::milliseconds;
    using namespace std::chrono_literals;

    if (isValid() && canDoNextStep())
        m_nextTimePoint = nextTimePoint();
    else
        m_nextTimePoint.reset();

    if (m_stepType == StepType::Immediate)
        return;

    std::optional<TimePoint> now;

    if (m_stepType == StepType::None && m_nextTimePoint) {
        if (now = SteadyClock::now(); *m_nextTimePoint <= *now) {
            m_nextTimePoint.reset();
            doNextStep(StepType::Immediate);
            now.reset();
        }
    }

    if (m_nextTimePoint) {
        if (!now)
            now = SteadyClock::now();
        *m_nextTimePoint = std::max(*m_nextTimePoint, *now);
        if (!m_timePoint || *m_nextTimePoint != std::max(*m_timePoint, *now)) {
            timer().setInterval(*m_nextTimePoint - *now);
            timer().start();
        }
    } else if (m_timePoint) {
        timer().stop();
    }

    m_timePoint = std::exchange(m_nextTimePoint, std::nullopt);
}

void PlaybackEngineObject::doNextStep(StepType type)
{
    Q_ASSERT(m_stepType == StepType::None && type != StepType::None);
    QScopedValueRollback rollback(m_stepType, type);
    doNextStep();
}

bool PlaybackEngineObject::event(QEvent *e)
{
    if (e->type() == FuncEventType) {
        e->accept();
        static_cast<FuncEvent *>(e)->invoke();
        return true;
    }

    return QObject::event(e);
}

}

QT_END_NAMESPACE

#include "moc_FFmpegPlaybackEngineObject_p.cpp"
