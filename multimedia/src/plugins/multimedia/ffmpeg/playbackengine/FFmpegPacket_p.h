#ifndef PLAYBACKENGINE_FFMPEGPACKET_P_H
#define PLAYBACKENGINE_FFMPEGPACKET_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpeg_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegPlaybackUtils_p.h>
#include <QtCore/qshareddata.h>

QT_BEGIN_NAMESPACE

namespace ffmpeg {

// 数据包结构：封装 AVPacket，包含循环偏移和源标识
struct Packet
{
    // Packet 内部共享数据，存储循环偏移、AVPacket 和对象标识
    struct Data : QSharedData
    {
        Data(const LoopOffset &offset, AVPacketUPtr p, const PlaybackEngineObjectID &sourceID)
            : loopOffset(offset), packet(std::move(p)), sourceID(sourceID)
        {
        }

        LoopOffset loopOffset;
        AVPacketUPtr packet;
        PlaybackEngineObjectID sourceID;
    };
    Packet() = default;
    Packet(const LoopOffset &offset, AVPacketUPtr p, const PlaybackEngineObjectID &sourceId)
        : d(new Data(offset, std::move(p), sourceId))
    {
    }

    bool isValid() const { return !!d; }
    AVPacket *avPacket() const { return d->packet.get(); }
    const LoopOffset &loopOffset() const { return d->loopOffset; }
    const PlaybackEngineObjectID &sourceID() const { return d->sourceID; }

private:
    QExplicitlySharedDataPointer<Data> d;
};

}

QT_END_NAMESPACE

Q_DECLARE_METATYPE(ffmpeg::Packet)

#endif
