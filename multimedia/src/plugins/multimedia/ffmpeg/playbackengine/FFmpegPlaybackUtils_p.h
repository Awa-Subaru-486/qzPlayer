#ifndef PLAYBACKENGINE_FFMPEGPLAYBACKUTILS_P_H
#define PLAYBACKENGINE_FFMPEGPLAYBACKUTILS_P_H

#include <qtypes.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegTime_p.h>

#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

namespace ffmpeg {

struct PlaybackEngineObjectID
{
    quint64 objectID = 0;
    quint64 sessionID = 0;
};

#ifndef QT_NO_DEBUG_STREAM
inline QDebug operator<<(QDebug dbg, const PlaybackEngineObjectID& id)
{
    QDebugStateSaver s(dbg);
    dbg.nospace();
    dbg << "[session: " << id.sessionID << ", object: " << id.objectID << "]";
    return dbg;
}
#endif

// 循环偏移，记录循环起始时间和循环索引
struct LoopOffset
{
    TrackPosition loopStartTimeUs =
            TrackPosition(0);
    int loopIndex = 0;
};

}

QT_END_NAMESPACE

#endif
