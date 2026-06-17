// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_PLAYBACK_MEDIAPLAYER_H
#define QT_PLAYBACK_MEDIAPLAYER_H
#include <QtCore/qobject.h>
#include <QtCore/qurl.h>
#include <QtCore/qsize.h>
#include <QtGui/qimage.h>
#include <qzMultimedia/PlaybackOptions.h>
#include <qzMultimedia/SubtitleStyle.h>
#include <qzMultimedia/ChapterInfo.h>
#include <qzMultimedia/TrackInfo.h>

class VideoSink;
class AudioOutput;
class AudioDevice;
class MediaMetaData;
class MediaTimeRange;
class AudioBufferOutput;
class PlaybackOptions;

class MediaPlayerPrivate;
// 媒体播放器公共 API：提供播放控制、轨道管理、元数据访问等接口
class QZ_MULTIMEDIA_EXPORT MediaPlayer : public QObject
{
    Q_OBJECT
    // 媒体源 URL
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    // 媒体总时长（毫秒）
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    // 当前播放位置（毫秒）
    Q_PROPERTY(qint64 position READ position WRITE setPosition NOTIFY positionChanged)

    // 缓冲进度 (0.0 ~ 1.0)
    Q_PROPERTY(float bufferProgress READ bufferProgress NOTIFY bufferProgressChanged)
    // 是否正在缓冲
    Q_PROPERTY(bool buffering READ isBuffering NOTIFY bufferingChanged)

    // 是否可以跳转
    Q_PROPERTY(bool seekable READ isSeekable NOTIFY seekableChanged)

    // 是否正在播放
    Q_PROPERTY(bool playing READ isPlaying NOTIFY playingChanged)

    // 播放速度
    Q_PROPERTY(qreal playbackRate READ playbackRate WRITE setPlaybackRate NOTIFY playbackRateChanged)

    // 播放状态
    Q_PROPERTY(PlaybackState playbackState READ playbackState NOTIFY playbackStateChanged)

    // 媒体加载状态
    Q_PROPERTY(MediaStatus mediaStatus READ mediaStatus NOTIFY mediaStatusChanged)

    // 媒体元数据
    Q_PROPERTY(MediaMetaData metaData READ metaData NOTIFY metaDataChanged)

    // 视频输出目标
    Q_PROPERTY(QObject *videoOutput READ videoOutput WRITE setVideoOutput NOTIFY videoOutputChanged)
    // 音频输出设备
    Q_PROPERTY(AudioOutput *audioOutput READ audioOutput WRITE setAudioOutput NOTIFY
                       audioOutputChanged)
    // 音频缓冲输出（用于可视化）
    Q_PROPERTY(AudioBufferOutput *audioBufferOutput READ audioBufferOutput WRITE
                       setAudioBufferOutput NOTIFY audioBufferOutputChanged)

    // 音频轨道模型
    Q_PROPERTY(TrackInfoModel *audioTracks READ audioTracks CONSTANT)
    // 视频轨道模型
    Q_PROPERTY(TrackInfoModel *videoTracks READ videoTracks CONSTANT)
    // 字幕轨道模型
    Q_PROPERTY(TrackInfoModel *subtitleTracks READ subtitleTracks CONSTANT)

    // 当前激活的轨道索引
    Q_PROPERTY(int activeAudioTrack READ activeAudioTrack NOTIFY activeAudioTrackChanged)
    Q_PROPERTY(int activeVideoTrack READ activeVideoTrack NOTIFY activeVideoTrackChanged)
    Q_PROPERTY(int activeSubtitleTrack READ activeSubtitleTrack NOTIFY activeSubtitleTrackChanged)

    // 章节列表
    Q_PROPERTY(QList<ChapterInfo> chapters READ chapters NOTIFY chaptersChanged)

    // 字幕样式
    Q_PROPERTY(SubtitleStyle subtitleStyle READ subtitleStyle WRITE setSubtitleStyle NOTIFY
                       subtitleStyleChanged)

    // 播放列表
    Q_PROPERTY(QList<QUrl> playlist READ playlist WRITE setPlaylist NOTIFY playlistChanged)
    // 当前播放项索引
    Q_PROPERTY(int playlistIndex READ playlistIndex WRITE setPlaylistIndex NOTIFY playlistIndexChanged)
    // 播放列表播放模式
    Q_PROPERTY(PlaybackMode playbackMode READ playbackMode WRITE setPlaybackMode NOTIFY playbackModeChanged)

    // 片头时长（毫秒），播放时自动跳过
    Q_PROPERTY(qint64 opening READ opening WRITE setOpening NOTIFY openingChanged)
    // 片尾时长（毫秒），播放时自动跳过
    Q_PROPERTY(qint64 ending READ ending WRITE setEnding NOTIFY endingChanged)

    // HDR 策略
    Q_PROPERTY(bool hdrEnabled READ hdrEnabled WRITE setHdrEnabled NOTIFY hdrEnabledChanged)
    // 零拷贝模式
    Q_PROPERTY(bool zeroCopyEnabled READ zeroCopyEnabled WRITE setZeroCopyEnabled NOTIFY zeroCopyEnabledChanged)
    // 低延迟流媒体
    Q_PROPERTY(bool lowLatencyStreamingEnabled READ lowLatencyStreamingEnabled WRITE setLowLatencyStreamingEnabled NOTIFY lowLatencyStreamingEnabledChanged)
    // 当前活跃的视频解码器
    Q_PROPERTY(PlaybackOptions::VideoDecoderPolicy activeDecoder READ activeDecoder NOTIFY activeDecoderChanged)

public:
    // 播放状态枚举
    enum PlaybackState
    {
        StoppedState,
        PlayingState,
        PausedState
    };
    Q_ENUM(PlaybackState)

    // 媒体状态枚举
    enum MediaStatus
    {
        NoMedia,
        LoadingMedia,
        LoadedMedia,
        StalledMedia,
        BufferingMedia,
        BufferedMedia,
        EndOfMedia,
        InvalidMedia
    };
    Q_ENUM(MediaStatus)

    // 错误类型枚举
    enum Error
    {
        NoError,
        ResourceError,
        FormatError,
        NetworkError,
        AccessDeniedError
    };

    // 播放列表播放模式枚举
    enum PlaybackMode
    {
        SequentialPlayback,      // 顺序播放，播完停止
        LoopPlayback,            // 列表循环
        CurrentItemPlayback,     // 单曲循环（播放当前项）
        RandomPlayback           // 随机播放
    };
    Q_ENUM(PlaybackMode)

    // 音高补偿可用性枚举
    enum class PitchCompensationAvailability
    {
        AlwaysOn,
        Available,
        Unavailable,
    };

    explicit MediaPlayer(QObject *parent = nullptr);
    ~MediaPlayer() override;

    // 获取各类型轨道模型
    [[nodiscard]] TrackInfoModel *audioTracks() const;
    [[nodiscard]] TrackInfoModel *videoTracks() const;
    [[nodiscard]] TrackInfoModel *subtitleTracks() const;

    [[nodiscard]] QList<ChapterInfo> chapters() const;

    // 激活指定轨道（同类型其他轨道自动取消激活）
    Q_INVOKABLE void activateTrack(TrackInfo::TrackType type, int index);

    // 获取/设置当前激活轨道索引
    [[nodiscard]] int activeAudioTrack() const;
    [[nodiscard]] int activeVideoTrack() const;
    [[nodiscard]] int activeSubtitleTrack() const;

    Q_INVOKABLE void setActiveAudioTrack(int index);
    Q_INVOKABLE void setActiveVideoTrack(int index);
    Q_INVOKABLE void setActiveSubtitleTrack(int index);

    [[nodiscard]] SubtitleStyle subtitleStyle() const;
    void setSubtitleStyle(const SubtitleStyle &style);

    Q_INVOKABLE void applySubtitleStyle(const QVariantMap &props);

    void setAudioBufferOutput(AudioBufferOutput *output);
    [[nodiscard]] AudioBufferOutput *audioBufferOutput() const;

    // 音频输出设备
    void setAudioOutput(AudioOutput *output);
    [[nodiscard]] AudioOutput *audioOutput() const;

    // 视频输出
    void setVideoOutput(QObject *);
    [[nodiscard]] QObject *videoOutput() const;

    void setVideoSink(VideoSink *sink);
    [[nodiscard]] VideoSink *videoSink() const;

    // 媒体源
    [[nodiscard]] QUrl source() const;
    [[nodiscard]] const QIODevice *sourceDevice() const;

    // 播放状态
    [[nodiscard]] PlaybackState playbackState() const;
    [[nodiscard]] MediaStatus mediaStatus() const;

    // 时长和位置
    [[nodiscard]] qint64 duration() const;
    [[nodiscard]] qint64 position() const;

    // 音视频可用性
    [[nodiscard]] bool hasAudio() const;
    [[nodiscard]] bool hasVideo() const;

    // 缓冲
    [[nodiscard]] float bufferProgress() const;
    [[nodiscard]] bool isBuffering() const;
    [[nodiscard]] MediaTimeRange bufferedTimeRange() const;

    // 跳转和速率
    [[nodiscard]] bool isSeekable() const;
    [[nodiscard]] qreal playbackRate() const;

    [[nodiscard]] bool isPlaying() const;

    // 循环播放
    [[nodiscard]] int loops() const;
    void setLoops(int loops);

    // 错误信息
    [[nodiscard]] Error error() const;
    [[nodiscard]] QString errorString() const;

    [[nodiscard]] bool isAvailable() const;
    // 元数据
    [[nodiscard]] MediaMetaData metaData() const;

    // 音高补偿
    [[nodiscard]] PitchCompensationAvailability pitchCompensationAvailability() const;
    [[nodiscard]] bool pitchCompensation() const;

    // 视频解码器优先级
    [[nodiscard]] Q_INVOKABLE QVector<PlaybackOptions::VideoDecoderPolicy> videoDecoderPriority() const;
    Q_INVOKABLE void prioritizeDecoder(PlaybackOptions::VideoDecoderPolicy policy);
    Q_INVOKABLE void deprioritizeDecoder(PlaybackOptions::VideoDecoderPolicy policy);

    // 当前活跃的视频解码器
    [[nodiscard]] PlaybackOptions::VideoDecoderPolicy activeDecoder() const;

    // 获取媒体封面
    QImage getMediaCover(QSize size = {}, bool decodeFrame =
#ifdef Q_OS_ANDROID
        false
#else
        true
#endif
    );

    void cancelCoverRequest();

    // 播放列表操作
    [[nodiscard]] QList<QUrl> playlist() const;
    Q_INVOKABLE void setPlaylist(const QList<QUrl> &urls);

    [[nodiscard]] int playlistIndex() const;
    void setPlaylistIndex(int index);

    [[nodiscard]] int playlistCount() const;

    [[nodiscard]] PlaybackMode playbackMode() const;
    void setPlaybackMode(PlaybackMode mode);

    // 跳过片头片尾
    [[nodiscard]] qint64 opening() const;
    [[nodiscard]] qint64 ending() const;

    // 播放列表增删
    Q_INVOKABLE void addToPlaylist(const QUrl &url);
    Q_INVOKABLE void addToPlaylist(const QList<QUrl> &urls);
    Q_INVOKABLE void clearPlaylist();

    // 播放列表导航
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void shufflePlaylist();

public Q_SLOTS:
    // 播放控制
    void play();
    void pause();
    void stop();

    void setPosition(qint64 position);

    void setPlaybackRate(qreal rate);

    void setSource(const QUrl &source);
    void setSourceDevice(QIODevice *device, const QUrl &sourceUrl = QUrl());

    void setPitchCompensation(bool) const;

    void setPlaybackOptions(const PlaybackOptions &options);

    void setOpening(qint64 ms);
    void setEnding(qint64 ms);

    // HDR / 零拷贝 / 低延迟流媒体 便利方法
    [[nodiscard]] bool hdrEnabled() const;
    void setHdrEnabled(bool enabled);
    [[nodiscard]] bool zeroCopyEnabled() const;
    void setZeroCopyEnabled(bool enabled);
    [[nodiscard]] bool lowLatencyStreamingEnabled() const;
    void setLowLatencyStreamingEnabled(bool enabled);

Q_SIGNALS:
    void sourceChanged(const QUrl &media);
    void playbackStateChanged(MediaPlayer::PlaybackState newState);
    void mediaStatusChanged(MediaPlayer::MediaStatus status);

    void durationChanged(qint64 duration);
    void positionChanged(qint64 position);

    void bufferProgressChanged(float progress);
    void bufferingChanged(bool buffering);

    void seekableChanged(bool seekable);
    void playingChanged(bool playing);
    void playbackRateChanged(qreal rate);

    void metaDataChanged();
    void videoOutputChanged();
    void audioOutputChanged();
    void audioBufferOutputChanged();

    void tracksChanged();
    void activeAudioTrackChanged();
    void activeVideoTrackChanged();
    void activeSubtitleTrackChanged();

    void subtitleStyleChanged();

    void chaptersChanged();

    void errorChanged();
    void errorOccurred(MediaPlayer::Error error, const QString &errorString);

    void pitchCompensationChanged(bool);

    void playbackOptionsChanged();

    void playlistChanged();
    void playlistIndexChanged(int index);
    void playbackModeChanged(PlaybackMode mode);

    void openingChanged();
    void endingChanged();

    void openingSkipped();
    void endingSkipped();

    void hdrEnabledChanged();
    void zeroCopyEnabledChanged();
    void lowLatencyStreamingEnabledChanged();
    void activeDecoderChanged(PlaybackOptions::VideoDecoderPolicy activeDecoder);

private:
    Q_DISABLE_COPY(MediaPlayer)
    Q_DECLARE_PRIVATE(MediaPlayer)
    friend class PlatformMediaPlayer;
};

#endif
