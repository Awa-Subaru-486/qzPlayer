#ifndef FFMPEGMEDIAPLAYER_P_H
#define FFMPEGMEDIAPLAYER_P_H

#include <qzMultimedia/private/PlatformMediaPlayer_p.h>
#include <MediaMetadata.h>
#include <qtimer.h>
#include <qpointer.h>
#include <qfuture.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpeg_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegMediaDataHolder_p.h>
#include <qzMultimedia/SubtitleStyle.h>

QT_BEGIN_NAMESPACE

class PlatformAudioOutput;

namespace ffmpeg {

class CancelToken;
class PlaybackEngine;

// FFmpeg 媒体播放器实现，提供播放控制和轨道管理
class MediaPlayer : public QObject, public PlatformMediaPlayer
{
    Q_OBJECT
public:
    explicit MediaPlayer(::MediaPlayer *player);
    ~MediaPlayer() override;

    qint64 duration() const override;

    void setPosition(qint64 position) override;

    float bufferProgress() const override;

    ::MediaTimeRange availablePlaybackRanges() const override;

    qreal playbackRate() const override;
    void setPlaybackRate(qreal rate) override;

    QUrl media() const override;
    const QIODevice *mediaStream() const override;
    void setMedia(const QUrl &media, QIODevice *stream) override;

    void play() override;
    void pause() override;
    void stop() override;

    void onAudioOutputDeviceChanged() override;

    void setAudioOutput(PlatformAudioOutput *) override;

    void setAudioBufferOutput(::AudioBufferOutput *) override;

    ::MediaMetaData metaData() const override;

    QList<ChapterInfo> chapters() const override;

    void cancelCoverRequest() override;

    void setVideoSink(::VideoSink *sink) override;
    ::VideoSink *videoSink() const;

    int trackCount(TrackType) override;
    ::MediaMetaData trackMetaData(TrackType type, int streamNumber) override;
    int activeTrack(TrackType) override;
    void setActiveTrack(TrackType, int streamNumber) override;
    void setLoops(int loops) override;

    PitchCompensationAvailability pitchCompensationAvailability() const override;
    void setPitchCompensation(bool enabled) override;
    bool pitchCompensation() const override;

    void setPlaybackOptions(const ::PlaybackOptions &options) override;

    void setSubtitleStyle(const SubtitleStyle &style) override;

    void setOpening(qint64 ms) override;
    void setEnding(qint64 ms) override;

    // 片头片尾抑制标志
    bool m_suppressOpening = false;  // 播放中更新片头时，若当前在片头区域内则抑制片头跳过
    bool m_suppressEnding = false;   // 播放中更新片尾时，若当前在片尾区域内则抑制片尾跳过

private:
    void runPlayback();
    void handleIncorrectMedia(::MediaPlayer::MediaStatus status);
    void setMediaAsync(MediaDataHolder::Maybe mediaDataHolder,
                       const std::shared_ptr<CancelToken> &cancelToken);

    void mediaStatusChanged(::MediaPlayer::MediaStatus) override;

    QImage getMediaCover(bool decodeFrame = true) override;

private slots:
    void updatePosition();
    void endOfStream();
    void error(::MediaPlayer::Error error, const QString &errorString)
    {
        PlatformMediaPlayer::error(error, errorString);
    }
    void onLoopChanged();
    void onBuffered();
    void onBufferProgressChanged(qint64 bufferedEndPositionUs);
    void onFirstFrameRendered();

private:
    QTimer m_positionUpdateTimer;
    ::MediaPlayer::PlaybackState m_requestedStatus = ::MediaPlayer::StoppedState;

    std::unique_ptr<PlaybackEngine> m_playbackEngine;
    PlatformAudioOutput *m_audioOutput = nullptr;
    QPointer<::AudioBufferOutput> m_audioBufferOutput;
    QPointer<::VideoSink> m_videoSink;

    QUrl m_url;
    QPointer<QIODevice> m_device;
    float m_playbackRate = 1.;
    float m_bufferProgress = 0.f;
    qint64 m_bufferedEndPositionUs = 0;
    QFuture<void> m_loadMedia;
    std::shared_ptr<CancelToken> m_cancelToken;
    std::shared_ptr<CancelToken> m_coverCancelToken;

    bool m_pitchCompensation = true;
    SubtitleStyle m_subtitleStyle;
};

}

QT_END_NAMESPACE

#endif
