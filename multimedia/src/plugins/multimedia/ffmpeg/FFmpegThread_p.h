#ifndef FFMPEGTHREAD_P_H
#define FFMPEGTHREAD_P_H

#include <qzMultimedia/private/MultimediaGlobal_p.h>

#include <qmutex.h>
#include <qwaitcondition.h>
#include <qthread.h>

QT_BEGIN_NAMESPACE

class AudioSink;

namespace ffmpeg
{

// 消费者线程基类，提供线程安全的循环处理框架
class ConsumerThread : public QThread
{
public:
    // ConsumerThread 删除器，确保线程安全停止
    struct Deleter
    {
        void operator()(ConsumerThread *thread) const { thread->stopAndDelete(); }
    };

protected:

    void stopAndDelete();

    virtual bool init() = 0;

    virtual void cleanup() = 0;

    virtual void processOne() = 0;

    void dataReady();

    virtual bool hasData() const = 0;

    QMutexLocker<QMutex> lockLoopData() const;

private:
    void run() final;

    mutable QMutex m_loopDataMutex;
    QWaitCondition m_condition;
    bool m_exit = false;
};

template <typename T>
using ConsumerThreadUPtr = std::unique_ptr<T, ConsumerThread::Deleter>;
}

QT_END_NAMESPACE

#endif
