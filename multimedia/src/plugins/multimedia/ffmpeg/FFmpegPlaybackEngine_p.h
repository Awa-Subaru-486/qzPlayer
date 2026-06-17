#ifndef FFMPEGPLAYBACKENGINE_P_H
#define FFMPEGPLAYBACKENGINE_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegPlaybackEngineDefs_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegTimeController_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegMediaDataHolder_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegCodecContext_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegPlaybackUtils_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegTime_p.h>
#include <qzMultimedia/PlaybackOptions.h>
#include <qzMultimedia/SubtitleStyle.h>

#include <QtCore/qpointer.h>

#include <unordered_map>

QT_BEGIN_NAMESPACE

class AudioSink;
class VideoSink;
class AudioOutput;
class AudioBufferOutput;
class MediaPlayer;
class PlaybackOptions;

namespace ffmpeg
{

// 播放引擎核心类，协调解复用、解码和渲染
class PlaybackEngine : public QObject
{
    Q_OBJECT
public:
    explicit PlaybackEngine(const ::PlaybackOptions &options);

    ~PlaybackEngine() override;

    void setMedia(MediaDataHolder media);

    void setVideoSink(::VideoSink *sink);

    void setAudioSink(::AudioOutput *output);

    void setAudioSink(PlatformAudioOutput *output);

    void setAudioBufferOutput(::AudioBufferOutput *output);

    void setState(::MediaPlayer::PlaybackState state);

    void play() {
        setState(::MediaPlayer::PlayingState);
    }
    void pause() {
        setState(::MediaPlayer::PausedState);
    }
    void stop() {
        setState(::MediaPlayer::StoppedState);
    }

    void seek(TrackPosition pos);

    void setLoops(int loopsCount);

    void setPlaybackRate(float rate);

    float playbackRate() const;

    void setActiveTrack(PlatformMediaPlayer::TrackType type, int streamNumber);

    TrackPosition currentPosition(bool topPos = true) const;

    TrackDuration duration() const;

    bool isSeekable() const;

    const QList<MediaDataHolder::StreamInfo> &
    streamInfo(PlatformMediaPlayer::TrackType trackType) const;

    const ::MediaMetaData &metaData() const;

    QImage thumbnailImage() const;

    const QList<ChapterInfo> &chapters() const;

    int activeTrack(PlatformMediaPlayer::TrackType type) const;

    [[nodiscard]] AVFormatContext *avContext() const;

    void setPitchCompensation(bool enabled);

    void setPlaybackOptions(const ::PlaybackOptions &options);

    void setSubtitleStyle(const SubtitleStyle &style);

    void setOpening(TrackPosition pos);
    void setEnding(TrackPosition pos);

signals:
    void endOfStream();
    void errorOccured(::MediaPlayer::Error, const QString &);
    void loopChanged();
    void buffered();
    void bufferProgressChanged(qint64 bufferedEndPositionUs);
    void firstFrameRendered();
    void activeVideoDecoderChanged(::PlaybackOptions::VideoDecoderPolicy activeDecoder);

private:
    SubtitleStyle m_subtitleStyle;

protected:
    // 播放引擎对象删除器，确保线程安全销毁
    struct ObjectDeleter
    {
        void operator()(PlaybackEngineObject *) const;

        PlaybackEngine *engine = nullptr;
    };

    template<typename T>
    using ObjectPtr = std::unique_ptr<T, ObjectDeleter>;

    using RendererPtr = ObjectPtr<Renderer>;
    using StreamPtr = ObjectPtr<StreamDecoder>;

    template<typename T, typename... Args>
    ObjectPtr<T> createPlaybackEngineObject(Args &&...args);

    virtual RendererPtr createRenderer(PlatformMediaPlayer::TrackType trackType);

    template <typename AudioOutput>
    void updateActiveAudioOutput(AudioOutput *output);

    void updateActiveVideoOutput(::VideoSink *sink, bool cleanOutput = false);

private:
    void createStreamAndRenderer(PlatformMediaPlayer::TrackType trackType);

    void createDemuxer();

    void registerObject(PlaybackEngineObject &object);

    template<typename C, typename Action>
    void forEachExistingObject(Action &&action);

    template<typename Action>
    void forEachExistingObject(Action &&action);

    void forceUpdate();

    void recreateObjects();

    void createObjectsIfNeeded();

    void updateObjectsPausedState();

    void deleteFreeThreads();

    void onFirstPacketFound(const PlaybackEngineObjectID &id, TrackPosition absSeekPos);

    void onRendererSynchronized(const PlaybackEngineObjectID &id, SteadyClock::time_point timePoint,
                                TrackPosition trackPosition);

    void onRendererFinished(const PlaybackEngineObjectID &id);

    void onRendererLoopChanged(const PlaybackEngineObjectID &id, TrackPosition offset,
                               int loopIndex);

    void triggerStepIfNeeded();

    static QString objectThreadName(const PlaybackEngineObject &object);

    std::optional<CodecContext> codecContextForTrack(PlatformMediaPlayer::TrackType trackType);

    bool hasMediaStream() const;

    void finilizeTime(TrackPosition pos);

    void finalizeOutputs();

    bool hasRenderer(const PlaybackEngineObjectID &id) const;

    template <typename T>
    bool checkObjectID(T &object, const PlaybackEngineObjectID &id) const
    {
        return object && object->objectID() == id.objectID && id.sessionID == m_currentID.sessionID;
    }

    void updateVideoSinkSize(::VideoSink *prevSink = nullptr);

    TrackPosition boundPosition(TrackPosition position) const;

    AudioRenderer *getAudioRenderer();

private:
    MediaDataHolder m_media;

    TimeController m_timeController;

    std::unordered_map<QString, std::unique_ptr<QThread>> m_threads;
    bool m_threadsDirty = false;

    QPointer<::VideoSink> m_videoSink;
    QPointer<::AudioOutput> m_audioOutput;
    QPointer<::AudioBufferOutput> m_audioBufferOutput;

    ::MediaPlayer::PlaybackState m_state = ::MediaPlayer::StoppedState;

    ObjectPtr<Demuxer> m_demuxer;
    std::array<StreamPtr, PlatformMediaPlayer::NTrackTypes> m_streams;
    std::array<RendererPtr, PlatformMediaPlayer::NTrackTypes> m_renderers;

    bool m_seekPending = false;

    std::array<std::optional<CodecContext>, PlatformMediaPlayer::NTrackTypes> m_codecContexts;
    int m_loops = 1;
    LoopOffset m_currentLoopOffset;

    bool m_pitchCompensation = true;
    ::PlaybackOptions m_options;
    PlaybackEngineObjectID m_currentID{ 1, 1 };
    TrackPosition m_openingUs = TrackPosition(0);
    TrackPosition m_endingUs = TrackPosition(0);
};

template<typename T, typename... Args>
PlaybackEngine::ObjectPtr<T> PlaybackEngine::createPlaybackEngineObject(Args &&...args)
{
    ++m_currentID.objectID;
    auto result = ObjectPtr<T>(new T(m_currentID, std::forward<Args>(args)...), { this });
    registerObject(*result);
    return result;
}
}

QT_END_NAMESPACE

#endif
