#ifndef PLAYBACKENGINE_FFMPEGMEDIADATAHOLDER_P_H
#define PLAYBACKENGINE_FFMPEGMEDIADATAHOLDER_P_H

#include <expected>
#include <qzMultimedia/MediaMetadata.h>
#include <qzMultimedia/VideoFrame.h>
#include <qzMultimedia/ChapterInfo.h>
#include <qzMultimedia/private/PlatformMediaPlayer_p.h>
#include <qzMultimedia/private/MultimediaUtils_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegTime_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpeg_p.h>

#include <array>
#include <optional>
#include <memory>

#include <QFile>

QT_BEGIN_NAMESPACE

namespace ffmpeg {

struct ICancelToken
{
    virtual ~ICancelToken() = default;
    virtual bool isCancelled() const = 0;
};

using AVFormatContextUPtr = std::unique_ptr<AVFormatContext, AVDeleter<decltype(&avformat_close_input), &avformat_close_input>>;

// 媒体数据持有者，管理流信息、元数据和章节
class MediaDataHolder
{
public:
    struct StreamInfo
    {
        int avStreamIndex = -1;
        bool isDefault = false;
        ::MediaMetaData metaData;
    };

    // 上下文错误，包含错误码和描述
    struct ContextError
    {
        ::MediaPlayer::Error code{};
        QString description;
    };

    // 流映射和索引类型
    using StreamsMap = std::array<QList<StreamInfo>, PlatformMediaPlayer::NTrackTypes>;
    using StreamIndexes = std::array<int, PlatformMediaPlayer::NTrackTypes>;

    MediaDataHolder() = default;
    MediaDataHolder(AVFormatContextUPtr context, const std::shared_ptr<ICancelToken> &cancelToken);
    MediaDataHolder(AVFormatContextUPtr context, std::unique_ptr<QFile> ownedFile,
                    const std::shared_ptr<ICancelToken> &cancelToken);

    // 从媒体类型获取轨道类型
    static PlatformMediaPlayer::TrackType trackTypeFromMediaType(int mediaType);

    // 获取/设置活动轨道
    int activeTrack(PlatformMediaPlayer::TrackType type) const;

    // 获取流信息
    const QList<StreamInfo> &streamInfo(PlatformMediaPlayer::TrackType trackType) const;

    // 时长、元数据、可跳转性
    TrackDuration duration() const { return m_duration; }

    const ::MediaMetaData &metaData() const { return m_metaData; }

    bool isSeekable() const { return m_isSeekable; }

    const QList<ChapterInfo> &chapters() const { return m_chapters; }

    // 获取封面缩略图
    QImage thumbnailImage() const;

    // 视频变换
    VideoTransformation transformation() const;

    // AV 上下文
    AVFormatContext *avContext() const;

    // 当前流索引
    int currentStreamIndex(PlatformMediaPlayer::TrackType trackType) const;

    // 创建媒体数据持有者
    using Maybe = std::expected<std::shared_ptr<MediaDataHolder>, ContextError>;
    static Maybe create(const QUrl &url, QIODevice *stream, const ::PlaybackOptions &options,
                        const std::shared_ptr<ICancelToken> &cancelToken);

    bool setActiveTrack(PlatformMediaPlayer::TrackType type, int streamNumber);

private:
    void updateMetaData();

    std::shared_ptr<ICancelToken> m_cancelToken;

    AVFormatContextUPtr m_context;
    std::unique_ptr<QFile> m_ownedFile;

    bool m_isSeekable = false;

    StreamIndexes m_currentAVStreamIndex = { -1, -1, -1 };
    StreamsMap m_streamMap;
    StreamIndexes m_requestedStreams = { -1, -1, -1 };
    TrackDuration m_duration = TrackDuration(0);
    ::MediaMetaData m_metaData;
    std::optional<QImage> m_cachedThumbnail;

    QList<ChapterInfo> m_chapters;
};

}

QT_END_NAMESPACE

#endif
