#include "FFmpegThread_p.h"

QT_BEGIN_NAMESPACE

namespace ffmpeg {
void ConsumerThread::stopAndDelete()
{
    {
        QMutexLocker locker(&m_loopDataMutex);
        m_exit = true;
    }
    dataReady();
    wait();
    delete this;
}

void ConsumerThread::dataReady()
{
    m_condition.wakeAll();
}

void ConsumerThread::run()
{
    if (!init())
        return;

    while (true) {

        {
            QMutexLocker locker(&m_loopDataMutex);
            while (!hasData() && !m_exit)
                m_condition.wait(&m_loopDataMutex);

            if (m_exit)
                break;
        }

        processOne();
    }

    cleanup();
}

QMutexLocker<QMutex> ConsumerThread::lockLoopData() const
{
    return QMutexLocker(&m_loopDataMutex);
}
}
QT_END_NAMESPACE
