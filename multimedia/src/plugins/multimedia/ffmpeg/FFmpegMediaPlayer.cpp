#include "FFmpegMediaPlayer_p.h"
#include "private/PlatformAudioOutput_p.h"
#include "AudioOutput.h"
#include "FFmpegPlaybackEngine_p.h"
#include <qzMultimedia/VideoSink.h>
#include <qzMultimedia/AudioBufferOutput.h>
#include <qiodevice.h>
#include <qzMultimedia/PlaybackOptions.h>
#include <qtimer.h>
#include <QtConcurrent/QtConcurrent>

#ifdef Q_OS_ANDROID
#include "private/AndroidAudioUtil_p.h"
#endif

extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

import qzLog;

QT_BEGIN_NAMESPACE

namespace ffmpeg {

namespace {

bool isYuvVibrant(const AVFrame *frame)
{
    if (!frame || frame->format != AV_PIX_FMT_YUV420P)
        return false;

    const int width = frame->width;
    const int height = frame->height;
    const long long totalPx = static_cast<long long>(width) * height;

    constexpr long long HD_THRESHOLD = 921600;
    const bool useHollowScan = totalPx > HD_THRESHOLD;

    const uint8_t *yPlane = frame->data[0];
    const uint8_t *uPlane = frame->data[1];
    const uint8_t *vPlane = frame->data[2];

    const int yStride = frame->linesize[0];
    const int uStride = frame->linesize[1];
    const int vStride = frame->linesize[2];

    long vividPixels = 0;
    long sampledPixels = 0;

    constexpr int yThresh = 40;
    constexpr int yHighThresh = 220;
    constexpr int chromaThresh = 30;
    constexpr int step = 4;

    int excludeStartX = 0, excludeEndX = 0;
    int excludeStartY = 0, excludeEndY = 0;

    if (useHollowScan) {
        constexpr float holeRatio = 0.5f;
        int holeW = static_cast<int>(width * holeRatio);
        int holeH = static_cast<int>(height * holeRatio);
        if (holeW < 2) holeW = 2;
        if (holeH < 2) holeH = 2;
        excludeStartX = (width - holeW) / 2;
        excludeEndX = excludeStartX + holeW;
        excludeStartY = (height - holeH) / 2;
        excludeEndY = excludeStartY + holeH;
    }

    for (int y = 0; y < height; y += step) {
        const uint8_t *yRowPtr = yPlane + y * yStride;
        const int uvY = y >> 1;
        const uint8_t *uRowPtr = uPlane + uvY * uStride;
        const uint8_t *vRowPtr = vPlane + uvY * vStride;

        const bool isYInHole = y >= excludeStartY && y < excludeEndY;

        for (int x = 0; x < width; x += step) {
            if (useHollowScan && isYInHole && x >= excludeStartX && x < excludeEndX)
                continue;

            if (yRowPtr[x] < yThresh || yRowPtr[x] > yHighThresh)
                continue;

            const int uvX = x >> 1;
            const int diffU = std::abs(static_cast<int>(uRowPtr[uvX]) - 128);
            const int diffV = std::abs(static_cast<int>(vRowPtr[uvX]) - 128);

            if (diffU + diffV > chromaThresh)
                vividPixels++;
            sampledPixels++;
        }
    }

    if (sampledPixels == 0)
        return false;

    const float ratio = static_cast<float>(vividPixels) / sampledPixels;
    return ratio > 0.15f;
}

QImage convertAVFrameToQImage(const AVFrame *frame)
{
    if (!frame || !frame->data[0])
        return {};

    const int width = frame->width;
    const int height = frame->height;
    const auto srcFmt = static_cast<AVPixelFormat>(frame->format);

    const SwsContextUPtr swsCtx = createSwsContext(QSize(width, height), srcFmt,
                                                   QSize(width, height), AV_PIX_FMT_RGBA,
                                                   SWS_BILINEAR);
    if (!swsCtx)
        return {};

    AVFrameUPtr dstFrame = makeAVFrame();
    dstFrame->width = width;
    dstFrame->height = height;
    dstFrame->format = AV_PIX_FMT_RGBA;

    if (av_frame_get_buffer(dstFrame.get(), 32) < 0)
        return {};

    sws_scale(swsCtx.get(), frame->data, frame->linesize, 0, height,
              dstFrame->data, dstFrame->linesize);

    const QImage tempImage(reinterpret_cast<const uchar *>(dstFrame->data[0]),
                           width, height, dstFrame->linesize[0],
                           QImage::Format_RGBA8888);

    return tempImage.copy();
}

QImage decodeVideoFrameCover(const QUrl &url, const std::shared_ptr<ICancelToken> &cancelToken,
                             const AVFormatContext *existingContext = nullptr)
{
    const QByteArray urlBytes = url.toString(QUrl::PreferLocalFile).toUtf8();

    AVFormatContext *formatCtxRaw = avformat_alloc_context();
    if (!formatCtxRaw)
        return {};

    // 复用已有 Context 的格式信息，跳过格式探测
    if (existingContext && existingContext->iformat)
        formatCtxRaw->iformat = existingContext->iformat;

    if (cancelToken) {
        formatCtxRaw->interrupt_callback.opaque = cancelToken.get();
        formatCtxRaw->interrupt_callback.callback = [](void *opaque) {
            const auto *token = static_cast<const ICancelToken *>(opaque);
            return (token && token->isCancelled()) ? 1 : 0;
        };
    }

    // 缩短初始化耗时：限制探测大小和分析时长
    AVDictionary *options = nullptr;
    av_dict_set(&options, "probesize", "32768", 0);
    av_dict_set(&options, "analyzeduration", "100000", 0);

    if (avformat_open_input(&formatCtxRaw, urlBytes.constData(), nullptr, &options) < 0) {
        av_dict_free(&options);
        return {};
    }
    av_dict_free(&options);
    const AVFormatContextUPtr formatCtx(formatCtxRaw);

    if (cancelToken && cancelToken->isCancelled())
        return {};

    // 封面提取只需要视频流，跳过字幕/音频流以避免 PGS 等图形字幕报
    // "Could not find codec parameters" 警告，同时加速探测
    for (unsigned int i = 0; i < formatCtx->nb_streams; ++i) {
        if (formatCtx->streams[i]->codecpar->codec_type != AVMEDIA_TYPE_VIDEO)
            formatCtx->streams[i]->discard = AVDISCARD_ALL;
    }

    if (avformat_find_stream_info(formatCtx.get(), nullptr) < 0) {
        return {};
    }

    if (cancelToken && cancelToken->isCancelled())
        return {};

    int videoStreamIndex = -1;

    // 如果已有 Context 提供了视频流信息，优先复用其视频流索引
    if (existingContext) {
        for (unsigned int i = 0; i < existingContext->nb_streams; ++i) {
            const AVStream *stream = existingContext->streams[i];
            if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO
                && !(stream->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
                // 验证索引在新 Context 中仍然有效
                if (i < formatCtx->nb_streams) {
                    const AVStream *newStream = formatCtx->streams[i];
                    if (newStream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO
                        && !(newStream->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
                        videoStreamIndex = static_cast<int>(i);
                    }
                }
                break;
            }
        }
    }

    // 回退到遍历搜索
    if (videoStreamIndex < 0) {
        for (unsigned int i = 0; i < formatCtx->nb_streams; ++i) {
            const AVStream *stream = formatCtx->streams[i];
            if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO
                && !(stream->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
                videoStreamIndex = static_cast<int>(i);
                break;
            }
        }
    }

    if (videoStreamIndex < 0)
        return {};

    const AVStream *videoStream = formatCtx->streams[videoStreamIndex];
    const AVCodec *codec = avcodec_find_decoder(videoStream->codecpar->codec_id);
    if (!codec)
        return {};

    AVCodecContextUPtr codecCtx(avcodec_alloc_context3(codec));
    if (!codecCtx)
        return {};

    if (avcodec_parameters_to_context(codecCtx.get(), videoStream->codecpar) < 0)
        return {};

    if (avcodec_open2(codecCtx.get(), codec, nullptr) < 0)
        return {};

    auto tryDecodeFrame = [&](AVDiscard discardPolicy) -> QImage {
        codecCtx->skip_frame = discardPolicy;
        avcodec_flush_buffers(codecCtx.get());

        AVFrameUPtr frame = makeAVFrame();
        AVPacketUPtr packet(av_packet_alloc());
        if (!packet)
            return {};

        QImage capturedImage;
        int attempts = 0;
        constexpr int maxAttempts = 10;

        while (av_read_frame(formatCtx.get(), packet.get()) >= 0) {
            if (cancelToken && cancelToken->isCancelled())
                return {};

            if (packet->stream_index != videoStreamIndex) {
                av_packet_unref(packet.get());
                continue;
            }

            int ret = avcodec_send_packet(codecCtx.get(), packet.get());
            av_packet_unref(packet.get());
            if (ret < 0)
                continue;

            while (avcodec_receive_frame(codecCtx.get(), frame.get()) >= 0) {
                if (cancelToken && cancelToken->isCancelled())
                    return {};

                attempts++;

                const bool vibrant = isYuvVibrant(frame.get());
                QImage image = convertAVFrameToQImage(frame.get());
                av_frame_unref(frame.get());

                if (!image.isNull()) {
                    capturedImage = image;
                    if (vibrant)
                        return capturedImage;
                }

                if (attempts >= maxAttempts)
                    return capturedImage;
            }
        }

        return capturedImage;
    };

    QImage result = tryDecodeFrame(AVDISCARD_NONINTRA);

    if (result.isNull()) {
        if (cancelToken && cancelToken->isCancelled())
            return {};
        av_seek_frame(formatCtx.get(), -1, 0, AVSEEK_FLAG_BACKWARD);
        avformat_flush(formatCtx.get());
        result = tryDecodeFrame(AVDISCARD_DEFAULT);
    }

    return result;
}

}

class CancelToken : public ICancelToken
{
public:

    bool isCancelled() const override { return m_cancelled.load(std::memory_order_acquire); }

    void cancel() { m_cancelled.store(true, std::memory_order_release); }

private:
    std::atomic_bool m_cancelled = false;
};

MediaPlayer::MediaPlayer(::MediaPlayer *player)
    : PlatformMediaPlayer(player)
{
    m_positionUpdateTimer.setInterval(50);
    m_positionUpdateTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_positionUpdateTimer, &QTimer::timeout, this, &MediaPlayer::updatePosition);
}

MediaPlayer::~MediaPlayer()
{
    if (m_cancelToken)
        m_cancelToken->cancel();

    if (m_coverCancelToken)
        m_coverCancelToken->cancel();

    m_loadMedia.waitForFinished();
};

qint64 MediaPlayer::duration() const
{
    return m_playbackEngine ? toUserDuration(m_playbackEngine->duration()).get() : 0;
}

void MediaPlayer::setPosition(qint64 position)
{
    if (mediaStatus() == ::MediaPlayer::LoadingMedia)
        return;

    if (m_playbackEngine) {
        // seek 位置钳制：不允许 seek 到片头或片尾区域内
        // 片头区域 [0, opening) → 钳制到 opening
        if (opening() > 0 && position < opening()) {
            position = opening();
        }
        // 片尾区域 [duration - ending, duration) → 钳制到 duration - ending
        if (ending() > 0 && position > duration() - ending()) {
            position = qMax(duration() - ending(), opening());
        }

        const auto statusBefore = mediaStatus();
        m_playbackEngine->seek(toTrackPosition(UserTrackPosition(position)));
        updatePosition();

        // 若 updatePosition 触发了片尾跳过（状态已变），不再覆盖媒体状态
        if (mediaStatus() == statusBefore)
            mediaStatusChanged(::MediaPlayer::BufferingMedia);
    } else {
        mediaStatusChanged(::MediaPlayer::BufferingMedia);
    }
}

void MediaPlayer::updatePosition()
{
    const qint64 pos = m_playbackEngine ? toUserPosition(m_playbackEngine->currentPosition()).get() : 0;
    positionChanged(pos);

    // 内核级片尾跳过：当位置到达 endingPos 时，结束当前视频播放
    const qint64 endingPos = duration() - ending();
    if (ending() > 0 && pos >= endingPos && state() == ::MediaPlayer::PlayingState) {
        if (!m_suppressEnding) {
            m_positionUpdateTimer.stop();
            positionChanged(endingPos);
            stateChanged(::MediaPlayer::StoppedState);
            mediaStatusChanged(::MediaPlayer::EndOfMedia);
            endingSkipped();
            if (m_playbackEngine)
                m_playbackEngine->stop();
            return;
        }
    }

    // 当位置离开片尾区域时，解除片尾抑制
    if (m_suppressEnding && ending() > 0 && pos < duration() - ending()) {
        m_suppressEnding = false;
    }

    // 当位置离开片头区域时，解除片头抑制
    if (m_suppressOpening && opening() > 0 && pos >= opening()) {
        m_suppressOpening = false;
    }
}

void MediaPlayer::endOfStream()
{
    // 若已经处于 StoppedState（如片尾跳过已手动设置），跳过重复处理
    if (state() == ::MediaPlayer::StoppedState)
        return;

    m_positionUpdateTimer.stop();
    QPointer currentPlaybackEngine(m_playbackEngine.get());
    positionChanged(duration());

    if (currentPlaybackEngine)
        stateChanged(::MediaPlayer::StoppedState);
    if (currentPlaybackEngine)
        mediaStatusChanged(::MediaPlayer::EndOfMedia);
}

void MediaPlayer::onLoopChanged()
{
    positionChanged(duration());
    positionChanged(0);
    m_positionUpdateTimer.stop();
    m_positionUpdateTimer.start();
}

void MediaPlayer::onBuffered()
{
    if (mediaStatus() == ::MediaPlayer::BufferingMedia)
        mediaStatusChanged(::MediaPlayer::BufferedMedia);
}

void MediaPlayer::onFirstFrameRendered()
{
    // 视频首帧入队，从 BufferingMedia 切换到 BufferedMedia
    // 优先于 onBuffered 触发（首帧远早于缓冲满）
    if (mediaStatus() == ::MediaPlayer::BufferingMedia)
        mediaStatusChanged(::MediaPlayer::BufferedMedia);
}

void MediaPlayer::onBufferProgressChanged(qint64 bufferedEndPositionUs)
{
    m_bufferedEndPositionUs = bufferedEndPositionUs;

    // 计算渐进式 bufferProgress (0.0 ~ 1.0)
    if (const qint64 dur = duration(); dur > 0) {
        // bufferedEndPositionUs 是微秒，duration() 返回毫秒
        float progress = static_cast<float>(
            std::min(bufferedEndPositionUs / 1000, dur)
        ) / dur;
        progress = std::clamp(progress, 0.f, 1.f);

        if (!qFuzzyCompare(progress, m_bufferProgress)) {
            m_bufferProgress = progress;
            bufferProgressChanged(progress);
        }
    }
}

float MediaPlayer::bufferProgress() const
{
    return m_bufferProgress;
}

void MediaPlayer::mediaStatusChanged(::MediaPlayer::MediaStatus status)
{
    if (mediaStatus() == status)
        return;

    // LoadingMedia/StalledMedia 时重置缓冲进度为 0
    // 其他状态下的缓冲进度由 onBufferProgressChanged 统一管理
    if (status == ::MediaPlayer::LoadingMedia || status == ::MediaPlayer::StalledMedia) {
        m_bufferedEndPositionUs = 0;
        if (!qFuzzyCompare(m_bufferProgress, 0.f)) {
            m_bufferProgress = 0.f;
            bufferProgressChanged(0.f);
        }
    }

    PlatformMediaPlayer::mediaStatusChanged(status);
}

::MediaTimeRange MediaPlayer::availablePlaybackRanges() const
{
    if (m_bufferedEndPositionUs <= 0)
        return {};

    // 从当前位置到已缓冲的最远位置
    const qint64 currentPosMs = position();
    const qint64 bufferedEndMs = m_bufferedEndPositionUs / 1000; // 微秒转毫秒
    ::MediaTimeRange range;
    range.addInterval(currentPosMs, std::max(currentPosMs, bufferedEndMs));
    return range;
}

qreal MediaPlayer::playbackRate() const
{
    return m_playbackRate;
}

void MediaPlayer::setPlaybackRate(qreal rate)
{
    const float effectiveRate = std::max(static_cast<float>(rate), 0.0f);

    if (qFuzzyCompare(m_playbackRate, effectiveRate))
        return;

    m_playbackRate = effectiveRate;

    if (m_playbackEngine)
        m_playbackEngine->setPlaybackRate(effectiveRate);

    playbackRateChanged(effectiveRate);
}

QUrl MediaPlayer::media() const
{
    return m_url;
}

const QIODevice *MediaPlayer::mediaStream() const
{
    return m_device;
}

void MediaPlayer::handleIncorrectMedia(::MediaPlayer::MediaStatus status)
{
    seekableChanged(false);
    audioAvailableChanged(false);
    videoAvailableChanged(false);
    metaDataChanged();
    mediaStatusChanged(status);
    m_playbackEngine = nullptr;
};

void MediaPlayer::setMedia(const QUrl &media, QIODevice *stream)
{
    if (m_cancelToken)
        m_cancelToken->cancel();

    m_loadMedia.waitForFinished();

    invalidateCoverCache();

    m_url = media;
    m_device = stream;
    m_playbackEngine = nullptr;

    // 切换媒体时重置片头片尾抑制标志
    m_suppressOpening = false;
    m_suppressEnding = false;

    if (media.isEmpty() && !stream) {
        handleIncorrectMedia(::MediaPlayer::NoMedia);
        return;
    }

    mediaStatusChanged(::MediaPlayer::LoadingMedia);

    m_requestedStatus = ::MediaPlayer::StoppedState;

    m_cancelToken = std::make_shared<CancelToken>();

    m_loadMedia = QtConcurrent::run([this, media, stream, cancelToken = m_cancelToken] {
        const MediaDataHolder::Maybe mediaHolder =
                MediaDataHolder::create(media, stream, playbackOptions(), cancelToken);

        QMetaObject::invokeMethod(this, [this, mediaHolder, cancelToken] {
            setMediaAsync(mediaHolder, cancelToken);
        });
    });
}

void MediaPlayer::setMediaAsync(MediaDataHolder::Maybe mediaDataHolder,
                                       const std::shared_ptr<CancelToken> &cancelToken)
{
    if (cancelToken->isCancelled()) {
        return;
    }

    Q_ASSERT(mediaStatus() == ::MediaPlayer::LoadingMedia);

    if (!mediaDataHolder) {
        const auto [code, description] = mediaDataHolder.error();
        error(code, description);
        handleIncorrectMedia(::MediaPlayer::MediaStatus::InvalidMedia);
        return;
    }

    m_playbackEngine = std::make_unique<PlaybackEngine>(playbackOptions());

    connect(m_playbackEngine.get(), &PlaybackEngine::endOfStream, this,
            &MediaPlayer::endOfStream);
    connect(m_playbackEngine.get(), &PlaybackEngine::errorOccured, this,
            &MediaPlayer::error);
    connect(m_playbackEngine.get(), &PlaybackEngine::loopChanged, this,
            &MediaPlayer::onLoopChanged);
    connect(m_playbackEngine.get(), &PlaybackEngine::buffered, this,
            &MediaPlayer::onBuffered);
    connect(m_playbackEngine.get(), &PlaybackEngine::bufferProgressChanged, this,
            &MediaPlayer::onBufferProgressChanged);
    connect(m_playbackEngine.get(), &PlaybackEngine::firstFrameRendered, this,
            &MediaPlayer::onFirstFrameRendered);
    connect(m_playbackEngine.get(), &PlaybackEngine::activeVideoDecoderChanged, this,
            [this](::PlaybackOptions::VideoDecoderPolicy policy) {
                PlatformMediaPlayer::activeDecoderChanged(policy);
            });

    m_playbackEngine->setMedia(std::move(*mediaDataHolder.value()));

    m_playbackEngine->setAudioBufferOutput(m_audioBufferOutput);
    m_playbackEngine->setAudioSink(m_audioOutput);
    m_playbackEngine->setVideoSink(m_videoSink);

    m_playbackEngine->setLoops(loops());
    m_playbackEngine->setPlaybackRate(m_playbackRate);
    m_playbackEngine->setPitchCompensation(m_pitchCompensation);
    m_playbackEngine->setSubtitleStyle(m_subtitleStyle);

    durationChanged(duration());
    tracksChanged();
    metaDataChanged();
    seekableChanged(m_playbackEngine->isSeekable());

    audioAvailableChanged(
            !m_playbackEngine->streamInfo(PlatformMediaPlayer::AudioStream).empty());
    videoAvailableChanged(
            !m_playbackEngine->streamInfo(PlatformMediaPlayer::VideoStream).empty());

    mediaStatusChanged(::MediaPlayer::LoadedMedia);

    if (m_requestedStatus != ::MediaPlayer::StoppedState) {
        if (m_requestedStatus == ::MediaPlayer::PlayingState)
            play();
        else if (m_requestedStatus == ::MediaPlayer::PausedState)
            pause();
    }
}

void MediaPlayer::play()
{
    if (mediaStatus() == ::MediaPlayer::LoadingMedia) {
        m_requestedStatus = ::MediaPlayer::PlayingState;
        return;
    }

    if (!m_playbackEngine) {
        return;
    }

    if (mediaStatus() == ::MediaPlayer::EndOfMedia && state() == ::MediaPlayer::StoppedState) {
        m_playbackEngine->seek(TrackPosition(0));
        positionChanged(0);
    }

    // 内核级片头跳过：如果配置了 opening 且当前位置在片头内，先 seek 到 opening 位置
    if (opening() > 0 && position() < opening()) {
        if (!m_suppressOpening) {
            m_playbackEngine->seek(toTrackPosition(UserTrackPosition(opening())));
            positionChanged(opening());
            openingSkipped();
        }
    }

    runPlayback();
}

void MediaPlayer::runPlayback()
{
    m_playbackEngine->play();
    m_positionUpdateTimer.start();
    stateChanged(::MediaPlayer::PlayingState);

    if (mediaStatus() == ::MediaPlayer::LoadedMedia || mediaStatus() == ::MediaPlayer::EndOfMedia)
        mediaStatusChanged(::MediaPlayer::BufferingMedia);
}

void MediaPlayer::pause()
{
    if (mediaStatus() == ::MediaPlayer::LoadingMedia) {
        m_requestedStatus = ::MediaPlayer::PausedState;
        return;
    }

    if (!m_playbackEngine)
        return;

    if (mediaStatus() == ::MediaPlayer::EndOfMedia && state() == ::MediaPlayer::StoppedState) {
        m_playbackEngine->seek(TrackPosition(0));
        positionChanged(0);
    }
    m_playbackEngine->pause();
    m_positionUpdateTimer.stop();
    stateChanged(::MediaPlayer::PausedState);

    if (mediaStatus() == ::MediaPlayer::LoadedMedia || mediaStatus() == ::MediaPlayer::EndOfMedia)
        mediaStatusChanged(::MediaPlayer::BufferingMedia);
}

void MediaPlayer::stop()
{
    if (mediaStatus() == ::MediaPlayer::LoadingMedia) {
        m_requestedStatus = ::MediaPlayer::StoppedState;
        return;
    }

    if (!m_playbackEngine)
        return;

    m_playbackEngine->stop();
    m_positionUpdateTimer.stop();
    m_playbackEngine->seek(TrackPosition(0));
    positionChanged(0);
    stateChanged(::MediaPlayer::StoppedState);
    mediaStatusChanged(::MediaPlayer::LoadedMedia);
}

void MediaPlayer::onAudioOutputDeviceChanged()
{
    if (state() != ::MediaPlayer::PlayingState)
        return;

    // Update the audio output to the new default device.
    // This triggers AudioOutput::deviceChanged() → AudioRenderer::onDeviceChanged()
    // → m_deviceChanged=true → freeOutput() → new sink created on the new device.
    if (m_audioOutput && m_audioOutput->q)
        m_audioOutput->q->setDevice(AudioDevice{});

    pause();
}

void MediaPlayer::setAudioOutput(PlatformAudioOutput *output)
{
    m_audioOutput = output;
    if (m_playbackEngine)
        m_playbackEngine->setAudioSink(output);
}

void MediaPlayer::setAudioBufferOutput(::AudioBufferOutput *output) {
    m_audioBufferOutput = output;
    if (m_playbackEngine)
        m_playbackEngine->setAudioBufferOutput(output);
}

::MediaMetaData MediaPlayer::metaData() const
{
    auto md = m_playbackEngine ? m_playbackEngine->metaData() : ::MediaMetaData{};

    // 当流中无法提取到标题时，从 URL 提取文件名（不含后缀）作为回退
    if (!md.keys().contains(::MediaMetaData::Title) && !m_url.isEmpty()) {
        QString baseName;

#ifdef Q_OS_ANDROID
        // Android: content:// URI 需要通过 ContentResolver 查询 DISPLAY_NAME
        if (m_url.scheme() == u"content") {
            const QString displayName = AndroidAudioUtil::getContentDisplayName(m_url.toString());
            if (!displayName.isEmpty()) {
                const int dotPos = displayName.lastIndexOf(u'.');
                baseName = (dotPos > 0) ? displayName.first(dotPos) : displayName;
            }
        }
#endif

        // 通用回退：从 URL 路径提取文件名
        if (baseName.isEmpty()) {
            const QString fileName = m_url.fileName();
            if (!fileName.isEmpty()) {
                const int dotPos = fileName.lastIndexOf(u'.');
                baseName = (dotPos > 0) ? fileName.first(dotPos) : fileName;
            }
        }

        if (!baseName.isEmpty())
            md.insert(::MediaMetaData::Title, baseName);
    }

    return md;
}

QList<ChapterInfo> MediaPlayer::chapters() const
{
    if (!m_playbackEngine)
        return {};

    return m_playbackEngine->chapters();
}

void MediaPlayer::setVideoSink(::VideoSink *sink)
{
    m_videoSink = sink;
    if (m_playbackEngine)
        m_playbackEngine->setVideoSink(sink);
}

::VideoSink *MediaPlayer::videoSink() const
{
    return m_videoSink;
}

int MediaPlayer::trackCount(TrackType type)
{
    return m_playbackEngine ? m_playbackEngine->streamInfo(type).count() : 0;
}

::MediaMetaData MediaPlayer::trackMetaData(TrackType type, int streamNumber)
{
    if (!m_playbackEngine || streamNumber < 0
        || streamNumber >= m_playbackEngine->streamInfo(type).count())
        return {};
    return m_playbackEngine->streamInfo(type).at(streamNumber).metaData;
}

int MediaPlayer::activeTrack(TrackType type)
{
    return m_playbackEngine ? m_playbackEngine->activeTrack(type) : -1;
}

void MediaPlayer::setActiveTrack(TrackType type, int streamNumber)
{
    if (!m_playbackEngine) {
        qz::Log::warn("Cannot set active track without open source");
        return;
    }

    int oldTrack = m_playbackEngine->activeTrack(type);
    m_playbackEngine->setActiveTrack(type, streamNumber);
    int newTrack = m_playbackEngine->activeTrack(type);

    if (oldTrack != newTrack)
        tracksChanged();
}

void MediaPlayer::setLoops(int loops)
{
    if (m_playbackEngine)
        m_playbackEngine->setLoops(loops);

    PlatformMediaPlayer::setLoops(loops);
}

void MediaPlayer::setPitchCompensation(bool enabled)
{
    if (enabled == m_pitchCompensation)
        return;

    m_pitchCompensation = enabled;
    if (m_playbackEngine)
        m_playbackEngine->setPitchCompensation(enabled);
    PlatformMediaPlayer::pitchCompensationChanged(enabled);
}

bool MediaPlayer::pitchCompensation() const
{
    return m_pitchCompensation;
}

PlatformMediaPlayer::PitchCompensationAvailability
MediaPlayer::pitchCompensationAvailability() const
{
    return PitchCompensationAvailability::Available;
}

void MediaPlayer::setPlaybackOptions(const ::PlaybackOptions &options)
{
    if (m_playbackEngine)
        m_playbackEngine->setPlaybackOptions(options);

    playbackOptionsChanged();
}

void MediaPlayer::setSubtitleStyle(const SubtitleStyle &style)
{
    m_subtitleStyle = style;
    if (m_playbackEngine)
        m_playbackEngine->setSubtitleStyle(style);
}

void MediaPlayer::setOpening(qint64 ms)
{
    const qint64 oldOpening = opening();
    PlatformMediaPlayer::setOpening(ms);
    if (m_playbackEngine)
        m_playbackEngine->setOpening(toTrackPosition(UserTrackPosition(ms)));

    // 播放中更新片头值：若当前在片头区域内，抑制片头跳过（仅当前视频生效）
    if (ms != oldOpening && state() == ::MediaPlayer::PlayingState && ms > 0) {
        const qint64 pos = position();
        if (pos < ms) {
            m_suppressOpening = true;
        }
    }
}

void MediaPlayer::setEnding(qint64 ms)
{
    const qint64 oldEnding = ending();
    PlatformMediaPlayer::setEnding(ms);
    if (m_playbackEngine) {
        const qint64 endingPos = duration() - ms;
        m_playbackEngine->setEnding(toTrackPosition(UserTrackPosition(qMax(endingPos, 0ll))));
    }

    // 播放中更新片尾值：若当前在片尾区域内，抑制片尾跳过（仅当前视频生效）
    if (ms != oldEnding && state() == ::MediaPlayer::PlayingState && ms > 0) {
        const qint64 pos = position();
        const qint64 endingPos = duration() - ms;
        if (pos >= endingPos) {
            m_suppressEnding = true;
        }
    }
}

QImage MediaPlayer::getMediaCover(bool decodeFrame)
{
    if (m_coverCancelToken)
        m_coverCancelToken->cancel();

    m_coverCancelToken = std::make_shared<CancelToken>();

    QImage coverSource;

    if (m_playbackEngine)
        coverSource = m_playbackEngine->thumbnailImage();

    if (decodeFrame && coverSource.isNull() && !m_url.isEmpty())
        coverSource = decodeVideoFrameCover(m_url, m_coverCancelToken,
                                            m_playbackEngine ? m_playbackEngine->avContext() : nullptr);

    m_coverCancelToken.reset();

    return coverSource;
}

void MediaPlayer::cancelCoverRequest()
{
    if (m_coverCancelToken)
        m_coverCancelToken->cancel();
}

}

QT_END_NAMESPACE