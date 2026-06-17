#ifndef PLAYBACKENGINE_FFMPEGDEMUXER_P_H
#define PLAYBACKENGINE_FFMPEGDEMUXER_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegPlaybackEngineObject_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegPacket_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegPlaybackUtils_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegTime_p.h>
#include <qzMultimedia/private/PlatformMediaPlayer_p.h>

#include <unordered_map>

QT_BEGIN_NAMESPACE

namespace ffmpeg {

// 解复用器，从媒体文件中分离各路流数据
class Demuxer : public PlaybackEngineObject
{
    Q_OBJECT
public:
    Demuxer(const PlaybackEngineObjectID &id, AVFormatContext *context, TrackPosition initialPosUs,
            bool seekPending, const LoopOffset &loopOffset, const StreamIndexes &streamIndexes,
            int loops);

    using RequestingSignal = void (Demuxer::*)(Packet);
    static RequestingSignal signalByTrackType(PlatformMediaPlayer::TrackType trackType);

    void setLoops(int loopsCount);

    void setOpening(TrackPosition pos);
    void setEnding(TrackPosition pos);

public slots:
    void onPacketProcessed(Packet);

signals:
    void requestProcessAudioPacket(Packet);
    void requestProcessVideoPacket(Packet);
    void requestProcessSubtitlePacket(Packet);
    void firstPacketFound(PlaybackEngineObjectID id, TrackPosition absSeekPos);
    void packetsBuffered();
    void bufferProgressChanged(qint64 bufferedEndPositionUs);

protected:
    TimePoint nextTimePoint() const override;

private:
    bool canDoNextStep() const override;

    void doNextStep() override;

    void ensureSeeked();

private:
    struct StreamData
    {
        PlatformMediaPlayer::TrackType trackType = PlatformMediaPlayer::TrackType::NTrackTypes;
        TrackDuration bufferedDuration = TrackDuration(0);
        qint64 bufferedSize = 0;

        TrackPosition maxSentPacketsPos = TrackPosition(0);
        TrackPosition maxProcessedPacketPos = TrackPosition(0);

        bool isDataLimitReached = false;
    };

    void updateStreamDataLimitFlag(StreamData &streamData);

private:
    AVFormatContext *m_context = nullptr;
    bool m_seeked = false;
    bool m_firstPacketFound = false;
    std::unordered_map<int, StreamData> m_streams;
    TrackPosition m_posInLoopUs = TrackPosition(0);
    LoopOffset m_loopOffset;
    TrackPosition m_maxPacketsEndPos = TrackPosition(0);
    QAtomicInt m_loops = 1;
    TrackPosition m_openingUs = TrackPosition(0);
    TrackPosition m_endingUs = TrackPosition(0);
    bool m_buffered = false;
    qsizetype m_demuxerRetryCount = 0;
    std::optional<TimePoint> m_failTimePoint;
    static constexpr qsizetype s_maxDemuxerRetries = 10;
    static constexpr std::chrono::milliseconds s_demuxerRetryInterval = std::chrono::milliseconds(10);
};

}

QT_END_NAMESPACE

#endif
