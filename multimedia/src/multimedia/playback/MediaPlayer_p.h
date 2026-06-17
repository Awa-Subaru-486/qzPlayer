// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_PLAYBACK_MEDIAPLAYER_P_H
#define QT_PLAYBACK_MEDIAPLAYER_P_H
#include "MediaPlayer.h"
#include "MediaMetadata.h"
#include "VideoSink.h"
#include "AudioOutput.h"
#include "AudioBufferOutput.h"
#include "PlaybackOptions.h"
#include <private/PlatformMediaPlayer_p.h>
#include <private/ErrorInfo_p.h>

#include "private/qobject_p.h"
#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtCore/qurl.h>
#include <QtCore/qfile.h>
#include <QtCore/qtimer.h>

#include <memory>

class PlatformMediaPlayer;

// MediaPlayer 私有实现：持有平台播放器指针，实现 PIMPL 模式
class MediaPlayerPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(MediaPlayer)

public:
    // 获取私有实现指针
    static MediaPlayerPrivate *get(MediaPlayer *session)
    {
        return reinterpret_cast<MediaPlayerPrivate *>(QObjectPrivate::get(session));
    }

    MediaPlayerPrivate() = default;
    // 平台播放器实现指针
    PlatformMediaPlayer *control = nullptr;

    QPointer<AudioBufferOutput> audioBufferOutput;
    QPointer<AudioOutput> audioOutput;
    QPointer<VideoSink> videoSink;
    QPointer<QObject> videoOutput;
    QUrl qrcMedia;
    std::unique_ptr<QFile> qrcFile;
    QUrl source;
    QIODevice *stream = nullptr;
    PlaybackOptions playbackOptions;
    PlaybackOptions::VideoDecoderPolicy m_activeDecoder = PlaybackOptions::VideoDecoderPolicy::Software;

    MediaPlayer::PlaybackState state = MediaPlayer::StoppedState;
    ErrorInfo<MediaPlayer::Error> error;

    // 播放列表数据
    QList<QUrl> playlistUrls;
    int playlistCurrentIndex = -1;
    MediaPlayer::PlaybackMode m_playbackMode = MediaPlayer::SequentialPlayback;

    // 随机播放辅助
    QList<int> shuffleOrder;      // 随机排列的索引列表
    int shufflePosition = -1;     // 当前在 shuffleOrder 中的位置

    // 轨道信息模型（服务于当前播放的媒体）
    TrackInfoModel *m_audioTracks = nullptr;
    TrackInfoModel *m_videoTracks = nullptr;
    TrackInfoModel *m_subtitleTracks = nullptr;
    QList<ChapterInfo> m_chapters;

    // 更新轨道缓存
    void updateTrackCache();

    // 设置媒体源
    void setMedia(const QUrl &media, QIODevice *stream = nullptr);

    // 获取轨道元数据
    QList<MediaMetaData> trackMetaData(PlatformMediaPlayer::TrackType s) const;

    // 获取轨道信息列表（包含激活状态）
    QList<TrackInfo> trackInfoList(PlatformMediaPlayer::TrackType s) const;

    // 状态更新
    void setState(MediaPlayer::PlaybackState state);
    void setStatus(MediaPlayer::MediaStatus status, MediaPlayer::MediaStatus oldStatus);
    void setError(MediaPlayer::Error error, const QString &errorString);

    // 播放列表导航辅助
    int nextPlaylistIndex() const;
    int previousPlaylistIndex() const;
    void regenerateShuffleOrder();

    // 播放列表切歌核心逻辑
    void handleEndOfMedia();

    // 设置视频输出
    void setVideoSink(VideoSink *sink)
    {
        Q_Q(MediaPlayer);
        if (sink == videoSink)
            return;
        if (videoSink)
            videoSink->setSource(nullptr);
        videoSink = sink;
        if (sink) {
            sink->setSource(q);
        }
        if (control)
            control->setVideoSink(sink);
        emit q->videoOutputChanged();
    }
};

#endif
