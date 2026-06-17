// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.


#ifndef QT_PLATFORM_PLATFORMMEDIAPLAYER_P_H
#define QT_PLATFORM_PLATFORMMEDIAPLAYER_P_H
#include <qzMultimedia/MediaPlayer.h>
#include <qzMultimedia/MediaTimeRange.h>
#include <qzMultimedia/AudioDevice.h>
#include <qzMultimedia/MediaMetadata.h>
#include <qzMultimedia/SubtitleStyle.h>
#include <qzMultimedia/ChapterInfo.h>

#include <QtCore/private/qglobal_p.h>
#include <QtCore/qobject.h>
#include <QtGui/qimage.h>

class MediaStreamsControl;
class PlatformAudioOutput;

// 平台播放器抽象接口：定义所有播放操作的纯虚函数
class QZ_MULTIMEDIA_EXPORT PlatformMediaPlayer
{
public:
    virtual ~PlatformMediaPlayer();
    // 获取播放状态
    virtual MediaPlayer::PlaybackState state() const { return m_state; }
    // 获取媒体状态
    virtual MediaPlayer::MediaStatus mediaStatus() const { return m_status; };

    // 获取媒体总时长
    virtual qint64 duration() const = 0;

    // 获取当前播放位置
    virtual qint64 position() const { return m_position; }
    // 设置播放位置
    virtual void setPosition(qint64 position) = 0;

    // 获取缓冲进度
    virtual float bufferProgress() const = 0;

    // 是否有音频轨道
    virtual bool isAudioAvailable() const { return m_audioAvailable; }
    // 是否有视频轨道
    virtual bool isVideoAvailable() const { return m_videoAvailable; }

    // 是否可跳转
    virtual bool isSeekable() const { return m_seekable; }

    // 获取可播放的时间范围
    virtual MediaTimeRange availablePlaybackRanges() const = 0;

    // 获取/设置播放速率
    virtual qreal playbackRate() const = 0;
    virtual void setPlaybackRate(qreal rate) = 0;

    // 获取/设置媒体源
    virtual QUrl media() const = 0;
    virtual const QIODevice *mediaStream() const = 0;
    virtual void setMedia(const QUrl &media, QIODevice *stream) = 0;

    // 播放控制
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;

    // 音频输出设备变化回调（如蓝牙连接/断开）
    // 默认实现：暂停播放
    virtual void onAudioOutputDeviceChanged() { pause(); }

    // 是否支持流播放
    virtual bool streamPlaybackSupported() const { return false; }

    // 设置音频输出
    virtual void setAudioOutput(PlatformAudioOutput *) {}

    // 设置音频缓冲输出
    virtual void setAudioBufferOutput(AudioBufferOutput *) { }

    // 获取媒体元数据
    [[nodiscard]] virtual MediaMetaData metaData() const { return {}; }

    // 获取媒体封面图
    virtual QImage getMediaCover(QSize size, bool decodeFrame = true);

    // 取消封面请求
    virtual void cancelCoverRequest() {}

    // 清空封面缓存（切换媒体源时调用）
    void invalidateCoverCache() { m_cachedCover = {}; }

    // 设置视频输出
    virtual void setVideoSink(VideoSink * ) = 0;

    // 设置字幕样式
    virtual void setSubtitleStyle(const SubtitleStyle &style) { Q_UNUSED(style); }

    // 是否支持播放 qrc 资源
    [[nodiscard]] virtual bool canPlayQrc() const { return false; }

    // 轨道类型枚举
    enum TrackType : uint8_t { VideoStream, AudioStream, SubtitleStream, NTrackTypes };

    // 轨道管理
    virtual int trackCount(TrackType) { return 0; };
    virtual MediaMetaData trackMetaData(TrackType , int ) { return {}; }
    virtual int activeTrack(TrackType) { return -1; }
    virtual void setActiveTrack(TrackType, int ) {}

    virtual QList<ChapterInfo> chapters() const { return {}; }

    void durationChanged(std::chrono::milliseconds ms) { durationChanged(ms.count()); }
    void durationChanged(qint64 duration) { emit player->durationChanged(duration); }
    void positionChanged(std::chrono::milliseconds ms) { positionChanged(ms.count()); }
    void positionChanged(qint64 position) {
        if (m_position == position)
            return;
        m_position = position;
        emit player->positionChanged(position);
    }
    void audioAvailableChanged(bool audioAvailable) {
        if (m_audioAvailable == audioAvailable)
            return;
        m_audioAvailable = audioAvailable;
    }
    void videoAvailableChanged(bool videoAvailable) {
        if (m_videoAvailable == videoAvailable)
            return;
        m_videoAvailable = videoAvailable;
    }
    void seekableChanged(bool seekable) {
        if (m_seekable == seekable)
            return;
        m_seekable = seekable;
        emit player->seekableChanged(seekable);
    }
    void playbackRateChanged(qreal rate) { emit player->playbackRateChanged(rate); }
    void bufferProgressChanged(float progress) { emit player->bufferProgressChanged(progress); }
    void metaDataChanged() { emit player->metaDataChanged(); }
    void tracksChanged();

    void stateChanged(MediaPlayer::PlaybackState newState);
    virtual void mediaStatusChanged(MediaPlayer::MediaStatus status);
    void error(MediaPlayer::Error, const QString &errorString);

    void resetCurrentLoop() { m_currentLoop = 0; }
    bool doLoop() {
        return isSeekable() && (m_loops < 0 || ++m_currentLoop < m_loops);
    }
    int loops() const { return m_loops; }
    virtual void setLoops(int loops)
    {
        if (m_loops == loops)
            return;
        m_loops = loops;
    }

    using PitchCompensationAvailability = MediaPlayer::PitchCompensationAvailability;

    virtual PitchCompensationAvailability pitchCompensationAvailability() const;
    virtual void setPitchCompensation(bool enabled);
    virtual bool pitchCompensation() const;
    void pitchCompensationChanged(bool enabled) const;

    PlaybackOptions playbackOptions() const;

    virtual void setPlaybackOptions(const PlaybackOptions &options);

    void playbackOptionsChanged() { emit player->playbackOptionsChanged(); }

    void activeDecoderChanged(::PlaybackOptions::VideoDecoderPolicy activeDecoder) { emit player->activeDecoderChanged(activeDecoder); }

    // 跳过片头片尾
    [[nodiscard]] qint64 opening() const { return m_opening; }
    [[nodiscard]] qint64 ending() const { return m_ending; }

    virtual void setOpening(qint64 ms)
    {
        if (m_opening == ms)
            return;
        m_opening = ms;
        Q_EMIT player->openingChanged();
    }
    virtual void setEnding(qint64 ms)
    {
        if (m_ending == ms)
            return;
        m_ending = ms;
        Q_EMIT player->endingChanged();
    }

    void openingSkipped() { Q_EMIT player->openingSkipped(); }
    void endingSkipped() { Q_EMIT player->endingSkipped(); }

    bool qmediaplayerDestructorCalled = false;

protected:
    explicit PlatformMediaPlayer(MediaPlayer *parent = nullptr);

    virtual QImage getMediaCover(bool decodeFrame = true) { Q_UNUSED(decodeFrame); return {}; }

private:
    MediaPlayer *player = nullptr;
    MediaPlayer::MediaStatus m_status = MediaPlayer::NoMedia;
    MediaPlayer::PlaybackState m_state = MediaPlayer::StoppedState;
    bool m_seekable = false;
    bool m_videoAvailable = false;
    bool m_audioAvailable = false;
    int m_loops = 1;
    int m_currentLoop = 0;
    qint64 m_position = 0;
    qint64 m_opening = 0;
    qint64 m_ending = 0;
    QImage m_cachedCover;
};

#ifndef QT_NO_DEBUG_STREAM
inline QDebug operator<<(QDebug dbg, PlatformMediaPlayer::TrackType type)
{
    QDebugStateSaver save(dbg);
    dbg.nospace();

    switch (type) {
    case PlatformMediaPlayer::TrackType::AudioStream:
        return dbg << "AudioStream";
    case PlatformMediaPlayer::TrackType::VideoStream:
        return dbg << "VideoStream";
    case PlatformMediaPlayer::TrackType::SubtitleStream:
        return dbg << "SubtitleStream";
    default:
        Q_UNREACHABLE_RETURN(dbg);
    }
}
#endif

#endif
