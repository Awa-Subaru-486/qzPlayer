#ifndef PLAYBACKENGINE_FFMPEGRENDERER_P_H
#define PLAYBACKENGINE_FFMPEGRENDERER_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegPlaybackEngineObject_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegTimeController_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegFrame_p.h>

#include <QtCore/qpointer.h>
#include <QtCore/qqueue.h>
#include <QtCore/qatomic.h>

#include <chrono>

QT_BEGIN_NAMESPACE

namespace ffmpeg {

// 渲染器基类，提供时间同步、帧队列和播放速率控制
class Renderer : public PlaybackEngineObject
{
    Q_OBJECT
public:
    using TimePoint = SteadyClock::time_point;

    Renderer(const PlaybackEngineObjectID &id, const TimeController &tc);

    TrackPosition seekPosition() const;

    TrackPosition lastPosition() const;

    void setPlaybackRate(float rate);

    void doForceStep();

    bool isStepForced() const;

    void setTimeController(const TimeController &tc);

public slots:

    void onFinalFrameReceived(PlaybackEngineObjectID sourceID);

    void render(Frame);

signals:
    void frameProcessed(Frame);

    void synchronized(PlaybackEngineObjectID id, TimePoint tp, TrackPosition pos);

    void forceStepDone();

    void loopChanged(PlaybackEngineObjectID id, TrackPosition offset, int index);

    void firstFrameReceived();

protected:
    bool setForceStepDone();

    void onPauseChanged() override;

    bool canDoNextStep() const override;

    TimePoint nextTimePoint() const override;

    virtual void onPlaybackRateChanged() { }

    TrackPosition currentTimelinePosition(TimePoint tp = SteadyClock::now()) const;

    TimePoint timeFromPosition(TrackPosition pos) const;

    static QAtomicInteger<qint64> s_lastVideoPtsUs;

    // 渲染结果，包含完成标志和重新检查间隔
    struct RenderingResult
    {
        bool done = true;
        std::chrono::microseconds recheckInterval = std::chrono::microseconds(0);
    };

    virtual RenderingResult renderInternal(Frame frame) = 0;

    float playbackRate() const;

    std::chrono::microseconds frameDelay(const Frame &frame,
                                         TimePoint timePoint = SteadyClock::now()) const;

    void changeRendererTime(std::chrono::microseconds offset);

    template<typename Output, typename ChangeHandler>
    void setOutputInternal(QPointer<Output> &actual, Output *desired, ChangeHandler &&changeHandler)
    {
        const auto connectionType =
                thread()->isCurrentThread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection;
        auto doer = [desired, changeHandler, &actual]() {
            const auto prev = std::exchange(actual, desired);
            if (prev != desired)
                changeHandler(prev);
        };
        QMetaObject::invokeMethod(this, doer, connectionType);
    }

private:
    void doNextStep() override;

private:
    TimeController m_timeController;
    TrackPosition m_lastFrameEnd = TrackPosition(0);
    QAtomicInteger<qint64> m_lastPosition = 0;
    QAtomicInteger<qint64> m_seekPos = 0;

    int m_loopIndex = 0;
    QQueue<Frame> m_frames;

    QAtomicInteger<bool> m_isStepForced = false;
    std::optional<TimePoint> m_explicitNextFrameTime;
};

}

QT_END_NAMESPACE

#endif
