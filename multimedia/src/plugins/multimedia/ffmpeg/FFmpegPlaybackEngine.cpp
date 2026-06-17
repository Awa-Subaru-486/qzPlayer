#include "FFmpegPlaybackEngine_p.h"

#include "VideoSink.h"
#include "AudioOutput.h"
#include "private/PlatformAudioOutput_p.h"
#include "private/PlatformVideoSink_p.h"
#include "private/AudioBufferOutput_p.h"
#include "qiodevice.h"
#include "playbackengine/FFmpegDemuxer_p.h"
#include "playbackengine/FFmpegStreamDecoder_p.h"
#include "playbackengine/FFmpegSubtitleRenderer_p.h"
#include "playbackengine/FFmpegVideoRenderer_p.h"
#include "playbackengine/FFmpegAudioRenderer_p.h"

#include <rhi/qrhi.h>

import qzLog;

QT_BEGIN_NAMESPACE

namespace ffmpeg {

static qz::Log::LogCategory qLcPlaybackEngine("qz.multimedia.ffmpeg.playbackengine");

template <typename Array>
inline static Array defaultObjectsArray()
{
    using T = typename Array::value_type;
    return { T{ {}, {} }, T{ {}, {} }, T{ {}, {} } };
}

PlaybackEngine::PlaybackEngine(const ::PlaybackOptions &options)
    : m_demuxer({}, {}),
      m_streams(defaultObjectsArray<decltype(m_streams)>()),
      m_renderers(defaultObjectsArray<decltype(m_renderers)>()),
      m_options{ options }
{
    qz::Log::cat_debug(qLcPlaybackEngine, "Create PlaybackEngine");
    qRegisterMetaType<Packet>();
    qRegisterMetaType<Frame>();
    qRegisterMetaType<TrackPosition>();
    qRegisterMetaType<PlaybackEngineObjectID>();
}

PlaybackEngine::~PlaybackEngine()
{
    finalizeOutputs();
    forEachExistingObject([](auto &object) { object.reset(); });
    deleteFreeThreads();
}

void PlaybackEngine::onRendererFinished(const PlaybackEngineObjectID &id)
{
    if (!hasRenderer(id))
        return;

    auto isAtEnd = [this](auto trackType) {
        return !m_renderers[trackType] || m_renderers[trackType]->isAtEnd();
    };

    if (!isAtEnd(PlatformMediaPlayer::VideoStream))
        return;

    if (!isAtEnd(PlatformMediaPlayer::AudioStream))
        return;

    if (!isAtEnd(PlatformMediaPlayer::SubtitleStream) && !hasMediaStream())
        return;

    if (std::exchange(m_state, ::MediaPlayer::StoppedState) == ::MediaPlayer::StoppedState)
        return;

    finilizeTime(duration().asTimePoint());

    forceUpdate();

    qz::Log::cat_debug(qLcPlaybackEngine, "Playback engine end of stream");

    emit endOfStream();
}

void PlaybackEngine::onRendererLoopChanged(const PlaybackEngineObjectID &id, TrackPosition offset,
                                           int loopIndex)
{
    if (!hasRenderer(id))
        return;

    if (loopIndex > m_currentLoopOffset.loopIndex) {
        m_currentLoopOffset = { offset, loopIndex };
        emit loopChanged();
    } else if (loopIndex == m_currentLoopOffset.loopIndex && offset != m_currentLoopOffset.loopStartTimeUs) {
        qz::Log::warn("Unexpected offset for loop {} : {} vs {}", loopIndex, offset.get(), m_currentLoopOffset.loopStartTimeUs.get());
        m_currentLoopOffset.loopStartTimeUs = offset;
    }
}

void PlaybackEngine::onFirstPacketFound(const PlaybackEngineObjectID &id, TrackPosition absSeekPos)
{
    if (!checkObjectID(m_demuxer, id))
        return;

    if (m_timeController.isStarted())
        return;

    const SteadyClock::time_point now = SteadyClock::now();
    const SteadyClock::time_point expectedTimePoint = m_timeController.timeFromPosition(absSeekPos);
    const auto delay =
            std::chrono::round<std::chrono::microseconds>(now - expectedTimePoint);
    qz::Log::cat_debug(qLcPlaybackEngine, "Delay of demuxer initialization:{}", delay);
    m_timeController.sync(now, absSeekPos);
    m_timeController.start();

    forEachExistingObject<Renderer>(
            [&](auto &renderer) { renderer->setTimeController(m_timeController); });
}

void PlaybackEngine::onRendererSynchronized(const PlaybackEngineObjectID &id,
                                            SteadyClock::time_point tp, TrackPosition pos)
{
    if (!hasRenderer(id))
        return;

    Q_ASSERT(checkObjectID(m_renderers[PlatformMediaPlayer::AudioStream], id));

    forEachExistingObject<Renderer>([&](auto &renderer) {
        if (id.objectID != renderer->objectID()) {
            auto tc = m_timeController;
            tc.syncSoft(tp, pos);
            renderer->setTimeController(tc);
        }
    });

    m_timeController.sync(tp, pos);
}

void PlaybackEngine::setState(::MediaPlayer::PlaybackState state) {
    if (!m_media.avContext())
        return;

    if (state == m_state)
        return;

    const auto prevState = std::exchange(m_state, state);

    if (m_state == ::MediaPlayer::StoppedState) {
        finalizeOutputs();
        finilizeTime(TrackPosition(0));
    }

    if (prevState == ::MediaPlayer::StoppedState || m_state == ::MediaPlayer::StoppedState)
        recreateObjects();

    if (prevState == ::MediaPlayer::StoppedState)
        triggerStepIfNeeded();

    updateObjectsPausedState();
}

void PlaybackEngine::updateObjectsPausedState()
{
    const bool paused = m_state != ::MediaPlayer::PlayingState;
    m_timeController.setPaused(paused);

    forEachExistingObject([&](auto &object) {
        if constexpr (std::is_same_v<decltype(*object), Renderer &>)
            object->setPaused(paused);
        else
            object->setPaused(false);
    });
}

void PlaybackEngine::ObjectDeleter::operator()(PlaybackEngineObject *object) const
{
    Q_ASSERT(engine);
    if (!std::exchange(engine->m_threadsDirty, true))
        QMetaObject::invokeMethod(engine, &PlaybackEngine::deleteFreeThreads, Qt::QueuedConnection);

    object->kill();
}

void PlaybackEngine::registerObject(PlaybackEngineObject &object)
{
    connect(&object, &PlaybackEngineObject::error, this, &PlaybackEngine::errorOccured);

    auto threadName = objectThreadName(object);
    auto &thread = m_threads[threadName];
    if (!thread) {
        thread = std::make_unique<QThread>();
        thread->setObjectName(threadName);
        thread->start();
    }

    Q_ASSERT(object.thread() != thread.get());
    object.moveToThread(thread.get());
}

PlaybackEngine::RendererPtr
PlaybackEngine::createRenderer(PlatformMediaPlayer::TrackType trackType)
{
    switch (trackType) {
    case PlatformMediaPlayer::VideoStream:
        return m_videoSink ? createPlaybackEngineObject<VideoRenderer>(
                       m_timeController, m_videoSink, m_media.transformation())
                           : RendererPtr{ {}, {} };
    case PlatformMediaPlayer::AudioStream:
        return m_audioOutput || m_audioBufferOutput
                ? createPlaybackEngineObject<AudioRenderer>(
                          m_timeController, m_audioOutput, m_audioBufferOutput, m_pitchCompensation)
                : RendererPtr{ {}, {} };
    case PlatformMediaPlayer::SubtitleStream: {
        auto renderer = m_videoSink
                ? createPlaybackEngineObject<SubtitleRenderer>(m_timeController, m_videoSink)
                : RendererPtr{ {}, {} };
        if (renderer) {
            auto *subRenderer = static_cast<SubtitleRenderer *>(renderer.get());
            subRenderer->setSubtitleStyle(m_subtitleStyle);
        }
        return renderer;
    }
    default:
        return { {}, {} };
    }
}

template<typename C, typename Action>
void PlaybackEngine::forEachExistingObject(Action &&action)
{
    auto handleNotNullObject = [&](auto &object) {
        if constexpr (std::is_base_of_v<C, std::remove_reference_t<decltype(*object)>>)
            if (object)
                action(object);
    };

    handleNotNullObject(m_demuxer);
    std::for_each(m_streams.begin(), m_streams.end(), handleNotNullObject);
    std::for_each(m_renderers.begin(), m_renderers.end(), handleNotNullObject);
}

template<typename Action>
void PlaybackEngine::forEachExistingObject(Action &&action)
{
    forEachExistingObject<PlaybackEngineObject>(std::forward<Action>(action));
}

void PlaybackEngine::seek(TrackPosition pos)
{
    pos = boundPosition(pos);

    m_timeController.deactivate();
    m_timeController.sync(m_currentLoopOffset.loopStartTimeUs.asDuration() + pos);
    m_seekPending = true;

    forceUpdate();
}

void PlaybackEngine::setLoops(int loops)
{
    if (!isSeekable()) {
        qz::Log::warn("Cannot set loops for non-seekable source");
        return;
    }

    if (std::exchange(m_loops, loops) == loops)
        return;

    qz::Log::cat_debug(qLcPlaybackEngine, "set playback engine loops:{} prev loops:{} index:{}", loops, m_loops, m_currentLoopOffset.loopIndex);

    if (m_demuxer)
        m_demuxer->setLoops(loops);
}

void PlaybackEngine::triggerStepIfNeeded()
{
    if (m_state != ::MediaPlayer::PausedState)
        return;

    if (m_renderers[PlatformMediaPlayer::VideoStream])
        m_renderers[PlatformMediaPlayer::VideoStream]->doForceStep();

}

QString PlaybackEngine::objectThreadName(const PlaybackEngineObject &object)
{
    QString result = QString::fromLatin1(object.metaObject()->className());
    if (auto stream = qobject_cast<const StreamDecoder *>(&object))
        result += QString::number(stream->trackType());

    return result;
}

void PlaybackEngine::setPlaybackRate(float rate) {
    if (rate == playbackRate())
        return;

    m_timeController.setPlaybackRate(rate);
    forEachExistingObject<Renderer>([rate](auto &renderer) { renderer->setPlaybackRate(rate); });
}

float PlaybackEngine::playbackRate() const {
    return m_timeController.playbackRate();
}

void PlaybackEngine::recreateObjects()
{
    m_timeController.deactivate();

    forEachExistingObject([](auto &object) { object.reset(); });

    createObjectsIfNeeded();
}

void PlaybackEngine::createObjectsIfNeeded()
{
    if (m_state == ::MediaPlayer::StoppedState || !m_media.avContext())
        return;

    qz::Log::cat_debug(qLcPlaybackEngine, "createObjectsIfNeeded: state={} timeControllerStarted={}",
                        static_cast<int>(m_state), m_timeController.isStarted());

    for (int i = 0; i < PlatformMediaPlayer::NTrackTypes; ++i)
        createStreamAndRenderer(static_cast<PlatformMediaPlayer::TrackType>(i));

    createDemuxer();

    if (!m_demuxer)
        m_timeController.start();
}

void PlaybackEngine::forceUpdate()
{
    recreateObjects();
    triggerStepIfNeeded();
    updateObjectsPausedState();
}

void PlaybackEngine::createStreamAndRenderer(PlatformMediaPlayer::TrackType trackType)
{
    auto codecContext = codecContextForTrack(trackType);

    if (!codecContext)
        return;

    auto &renderer = m_renderers[trackType];

    if (!renderer) {
        renderer = createRenderer(trackType);

        if (!renderer) {
            return;
        }

        connect(renderer.get(), &Renderer::synchronized, this,
                &PlaybackEngine::onRendererSynchronized);

        connect(renderer.get(), &Renderer::loopChanged, this,
                &PlaybackEngine::onRendererLoopChanged);

        connect(renderer.get(), &PlaybackEngineObject::atEnd, this,
                &PlaybackEngine::onRendererFinished);

        // 视频渲染器首帧到达时，通知上层（用于 seek 后隐藏加载弹窗）
        if (trackType == PlatformMediaPlayer::VideoStream) {
            connect(renderer.get(), &Renderer::firstFrameReceived, this,
                    &PlaybackEngine::firstFrameRendered);
        }
    }

    const auto &stream = m_streams[trackType] =
            createPlaybackEngineObject<StreamDecoder>(*codecContext, renderer->seekPosition());

    Q_ASSERT(trackType == stream->trackType());

    connect(stream.get(), &StreamDecoder::requestHandleFrame, renderer.get(), &Renderer::render);
    connect(stream.get(), &PlaybackEngineObject::atEnd, renderer.get(),
            &Renderer::onFinalFrameReceived);
    connect(renderer.get(), &Renderer::frameProcessed, stream.get(),
            &StreamDecoder::onFrameProcessed);
}

std::optional<CodecContext> PlaybackEngine::codecContextForTrack(PlatformMediaPlayer::TrackType trackType)
{
    const auto streamIndex = m_media.currentStreamIndex(trackType);
    if (streamIndex < 0)
        return {};

    auto &codecContext = m_codecContexts[trackType];

    if (!codecContext) {
        qz::Log::cat_debug(qLcPlaybackEngine, "Create codec for stream:{} trackType:{}", streamIndex, static_cast<int>(trackType));

        QRhi *rhi = nullptr;
        if (trackType == PlatformMediaPlayer::VideoStream && m_videoSink) {
            rhi = m_videoSink->rhi();
            qz::Log::cat_debug(qLcPlaybackEngine, "Video sink RHI available:{} backend:{}", rhi ? "yes" : "no", rhi ? static_cast<int>(rhi->backend()) : -1);
        }

        auto maybeCodecContext = CodecContext::create(m_media.avContext()->streams[streamIndex],
                                                      m_media.avContext(), m_options, rhi);

        if (!maybeCodecContext) {
            qz::Log::cat_warn(qLcPlaybackEngine, "Failed to create codec for stream:{} trackType:{} error:{}",
                               streamIndex, static_cast<int>(trackType), maybeCodecContext.error());
            emit errorOccured(::MediaPlayer::FormatError,
                              u"Cannot create codec," + maybeCodecContext.error());
            return {};
        }

        codecContext = maybeCodecContext.value();

        qz::Log::cat_debug(qLcPlaybackEngine, "Codec created for stream:{} trackType:{} codecName:{}",
                            streamIndex, static_cast<int>(trackType),
                            codecContext->context()->codec->name);

        if (trackType == PlatformMediaPlayer::VideoStream) {
            ::PlaybackOptions::VideoDecoderPolicy activeDecoder =
                    ::PlaybackOptions::VideoDecoderPolicy::Software;
            if (const auto *accel = codecContext->hwAccel()) {
                switch (accel->deviceType()) {
                case AV_HWDEVICE_TYPE_D3D11VA:
                    activeDecoder = ::PlaybackOptions::VideoDecoderPolicy::HardwareD3D11VA;
                    break;
                case AV_HWDEVICE_TYPE_VULKAN:
                    activeDecoder = ::PlaybackOptions::VideoDecoderPolicy::HardwareVkVideo;
                    break;
                case AV_HWDEVICE_TYPE_MEDIACODEC:
                    activeDecoder = ::PlaybackOptions::VideoDecoderPolicy::HardwareMediaVideo;
                    break;
                default:
                    // 未知硬件设备类型，回退到平台默认
#ifdef Q_OS_WINDOWS
                    activeDecoder = ::PlaybackOptions::VideoDecoderPolicy::HardwareD3D11VA;
#elif defined(Q_OS_ANDROID)
                    activeDecoder = ::PlaybackOptions::VideoDecoderPolicy::HardwareMediaVideo;
#else
                    activeDecoder = ::PlaybackOptions::VideoDecoderPolicy::HardwareVkVideo;
#endif
                    break;
                }
            }
            emit activeVideoDecoderChanged(activeDecoder);
        }
    }

    return codecContext;
}

bool PlaybackEngine::hasMediaStream() const
{
    return m_renderers[PlatformMediaPlayer::AudioStream]
            || m_renderers[PlatformMediaPlayer::VideoStream];
}

void PlaybackEngine::createDemuxer()
{
    std::array<int, PlatformMediaPlayer::NTrackTypes> streamIndexes = { -1, -1, -1 };

    bool hasStreams = false;
    forEachExistingObject<StreamDecoder>([&](auto &stream) {
        hasStreams = true;
        const auto trackType = stream->trackType();
        streamIndexes[trackType] = m_media.currentStreamIndex(trackType);
    });

    qz::Log::cat_debug(qLcPlaybackEngine, "createDemuxer: streamIndexes video={} audio={} subtitle={}",
                        streamIndexes[PlatformMediaPlayer::VideoStream],
                        streamIndexes[PlatformMediaPlayer::AudioStream],
                        streamIndexes[PlatformMediaPlayer::SubtitleStream]);

    if (!hasStreams)
        return;

    const TrackPosition currentLoopPosUs = currentPosition(false);

    m_demuxer = createPlaybackEngineObject<Demuxer>(m_media.avContext(), currentLoopPosUs,
                                                    m_seekPending, m_currentLoopOffset,
                                                    streamIndexes, m_loops);

    m_seekPending = false;

    if (m_openingUs > TrackPosition(0))
        m_demuxer->setOpening(m_openingUs);
    if (m_endingUs > TrackPosition(0))
        m_demuxer->setEnding(m_endingUs);

    connect(m_demuxer.get(), &Demuxer::packetsBuffered, this, &PlaybackEngine::buffered);
    connect(m_demuxer.get(), &Demuxer::bufferProgressChanged, this,
            &PlaybackEngine::bufferProgressChanged);

    forEachExistingObject<StreamDecoder>([&](auto &stream) {
        connect(m_demuxer.get(), Demuxer::signalByTrackType(stream->trackType()), stream.get(),
                &StreamDecoder::decode);
        connect(m_demuxer.get(), &PlaybackEngineObject::atEnd, stream.get(),
                &StreamDecoder::onFinalPacketReceived);
        connect(stream.get(), &StreamDecoder::packetProcessed, m_demuxer.get(),
                &Demuxer::onPacketProcessed);
    });

    connect(m_demuxer.get(), &Demuxer::firstPacketFound, this, &PlaybackEngine::onFirstPacketFound);
}

void PlaybackEngine::deleteFreeThreads() {
    m_threadsDirty = false;
    auto freeThreads = std::move(m_threads);

    forEachExistingObject([&](auto &object) {
        m_threads.insert(freeThreads.extract(objectThreadName(*object)));
    });

    for (auto &[name, thr] : freeThreads)
        thr->quit();

    for (auto &[name, thr] : freeThreads)
        thr->wait();
}

void PlaybackEngine::setMedia(MediaDataHolder media)
{
    Q_ASSERT(!m_media.avContext());
    Q_ASSERT(m_state == ::MediaPlayer::StoppedState);
    Q_ASSERT(m_threads.empty());

    m_media = std::move(media);
    updateVideoSinkSize();
}

void PlaybackEngine::setVideoSink(::VideoSink *sink)
{
    auto prev = std::exchange(m_videoSink, sink);
    if (prev == sink)
        return;

    updateVideoSinkSize(prev);
    updateActiveVideoOutput(sink);

    if (!sink || !prev) {

        forceUpdate();
    }
}

void PlaybackEngine::setAudioSink(PlatformAudioOutput *output) {
    setAudioSink(output ? output->q : nullptr);
}

void PlaybackEngine::setAudioSink(::AudioOutput *output)
{
    ::AudioOutput *prev = std::exchange(m_audioOutput, output);
    if (prev == output)
        return;

    updateActiveAudioOutput(output);

    if (!output || !prev) {

        forceUpdate();
    }
}

void PlaybackEngine::setAudioBufferOutput(::AudioBufferOutput *output)
{
    ::AudioBufferOutput *prev = std::exchange(m_audioBufferOutput, output);
    if (prev == output)
        return;
    updateActiveAudioOutput(output);
}

TrackPosition PlaybackEngine::currentPosition(bool topPos) const
{
    std::optional<TrackPosition> pos;

    for (size_t i = 0; i < m_renderers.size(); ++i) {
        const auto &renderer = m_renderers[i];
        if (!renderer)
            continue;

        if (!topPos && i == PlatformMediaPlayer::SubtitleStream && hasMediaStream())
            continue;

        const auto rendererPos = renderer->lastPosition();
        pos = !pos       ? rendererPos
                : topPos ? std::max(*pos, rendererPos)
                         : std::min(*pos, rendererPos);
    }

    if (!pos)
        pos = m_timeController.currentPosition();

    return boundPosition(*pos - m_currentLoopOffset.loopStartTimeUs.asDuration());
}

TrackDuration PlaybackEngine::duration() const
{
    return m_media.duration();
}

bool PlaybackEngine::isSeekable() const { return m_media.isSeekable(); }

const QList<MediaDataHolder::StreamInfo> &
PlaybackEngine::streamInfo(PlatformMediaPlayer::TrackType trackType) const
{
    return m_media.streamInfo(trackType);
}

const ::MediaMetaData &PlaybackEngine::metaData() const
{
    return m_media.metaData();
}

QImage PlaybackEngine::thumbnailImage() const
{
    return m_media.thumbnailImage();
}

const QList<ChapterInfo> &PlaybackEngine::chapters() const
{
    return m_media.chapters();
}

int PlaybackEngine::activeTrack(PlatformMediaPlayer::TrackType type) const
{
    return m_media.activeTrack(type);
}

AVFormatContext *PlaybackEngine::avContext() const
{
    return m_media.avContext();
}

void PlaybackEngine::setPitchCompensation(bool enabled)
{
    m_pitchCompensation = enabled;
    if (AudioRenderer *renderer = getAudioRenderer())
        renderer->setPitchCompensation(enabled);
}

void PlaybackEngine::setPlaybackOptions(const ::PlaybackOptions &options)
{
    if (m_options == options)
        return;

    m_options = options;

    m_codecContexts = {};

    // 先销毁 StreamDecoder 和 Renderer，释放所有持有 AVFrame 的对象，
    // 确保在 CodecContext 和 HWAccel 销毁之前释放 HW 帧资源，
    // 避免 AVFrame 的 hw_frames_ctx/hw_device_ctx 引用已释放的 HW 资源导致崩溃。
    for (size_t i = 0; i < PlatformMediaPlayer::NTrackTypes; ++i) {
        m_renderers[i].reset();
        m_streams[i].reset();
    }
    m_demuxer.reset();

    createObjectsIfNeeded();
    updateObjectsPausedState();
}

void PlaybackEngine::setSubtitleStyle(const SubtitleStyle &style)
{
    m_subtitleStyle = style;
    auto *subtitleRenderer = static_cast<SubtitleRenderer *>(m_renderers[PlatformMediaPlayer::SubtitleStream].get());
    if (subtitleRenderer)
        subtitleRenderer->setSubtitleStyle(style);
}

void PlaybackEngine::setOpening(TrackPosition pos)
{
    m_openingUs = pos;
    if (m_demuxer)
        m_demuxer->setOpening(pos);
}

void PlaybackEngine::setEnding(TrackPosition pos)
{
    m_endingUs = pos;
    if (m_demuxer)
        m_demuxer->setEnding(pos);
}

void PlaybackEngine::setActiveTrack(PlatformMediaPlayer::TrackType trackType, int streamNumber)
{
    qz::Log::cat_debug(qLcPlaybackEngine, "setActiveTrack: trackType={} streamNumber={} state={} timeControllerStarted={}",
                        static_cast<int>(trackType), streamNumber, static_cast<int>(m_state),
                        m_timeController.isStarted());

    if (!m_media.setActiveTrack(trackType, streamNumber))
        return;
    
    m_codecContexts[trackType] = {};

    // 先销毁 Renderer 和 StreamDecoder，释放所有持有 AVFrame 的对象，
    // 确保在 CodecContext 和 HWAccel 销毁之前释放 HW 帧资源。
    m_renderers[trackType].reset();
    m_streams = defaultObjectsArray<decltype(m_streams)>();
    m_demuxer.reset();

    updateVideoSinkSize();
    createObjectsIfNeeded();
    updateObjectsPausedState();

    qz::Log::cat_debug(qLcPlaybackEngine, "setActiveTrack done: trackType={} hasStreamDecoder={} hasRenderer={} hasDemuxer={}",
                        static_cast<int>(trackType),
                        !!m_streams[trackType], !!m_renderers[trackType], !!m_demuxer);

    if (trackType == PlatformMediaPlayer::VideoStream
        && !m_renderers[PlatformMediaPlayer::VideoStream] && m_videoSink) {
        m_videoSink->setVideoFrame({});
    }
}

void PlaybackEngine::finilizeTime(TrackPosition pos)
{
    Q_ASSERT(pos >= TrackPosition(0) && pos <= duration().asTimePoint());

    m_timeController.deactivate();
    m_timeController.sync(pos);
    m_currentLoopOffset = {};
}

void PlaybackEngine::finalizeOutputs()
{
    if (m_audioBufferOutput)
        updateActiveAudioOutput(static_cast<::AudioBufferOutput *>(nullptr));
    if (m_audioOutput)
        updateActiveAudioOutput(static_cast<::AudioOutput *>(nullptr));
    updateActiveVideoOutput(nullptr, true);
}

bool PlaybackEngine::hasRenderer(const PlaybackEngineObjectID &id) const
{
    return std::any_of(m_renderers.begin(), m_renderers.end(),
                       [&](auto &renderer) { return checkObjectID(renderer, id); });
}

template <typename AudioOutput>
void PlaybackEngine::updateActiveAudioOutput(AudioOutput *output)
{
    if (AudioRenderer *renderer = getAudioRenderer())
        renderer->setOutput(output);
}

void PlaybackEngine::updateActiveVideoOutput(::VideoSink *sink, bool cleanOutput)
{
    if (auto renderer = qobject_cast<SubtitleRenderer *>(
                m_renderers[PlatformMediaPlayer::SubtitleStream].get()))
        renderer->setOutput(sink, cleanOutput);
    if (auto renderer =
                qobject_cast<VideoRenderer *>(m_renderers[PlatformMediaPlayer::VideoStream].get()))
        renderer->setOutput(sink, cleanOutput);
}

void PlaybackEngine::updateVideoSinkSize(::VideoSink *prevSink)
{
    auto platformVideoSink = m_videoSink ? m_videoSink->platformVideoSink() : nullptr;
    if (!platformVideoSink)
        return;

    if (prevSink && prevSink->platformVideoSink())
        platformVideoSink->setNativeSize(prevSink->platformVideoSink()->nativeSize());
    else {
        const auto streamIndex = m_media.currentStreamIndex(PlatformMediaPlayer::VideoStream);
        if (streamIndex >= 0) {
            const auto context = m_media.avContext();
            const auto stream = context->streams[streamIndex];
            const AVRational pixelAspectRatio =
                    av_guess_sample_aspect_ratio(context, stream, nullptr);

            const QSize size =
                    qCalculateFrameSize({ stream->codecpar->width, stream->codecpar->height },
                                        { pixelAspectRatio.num, pixelAspectRatio.den });

            platformVideoSink->setNativeSize(
                    qRotatedFrameSize(size, m_media.transformation().rotation));
        }
    }
}

TrackPosition PlaybackEngine::boundPosition(TrackPosition position) const
{
    position = qMax(position, TrackPosition(0));
    return duration() > TrackDuration(0) ? qMin(position, duration().asTimePoint()) : position;
}

AudioRenderer *PlaybackEngine::getAudioRenderer()
{
    return qobject_cast<AudioRenderer *>(m_renderers[PlatformMediaPlayer::AudioStream].get());
}

}

QT_END_NAMESPACE

#include "moc_FFmpegPlaybackEngine_p.cpp"
