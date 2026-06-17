#ifndef PLAYBACKENGINE_FFMPEGSTREAMDECODER_P_H
#define PLAYBACKENGINE_FFMPEGSTREAMDECODER_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegPlaybackEngineObject_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegFrame_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegPacket_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegPlaybackUtils_p.h>
#include <qzMultimedia/private/PlatformMediaPlayer_p.h>

#include <QtCore/qqueue.h>

#include <optional>

QT_BEGIN_NAMESPACE

namespace ffmpeg {

// 流解码器，解码音频、视频和字幕流
class StreamDecoder : public PlaybackEngineObject
{
    Q_OBJECT
public:
    StreamDecoder(const PlaybackEngineObjectID &id, const CodecContext &codecContext,
                  TrackPosition absSeekPos);

    ~StreamDecoder() override;

    PlatformMediaPlayer::TrackType trackType() const;

    static qint32 maxQueueSize(PlatformMediaPlayer::TrackType type);

public slots:

    void decode(Packet);

    void onFinalPacketReceived(PlaybackEngineObjectID sourceID);

    void onFrameProcessed(Frame frame);

signals:
    void requestHandleFrame(Frame frame);

    void packetProcessed(Packet);

protected:
    bool canDoNextStep() const override;

    void doNextStep() override;

private:
    void decodeMedia(const Packet &packet);

    void decodeSubtitle(const Packet &packet);

    void onFrameFound(Frame frame);

    int sendAVPacket(const Packet &packet);

    void receiveAVFrames(bool flushPacket = false);

private:
    CodecContext m_codecContext;
    TrackPosition m_absSeekPos = TrackPosition(0);
    const PlatformMediaPlayer::TrackType m_trackType;

    qint32 m_pendingFramesCount = 0;

    qint32 m_consecutiveErrors = 0;
    static constexpr qint32 maxConsecutiveErrors = 100;

    LoopOffset m_offset;

    QQueue<Packet> m_packets;
};

}

QT_END_NAMESPACE

#endif
