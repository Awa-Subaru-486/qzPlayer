#include "playbackengine/FFmpegMediaDataHolder_p.h"

#include "FFmpegMediaMetadata_p.h"
#include "FFmpegMediaFormatInfo_p.h"
#include "FFmpegIoUtils_p.h"
#include "qiodevice.h"
#include "qfile.h"
#include "qdatetime.h"
import qzLog;

#include <qzMultimedia/PlaybackOptions.h>

#include <optional>

extern "C" {
#include "libavutil/display.h"
}

QT_BEGIN_NAMESPACE

static qz::Log::LogCategory qLcMediaDataHolder("qz.multimedia.ffmpeg.mediadataholder");

namespace ffmpeg {

static std::optional<TrackDuration> streamDuration(const AVStream &stream)
{
    if (stream.duration > 0)
        return toTrackDuration(AVStreamDuration(stream.duration), &stream);

    if (stream.duration < 0 && stream.duration != AV_NOPTS_VALUE) {
        qz::Log::cat_warn(qLcMediaDataHolder, "AVStream duration {} is invalid. Taking it from the metadata", stream.duration);
    }

    if (const auto duration = av_dict_get(stream.metadata, "DURATION", nullptr, 0)) {
        const auto time = QTime::fromString(QString::fromUtf8(static_cast<const char*>(duration->value)));
        return TrackDuration(qint64(1000) * time.msecsSinceStartOfDay());
    }

    return {};
}

static QTransform displayMatrixToTransform(const int32_t *displayMatrix)
{

    auto toRotateMirrorValue = [displayMatrix](int index) {

        return displayMatrix[index];
    };

    return QTransform(toRotateMirrorValue(0), toRotateMirrorValue(1),
                      toRotateMirrorValue(3), toRotateMirrorValue(4),
                      0, 0);
}

static VideoTransformation streamTransformation(const AVStream *stream)
{
    Q_ASSERT(stream);

    using SideDataSize = decltype(AVPacketSideData::size);
    constexpr SideDataSize displayMatrixSize = sizeof(int32_t) * 9;
    const AVPacketSideData *sideData = streamSideData(stream, AV_PKT_DATA_DISPLAYMATRIX);
    if (!sideData || sideData->size < displayMatrixSize)
        return {};

    const auto displayMatrix = reinterpret_cast<const int32_t *>(sideData->data);
    const QTransform transform = displayMatrixToTransform(displayMatrix);
    const VideoTransformationOpt result = qVideoTransformationFromMatrix(transform);
    if (!result) {
        qz::Log::cat_warn(qLcMediaDataHolder, "Video stream contains malformed display matrix");
        return {};
    }
    return *result;
}

static bool colorTransferSupportsHdr(const AVStream *stream)
{
    if (!stream)
        return false;

    const AVCodecParameters *codecPar = stream->codecpar;
    if (!codecPar)
        return false;

    const VideoFrameFormat::ColorTransfer colorTransfer = fromAvColorTransfer(codecPar->color_trc);

    return colorTransfer == VideoFrameFormat::ColorTransfer_ST2084
            || colorTransfer == VideoFrameFormat::ColorTransfer_STD_B67;
}

static double calculateVideoQualityScore(const MediaDataHolder::StreamInfo &streamInfo)
{
    double score = 0.0;

    const auto &metaData = streamInfo.metaData;

    const int bitRate = metaData.value(::MediaMetaData::VideoBitRate).toInt();
    if (bitRate > 0) {
        score += std::log10(static_cast<double>(bitRate)) * 10.0;
    }

    const QSize resolution = metaData.value(::MediaMetaData::Resolution).toSize();
    if (resolution.isValid()) {
        const int pixelCount = resolution.width() * resolution.height();
        score += std::log10(static_cast<double>(pixelCount)) * 5.0;
    }

    const double frameRate = metaData.value(::MediaMetaData::VideoFrameRate).toDouble();
    if (frameRate > 0.0) {
        score += std::log10(frameRate) * 3.0;
    }

    const bool hasHdr = metaData.value(::MediaMetaData::HasHdrContent).toBool();
    if (hasHdr) {
        score += 50.0;
    }

    if (streamInfo.isDefault) {
        score += 5.0;
    }

    return score;
}

static int selectBestVideoStream(const QList<MediaDataHolder::StreamInfo> &streamMap)
{
    if (streamMap.isEmpty())
        return -1;

    if (streamMap.size() == 1)
        return 0;

    int bestIndex = 0;
    double bestScore = calculateVideoQualityScore(streamMap[0]);

    qz::Log::cat_debug(qLcMediaDataHolder, "Video stream selection: analyzing {} streams", streamMap.size());

    for (int i = 0; i < streamMap.size(); ++i) {
        const auto &stream = streamMap[i];
        const double score = calculateVideoQualityScore(stream);

        const int bitRate = stream.metaData.value(::MediaMetaData::VideoBitRate).toInt();
        const QSize resolution = stream.metaData.value(::MediaMetaData::Resolution).toSize();
        const double frameRate = stream.metaData.value(::MediaMetaData::VideoFrameRate).toDouble();
        const bool hasHdr = stream.metaData.value(::MediaMetaData::HasHdrContent).toBool();

        qz::Log::cat_debug(qLcMediaDataHolder, "  Stream {} : bitrate={} resolution={} framerate={} hdr={} default={} score={}", i, bitRate, resolution, frameRate, hasHdr, stream.isDefault, score);

        if (score > bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }

    qz::Log::cat_debug(qLcMediaDataHolder, "Selected best video stream: {} with score {}", bestIndex, bestScore);

    return bestIndex;
}

VideoTransformation MediaDataHolder::transformation() const
{

    const int streamIndex = m_currentAVStreamIndex[PlatformMediaPlayer::VideoStream];
    if (streamIndex < 0)
        return {};

    return streamTransformation(m_context->streams[streamIndex]);
}

AVFormatContext *MediaDataHolder::avContext() const
{
    return m_context.get();
}

int MediaDataHolder::currentStreamIndex(PlatformMediaPlayer::TrackType trackType) const
{
    return m_currentAVStreamIndex[trackType];
}

static void insertMediaData(::MediaMetaData &metaData, PlatformMediaPlayer::TrackType trackType,
                            const AVStream *stream)
{
    Q_ASSERT(stream);
    const auto *codecPar = stream->codecpar;

    switch (trackType) {
    case PlatformMediaPlayer::VideoStream:
        metaData.insert(::MediaMetaData::VideoBitRate, static_cast<int>(codecPar->bit_rate));
        metaData.insert(::MediaMetaData::VideoCodec,
                        QVariant::fromValue(MediaFormatInfo::videoCodecForAVCodecId(
                                codecPar->codec_id)));
        metaData.insert(::MediaMetaData::Resolution, QSize(codecPar->width, codecPar->height));
        metaData.insert(::MediaMetaData::VideoFrameRate,
                        static_cast<qreal>(stream->avg_frame_rate.num) / static_cast<qreal>(stream->avg_frame_rate.den));
        metaData.insert(::MediaMetaData::Orientation,
                        QVariant::fromValue(streamTransformation(stream).rotation));
        metaData.insert(::MediaMetaData::HasHdrContent, colorTransferSupportsHdr(stream));
        break;
    case PlatformMediaPlayer::AudioStream:
        metaData.insert(::MediaMetaData::AudioBitRate, static_cast<int>(codecPar->bit_rate));
        metaData.insert(::MediaMetaData::AudioCodec,
                        QVariant::fromValue(MediaFormatInfo::audioCodecForAVCodecId(
                                codecPar->codec_id)));
        break;
    default:
        break;
    }
};

PlatformMediaPlayer::TrackType MediaDataHolder::trackTypeFromMediaType(int mediaType)
{
    switch (mediaType) {
    case AVMEDIA_TYPE_AUDIO:
        return PlatformMediaPlayer::AudioStream;
    case AVMEDIA_TYPE_VIDEO:
        return PlatformMediaPlayer::VideoStream;
    case AVMEDIA_TYPE_SUBTITLE:
        return PlatformMediaPlayer::SubtitleStream;
    default:
        return PlatformMediaPlayer::NTrackTypes;
    }
}

namespace {

struct LoadMediaResult
{
    AVFormatContextUPtr context;
    std::unique_ptr<QFile> ownedFile;
};

std::expected<LoadMediaResult, MediaDataHolder::ContextError>
loadMedia(const QUrl &mediaUrl, QIODevice *stream, const ::PlaybackOptions &playbackOptions,
          const std::shared_ptr<ICancelToken> &cancelToken)
{
    using std::chrono::duration_cast;
    using std::chrono::microseconds;
    using std::chrono::milliseconds;

    const QByteArray url = mediaUrl.toString(QUrl::PreferLocalFile).toUtf8();

    AVFormatContextUPtr context{ avformat_alloc_context() };

    std::unique_ptr<QFile> ownedFile;

    if (stream) {
        if (!stream->isOpen()) {
            if (!stream->open(QIODevice::ReadOnly)) {
                return std::unexpected{
                    MediaDataHolder::ContextError{
                            ::MediaPlayer::ResourceError,
                            QLatin1String("Could not open source device."),
                    },
                };
            }
        }
    } else if (mediaUrl.isLocalFile()) {
#ifdef Q_OS_WIN
        ownedFile = std::make_unique<QFile>(mediaUrl.toLocalFile());
        if (!ownedFile->open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {
            ownedFile.reset();
            return std::unexpected{
                MediaDataHolder::ContextError{
                        ::MediaPlayer::ResourceError,
                        QLatin1String("Could not open source file."),
                },
            };
        }
        stream = ownedFile.get();
#endif
    }

    if (stream) {
        auto seek = &seekQIODevice;

        if (!stream->isSequential()) {
            stream->seek(0);
        } else {
            context->ctx_flags |= AVFMTCTX_UNSEEKABLE;
            seek = nullptr;
        }

        constexpr int bufferSize = 32768;
        auto *buffer = static_cast<unsigned char*>(av_malloc(bufferSize));
        context->pb = avio_alloc_context(buffer, bufferSize, false, stream, &readQIODevice, nullptr,
                                         seek);
    }

    AVDictionaryHolder dict;
    {
        const milliseconds timeout = playbackOptions.networkTimeout();
        av_dict_set_int(dict, "timeout", duration_cast<microseconds>(timeout).count(), 0);
        qz::Log::cat_debug(qLcMediaDataHolder, "Using custom network timeout:{}", timeout);
    }

    {
        if (const qsizetype probeSize = playbackOptions.probeSize(); probeSize != -1) {
            constexpr qsizetype minProbeSizeFFmpeg = 32;
            if (probeSize >= minProbeSizeFFmpeg) {
                av_dict_set_int(dict, "probesize", probeSize, 0);
            }
            else
                qz::Log::cat_warn(qLcMediaDataHolder, "Invalid probe size, using default");
        }
    }

    // 协议白名单，用于指定允许的协议
    const QByteArray protocolWhitelist = qgetenv("QT_FFMPEG_PROTOCOL_WHITELIST");
    if (!protocolWhitelist.isNull())
        av_dict_set(dict, "protocol_whitelist", protocolWhitelist.data(), 0);

    if (playbackOptions.playbackIntent() == ::PlaybackOptions::PlaybackIntent::LowLatencyStreaming) {
        av_dict_set(dict, "fflags", "nobuffer", 0);
        av_dict_set_int(dict, "flush_packets", 1, 0);
        qz::Log::cat_debug(qLcMediaDataHolder, "Enabled low latency streaming");
    }

    context->interrupt_callback.opaque = cancelToken.get();
    context->interrupt_callback.callback = [](void *opaque) {
        const auto *cancelToken = static_cast<const ICancelToken *>(opaque);
        if (cancelToken && cancelToken->isCancelled())
            return 1;
        return 0;
    };

    int ret = 0;
    {
        AVFormatContext *contextRaw = context.release();

        // When using custom I/O (context->pb is set), FFmpeg doesn't need to open
        // the URL via its protocol layer. However, if the URL scheme is not in
        // FFmpeg's protocol whitelist (e.g., "content://" on Android), avformat_open_input
        // may reject it. Use a dummy URL with a known scheme to avoid this issue.
        QByteArray avioUrl = url;
        if (contextRaw->pb && mediaUrl.scheme() != QLatin1String("file")
            && mediaUrl.scheme() != QLatin1String("http")
            && mediaUrl.scheme() != QLatin1String("https")
            && mediaUrl.scheme() != QLatin1String("ftp")
            && mediaUrl.scheme() != QLatin1String("rtsp")
            && mediaUrl.scheme() != QLatin1String("rtp")) {
            // Use a dummy URL with the original file extension for format detection
            const QString path = mediaUrl.path();
            const QString ext = path.lastIndexOf(QLatin1Char('.')) >= 0
                                    ? path.mid(path.lastIndexOf(QLatin1Char('.')))
                                    : QString();
            avioUrl = QByteArray("file:dummy") + ext.toUtf8();
        }

        ret = avformat_open_input(&contextRaw, avioUrl.constData(), nullptr, dict);
        context.reset(contextRaw);
    }

    if (ret < 0) {
        auto code = ::MediaPlayer::ResourceError;
        if (ret == AVERROR(EACCES))
            code = ::MediaPlayer::AccessDeniedError;
        else if (ret == AVERROR(EINVAL) || ret == AVERROR_INVALIDDATA)
            code = ::MediaPlayer::FormatError;

        qz::Log::cat_warn(qLcMediaDataHolder, "Could not open media. FFmpeg error description:{}", static_cast<int>(static_cast<AVError>(ret)));

        return std::unexpected{
            MediaDataHolder::ContextError{ code, ::MediaPlayer::tr("Could not open file") },
        };
    }

    // matroska/webm 容器可能包含 TrueHD、PGS 等需要更长探测时间的流，
    // 在 avformat_open_input 成功后根据实际格式名直接增大分析参数
    if (context->iformat && strstr(context->iformat->name, "matroska")) {
        if (context->probesize < 10 * 1024 * 1024)
            context->probesize = 10 * 1024 * 1024;
        if (context->max_analyze_duration < 10 * AV_TIME_BASE)
            context->max_analyze_duration = 10 * AV_TIME_BASE;
    }

    ret = avformat_find_stream_info(context.get(), nullptr);
    if (ret < 0) {
        return std::unexpected{
            MediaDataHolder::ContextError{
                    ::MediaPlayer::FormatError,
                    ::MediaPlayer::tr("Could not find stream information for media file") },
        };
    }

    if (qLcMediaDataHolder.is_enabled())
        av_dump_format(context.get(), 0, url.constData(), 0);

    return LoadMediaResult{ std::move(context), std::move(ownedFile) };
}

}

MediaDataHolder::Maybe MediaDataHolder::create(const QUrl &url, QIODevice *stream,
                                               const ::PlaybackOptions &options,
                                               const std::shared_ptr<ICancelToken> &cancelToken)
{
    std::expected result = loadMedia(url, stream, options, cancelToken);
    if (result) {
        auto &value = result.value();
        return std::make_shared<MediaDataHolder>(
                MediaDataHolder{ std::move(value.context), std::move(value.ownedFile), cancelToken });
    }
    return std::unexpected{ result.error() };
}

MediaDataHolder::MediaDataHolder(AVFormatContextUPtr context,
                                 const std::shared_ptr<ICancelToken> &cancelToken)
    : MediaDataHolder(std::move(context), nullptr, cancelToken)
{
}

MediaDataHolder::MediaDataHolder(AVFormatContextUPtr context,
                                 std::unique_ptr<QFile> ownedFile,
                                 const std::shared_ptr<ICancelToken> &cancelToken)
    : m_cancelToken{ cancelToken }
    , m_ownedFile{ std::move(ownedFile) }
{
    Q_ASSERT(context);

    m_context = std::move(context);
    m_isSeekable = !(m_context->ctx_flags & AVFMTCTX_UNSEEKABLE);

    qz::Log::cat_debug(qLcMediaDataHolder, "MediaDataHolder: parsing {} streams from context", m_context->nb_streams);

    for (unsigned int i = 0; i < m_context->nb_streams; ++i) {

        const auto *stream = m_context->streams[i];
        const auto trackType = trackTypeFromMediaType(stream->codecpar->codec_type);

        qz::Log::cat_debug(qLcMediaDataHolder, "  stream[{}]: codec_type={} trackType={} attached_pic={} time_base={}/{}",
                           i, static_cast<int>(stream->codecpar->codec_type), static_cast<int>(trackType),
                           !!(stream->disposition & AV_DISPOSITION_ATTACHED_PIC),
                           stream->time_base.num, stream->time_base.den);

        if (trackType == PlatformMediaPlayer::NTrackTypes)
            continue;

        if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC)
            continue;

        if (stream->time_base.num <= 0 || stream->time_base.den <= 0) {

            qz::Log::cat_warn(qLcMediaDataHolder, "A stream for the track type {} has an invalid timebase: ({}/{})", static_cast<int>(trackType), stream->time_base.num, stream->time_base.den);
            continue;
        }

        auto metaData = MetaData::fromAVMetaData(stream->metadata);
        const bool isDefault = stream->disposition & AV_DISPOSITION_DEFAULT;

        insertMediaData(metaData, trackType, stream);

        if (isDefault && m_requestedStreams[trackType] < 0)
            m_requestedStreams[trackType] = m_streamMap[trackType].size();

        if (auto duration = streamDuration(*stream)) {
            m_duration = qMax(m_duration, *duration);
            metaData.insert(::MediaMetaData::Duration, toUserDuration(*duration).get());
        }

        m_streamMap[trackType].append({ (int)i, isDefault, metaData });
        qz::Log::cat_debug(qLcMediaDataHolder, "  -> added to streamMap[{}], now count={}, isDefault={}, avStreamIndex={}",
                           static_cast<int>(trackType), m_streamMap[trackType].size(), isDefault, i);
    }

    qz::Log::cat_debug(qLcMediaDataHolder, "MediaDataHolder: streamMap populated: video={} audio={} subtitle={}",
                       m_streamMap[PlatformMediaPlayer::VideoStream].size(),
                       m_streamMap[PlatformMediaPlayer::AudioStream].size(),
                       m_streamMap[PlatformMediaPlayer::SubtitleStream].size());

    if (m_duration == TrackDuration(0) && m_context->duration > 0ll) {
        m_duration = toTrackDuration(AVContextDuration(m_context->duration));
    }

    for (unsigned int i = 0; i < m_context->nb_chapters; ++i) {
        const AVChapter *chapter = m_context->chapters[i];
        if (!chapter)
            continue;

        ChapterInfo info;
        info.setStartTime(av_rescale_q(chapter->start, chapter->time_base, {1, 1000}));
        info.setEndTime(av_rescale_q(chapter->end, chapter->time_base, {1, 1000}));

        if (const auto entry = av_dict_get(chapter->metadata, "title", nullptr, 0))
            info.setTitle(QString::fromUtf8(static_cast<const char*>(entry->value)));

        m_chapters.append(std::move(info));
    }

    for (const auto trackType :
         { PlatformMediaPlayer::VideoStream, PlatformMediaPlayer::AudioStream,
           PlatformMediaPlayer::SubtitleStream }) {
        auto &requestedStream = m_requestedStreams[trackType];
        auto &streamMap = m_streamMap[trackType];

        if (requestedStream < 0 && !streamMap.empty()) {
            if (trackType == PlatformMediaPlayer::VideoStream && streamMap.size() > 1) {
                requestedStream = selectBestVideoStream(streamMap);
            } else {
                requestedStream = 0;
            }
        }

        if (requestedStream >= 0)
            m_currentAVStreamIndex[trackType] = streamMap[requestedStream].avStreamIndex;

        qz::Log::cat_debug(qLcMediaDataHolder, "  trackType {}: requestedStream={} currentAVStreamIndex={}",
                           static_cast<int>(trackType), requestedStream, m_currentAVStreamIndex[trackType]);
    }

    updateMetaData();
}

namespace {

QImage getAttachedPicture(const AVFormatContext *context)
{
    if (!context)
        return {};

    for (unsigned int i = 0; i < context->nb_streams; ++i) {
        const AVStream* stream = context->streams[i];
        if (!stream || !(stream->disposition & AV_DISPOSITION_ATTACHED_PIC))
            continue;

        const AVPacket *compressedImage = &stream->attached_pic;
        if (!compressedImage || !compressedImage->data || compressedImage->size <= 0)
            continue;

        if (QImage image = QImage::fromData({ compressedImage->data, compressedImage->size }); !image.isNull())
            return image;
    }

    return {};
}

}

void MediaDataHolder::updateMetaData()
{
    m_metaData = {};

    if (!m_context)
        return;

    m_metaData = MetaData::fromAVMetaData(m_context->metadata);
    m_metaData.insert(::MediaMetaData::FileFormat,
                      QVariant::fromValue(MediaFormatInfo::fileFormatForAVInputFormat(
                              *m_context->iformat)));
    m_metaData.insert(::MediaMetaData::Duration, toUserDuration(m_duration).get());

    if (!m_cachedThumbnail.has_value())
        m_cachedThumbnail = getAttachedPicture(m_context.get());

    if (!m_cachedThumbnail->isNull())
        m_metaData.insert(::MediaMetaData::ThumbnailImage, m_cachedThumbnail.value());

    for (const auto trackType :
         { PlatformMediaPlayer::AudioStream, PlatformMediaPlayer::VideoStream }) {
        const auto streamIndex = m_currentAVStreamIndex[trackType];
        if (streamIndex >= 0)
            insertMediaData(m_metaData, trackType, m_context->streams[streamIndex]);
    }
}

bool MediaDataHolder::setActiveTrack(PlatformMediaPlayer::TrackType type, int streamNumber)
{
    if (!m_context)
        return false;

    if (streamNumber < 0 || streamNumber >= m_streamMap[type].size())
        streamNumber = -1;
    if (m_requestedStreams[type] == streamNumber)
        return false;
    m_requestedStreams[type] = streamNumber;
    const int avStreamIndex = m_streamMap[type].value(streamNumber).avStreamIndex;

    const int oldIndex = m_currentAVStreamIndex[type];
    qz::Log::cat_debug(qLcMediaDataHolder, ">>>>> change track {} from {} to {}", static_cast<int>(type), oldIndex, avStreamIndex);

    m_currentAVStreamIndex[type] = avStreamIndex;

    updateMetaData();

    return true;
}

int MediaDataHolder::activeTrack(PlatformMediaPlayer::TrackType type) const
{
    return type < PlatformMediaPlayer::NTrackTypes ? m_requestedStreams[type] : -1;
}

QImage MediaDataHolder::thumbnailImage() const
{
    return m_cachedThumbnail.value_or(QImage{});
}

const QList<MediaDataHolder::StreamInfo> &MediaDataHolder::streamInfo(
        PlatformMediaPlayer::TrackType trackType) const
{
    Q_ASSERT(trackType < PlatformMediaPlayer::NTrackTypes);

    return m_streamMap[trackType];
}

}

QT_END_NAMESPACE
