// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "MediaPlayer_p.h"

#include <private/MultimediaUtils_p.h>
#include <private/PlatformMediaIntegration_p.h>
#include <private/AudioBufferOutput_p.h>
#include <VideoSink.h>
#include <AudioOutput.h>

import qzLog;

#include <QtCore/qcoreevent.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qtimer.h>
#include <QtCore/qdebug.h>
#include <QtCore/qdir.h>
#include <QtCore/qpointer.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qtemporaryfile.h>
#include <QtCore/qcoreapplication.h>

#include <memory>

#include <algorithm>
#include <numeric>

#include <QtCore/qrandom.h>

#ifdef Q_OS_WINDOWS
#include <private/WindowsPlatformSpecificInterface_p.h>
#include <private/WindowsSmtcManager_p.h>
#endif

void MediaPlayerPrivate::setState(MediaPlayer::PlaybackState toState)
{
    Q_Q(MediaPlayer);

    if (toState != state) {
        const auto fromState = std::exchange(state, toState);
        if (toState == MediaPlayer::PlayingState || fromState == MediaPlayer::PlayingState)
            emit q->playingChanged(toState == MediaPlayer::PlayingState);
        emit q->playbackStateChanged(toState);
    }
}

void MediaPlayerPrivate::setStatus(MediaPlayer::MediaStatus s, MediaPlayer::MediaStatus oldStatus)
{
    Q_Q(MediaPlayer);

    const bool wasBuffering = (oldStatus == MediaPlayer::LoadingMedia
                               || oldStatus == MediaPlayer::BufferingMedia
                               || oldStatus == MediaPlayer::StalledMedia);
    const bool isBuffering = (s == MediaPlayer::LoadingMedia
                              || s == MediaPlayer::BufferingMedia
                              || s == MediaPlayer::StalledMedia);

    emit q->mediaStatusChanged(s);

    if (wasBuffering != isBuffering)
        emit q->bufferingChanged(isBuffering);

    // 播放列表自动切歌
    if (s == MediaPlayer::EndOfMedia)
        handleEndOfMedia();
}

void MediaPlayerPrivate::setError(MediaPlayer::Error error, const QString &errorString)
{
    Q_Q(MediaPlayer);

    this->error.setAndNotify(error, errorString, *q);
}

int MediaPlayerPrivate::nextPlaylistIndex() const
{
    if (playlistUrls.isEmpty())
        return -1;

    switch (m_playbackMode) {
    case MediaPlayer::SequentialPlayback:
        return playlistCurrentIndex + 1 < playlistUrls.size()
                   ? playlistCurrentIndex + 1
                   : -1;
    case MediaPlayer::LoopPlayback:
        return (playlistCurrentIndex + 1) % playlistUrls.size();
    case MediaPlayer::CurrentItemPlayback:
        return playlistCurrentIndex;
    case MediaPlayer::RandomPlayback:
        if (shuffleOrder.isEmpty())
            return -1;
        if (shufflePosition + 1 < shuffleOrder.size())
            return shuffleOrder[shufflePosition + 1];
        return -1;
    }
    return -1;
}

int MediaPlayerPrivate::previousPlaylistIndex() const
{
    if (playlistUrls.isEmpty())
        return -1;

    switch (m_playbackMode) {
    case MediaPlayer::SequentialPlayback:
        return playlistCurrentIndex - 1 >= 0
                   ? playlistCurrentIndex - 1
                   : -1;
    case MediaPlayer::LoopPlayback:
        return (playlistCurrentIndex - 1 + playlistUrls.size()) % playlistUrls.size();
    case MediaPlayer::CurrentItemPlayback:
        return playlistCurrentIndex;
    case MediaPlayer::RandomPlayback:
        if (shufflePosition > 0)
            return shuffleOrder[shufflePosition - 1];
        return playlistCurrentIndex;
    }
    return -1;
}

void MediaPlayerPrivate::regenerateShuffleOrder()
{
    shuffleOrder.resize(playlistUrls.size());
    std::iota(shuffleOrder.begin(), shuffleOrder.end(), 0);
    std::ranges::shuffle(shuffleOrder, *QRandomGenerator::global());
    // 将当前项放到首位
    if (playlistCurrentIndex >= 0) {
        int pos = shuffleOrder.indexOf(playlistCurrentIndex);
        if (pos > 0)
            std::swap(shuffleOrder[0], shuffleOrder[pos]);
    }
    shufflePosition = 0;
}

void MediaPlayerPrivate::handleEndOfMedia()
{
    Q_Q(MediaPlayer);

    // 无播放列表或列表为空，不处理
    if (playlistUrls.isEmpty())
        return;

    // loops > 1 时由平台层 doLoop() 处理单曲循环
    // 当 loops == 1 且播放列表存在时，由播放列表接管切歌
    if (q->loops() != 1)
        return;

    int nextIdx = nextPlaylistIndex();
    if (nextIdx < 0) {
        // 随机模式下播完一轮，重新生成并继续
        if (m_playbackMode == MediaPlayer::RandomPlayback) {
            regenerateShuffleOrder();
            nextIdx = shuffleOrder.isEmpty() ? -1 : shuffleOrder[0];
        }
        if (nextIdx < 0)
            return;
    }

    playlistCurrentIndex = nextIdx;

    // 更新随机位置
    if (m_playbackMode == MediaPlayer::RandomPlayback)
        shufflePosition = shuffleOrder.indexOf(nextIdx);

    // 加载并播放下一首
    q->setSource(playlistUrls.at(nextIdx));
    emit q->playlistIndexChanged(nextIdx);
    q->play();
}

void MediaPlayerPrivate::setMedia(const QUrl &media, QIODevice *stream)
{
    Q_Q(MediaPlayer);
    setError(MediaPlayer::NoError, {});

    // 清空轨道缓存
    m_audioTracks->setTracks({});
    m_videoTracks->setTracks({});
    m_subtitleTracks->setTracks({});
    m_chapters.clear();
    emit q->chaptersChanged();

    if (!control) {
        return;
    }

    auto setErrorFn = [&](
        MediaPlayer::MediaStatus status,
        MediaPlayer::Error err,
        const QString &errString)
    {
        control->setMedia(QUrl(), nullptr);
        control->mediaStatusChanged(status);
        control->error(err, errString);
    };

    std::unique_ptr<QFile> file;

    if (!media.isEmpty() && !stream && media.scheme() == QLatin1String("qrc")
        && !control->canPlayQrc()) {
        qrcMedia = media;

        file = std::make_unique<QFile>(QLatin1Char(':') + media.path());
        if (!file->open(QFile::ReadOnly)) {
            file.reset();
            setErrorFn(
                MediaPlayer::InvalidMedia,
                MediaPlayer::ResourceError,
                MediaPlayer::tr("Attempting to play invalid Qt resource"));

        } else if (control->streamPlaybackSupported()) {
            control->setMedia(media, file.get());
        } else {
#if QT_CONFIG(temporaryfile)
            auto tempFile = std::make_unique<QTemporaryFile>();

            if (const QString suffix = QFileInfo(*file).suffix(); !suffix.isEmpty())
                tempFile->setFileTemplate(tempFile->fileTemplate() + QLatin1Char('.') + suffix);

            if (!tempFile->open()) {
                setErrorFn(
                    MediaPlayer::InvalidMedia,
                    MediaPlayer::ResourceError,
                    tempFile->errorString());
                qrcFile.reset();
                return;
            }
            char buffer[4096];
            while (true) {
                const qint64 len = file->read(buffer, sizeof(buffer));
                if (len < 1)
                    break;
                tempFile->write(buffer, len);
            }
            tempFile->close();
            file = std::move(tempFile);
            control->setMedia(QUrl(QUrl::fromLocalFile(file->fileName())), nullptr);
#else
            qWarning("Qt was built with -no-feature-temporaryfile: playback from resource file is not supported!");
#endif
        }
    } else {
        qrcMedia = QUrl();
        const QUrl url = qMediaFromUserInput(media);
        if (url.scheme() == QLatin1String("content") && !stream) {
            file = std::make_unique<QFile>(media.url());
            if (!file->open(QIODevice::ReadOnly)) {
                setErrorFn(
                    MediaPlayer::InvalidMedia,
                    MediaPlayer::ResourceError,
                    QLatin1String("Could not open content:// URI: ") + file->errorString());
                qrcFile.swap(file);
                return;
            }
            stream = file.get();
        }

        control->setMedia(url, stream);
    }

    qrcFile.swap(file);
}

QList<MediaMetaData> MediaPlayerPrivate::trackMetaData(PlatformMediaPlayer::TrackType s) const
{
    QList<MediaMetaData> tracks;
    if (control) {
        const int count = control->trackCount(s);
        for (int i = 0; i < count; ++i) {
            tracks.append(control->trackMetaData(s, i));
        }
    }
    return tracks;
}

QList<TrackInfo> MediaPlayerPrivate::trackInfoList(PlatformMediaPlayer::TrackType s) const
{
    QList<TrackInfo> result;
    if (control) {
        const int count = control->trackCount(s);
        const int active = control->activeTrack(s);
        const auto trackType = static_cast<TrackInfo::TrackType>(s);
        for (int i = 0; i < count; ++i) {
            result.append(TrackInfo(i, trackType, control->trackMetaData(s, i), i == active));
        }
    }
    return result;
}

void MediaPlayerPrivate::updateTrackCache()
{
    m_audioTracks->setTracks(trackInfoList(PlatformMediaPlayer::AudioStream));
    m_videoTracks->setTracks(trackInfoList(PlatformMediaPlayer::VideoStream));
    m_subtitleTracks->setTracks(trackInfoList(PlatformMediaPlayer::SubtitleStream));
    m_chapters = control ? control->chapters() : QList<ChapterInfo>{};
}

MediaPlayer::MediaPlayer(QObject *parent)
    : QObject(*new MediaPlayerPrivate, parent)
{
    Q_D(MediaPlayer);

    // 初始化轨道模型
    d->m_audioTracks = new TrackInfoModel(this);
    d->m_videoTracks = new TrackInfoModel(this);
    d->m_subtitleTracks = new TrackInfoModel(this);

    auto maybeControl = PlatformMediaIntegration::instance()->createPlayer(this);
    if (maybeControl) {
        d->control = maybeControl.value();
        d->state = d->control->state();
    } else {
        d->setError(MediaPlayer::ResourceError, maybeControl.error());
    }

    connect(this, &MediaPlayer::activeDecoderChanged, this, [d](PlaybackOptions::VideoDecoderPolicy policy) {
        d->m_activeDecoder = policy;
    });

    // 轨道/章节缓存更新已在 PlatformMediaPlayer::tracksChanged() 中完成

#ifdef Q_OS_WINDOWS
    if (auto *psi = PlatformMediaIntegration::instance()->platformSpecificInterface()) {
        auto *winInterface = static_cast<WindowsPlatformSpecificInterface *>(psi);
        winInterface->smtcManager()->setMediaPlayer(this);
    }
#endif
}

MediaPlayer::~MediaPlayer()
{
    Q_D(MediaPlayer);

    QSignalBlocker blocker(this);

    if (d->control)
        d->control->qmediaplayerDestructorCalled = true;
    setAudioOutput(nullptr);

    d->setVideoSink(nullptr);
    delete d->control;
}

QUrl MediaPlayer::source() const
{
    Q_D(const MediaPlayer);

    return d->source;
}

const QIODevice *MediaPlayer::sourceDevice() const
{
    Q_D(const MediaPlayer);

    return d->stream;
}

MediaPlayer::PlaybackState MediaPlayer::playbackState() const
{
    Q_D(const MediaPlayer);

    if (d->control
        && d->control->mediaStatus() == MediaPlayer::EndOfMedia
        && d->state != d->control->state()) {
        return d->control->state();
    }

    return d->state;
}

MediaPlayer::MediaStatus MediaPlayer::mediaStatus() const
{
    Q_D(const MediaPlayer);
    return d->control ? d->control->mediaStatus() : NoMedia;
}

qint64 MediaPlayer::duration() const
{
    Q_D(const MediaPlayer);
    return d->control ? d->control->duration() : 0;
}

qint64 MediaPlayer::position() const
{
    Q_D(const MediaPlayer);
    return d->control ? d->control->position() : 0;
}

float MediaPlayer::bufferProgress() const
{
    Q_D(const MediaPlayer);
    return d->control ? d->control->bufferProgress() : 0;
}

bool MediaPlayer::isBuffering() const
{
    const auto status = mediaStatus();
    return status == LoadingMedia || status == BufferingMedia || status == StalledMedia;
}

MediaTimeRange MediaPlayer::bufferedTimeRange() const
{
    Q_D(const MediaPlayer);
    return d->control ? d->control->availablePlaybackRanges() : MediaTimeRange{};
}

bool MediaPlayer::hasAudio() const
{
    Q_D(const MediaPlayer);
    return d->control && d->control->isAudioAvailable();
}

bool MediaPlayer::hasVideo() const
{
    Q_D(const MediaPlayer);
    return d->control && d->control->isVideoAvailable();
}

bool MediaPlayer::isSeekable() const
{
    Q_D(const MediaPlayer);
    return d->control && d->control->isSeekable();
}

bool MediaPlayer::isPlaying() const
{
    Q_D(const MediaPlayer);
    return d->state == MediaPlayer::PlayingState;
}

qreal MediaPlayer::playbackRate() const
{
    Q_D(const MediaPlayer);
    return d->control ? d->control->playbackRate() : 0.;
}

int MediaPlayer::loops() const
{
    Q_D(const MediaPlayer);
    return d->control ? d->control->loops() : 1;
}

void MediaPlayer::setLoops(int loops)
{
    Q_D(MediaPlayer);
    if (loops == 0)
        return;
    if (d->control)
        d->control->setLoops(loops);
}

MediaPlayer::Error MediaPlayer::error() const
{
    return d_func()->error.code();
}

QString MediaPlayer::errorString() const
{
    return d_func()->error.description();
}

void MediaPlayer::play()
{
    Q_D(MediaPlayer);

    if (!d->control) {
        return;
    }

    d->control->play();
}

void MediaPlayer::pause()
{
    Q_D(MediaPlayer);

    if (d->control)
        d->control->pause();
}

void MediaPlayer::stop()
{
    Q_D(MediaPlayer);

    if (d->control)
        d->control->stop();
}

void MediaPlayer::setPosition(qint64 position)
{
    Q_D(MediaPlayer);

    if (!d->control)
        return;
    if (!d->control->isSeekable())
        return;
    d->control->setPosition(qMax(position, 0ll));
}

void MediaPlayer::setPlaybackRate(qreal rate)
{
    Q_D(MediaPlayer);

    if (d->control)
        d->control->setPlaybackRate(rate);
}

void MediaPlayer::setSource(const QUrl &source)
{
    Q_D(MediaPlayer);
    stop();

    if (d->source == source && d->stream == nullptr)
        return;

    d->source = source;
    d->stream = nullptr;

    // 同步播放列表索引
    if (!d->playlistUrls.isEmpty()) {
        int idx = d->playlistUrls.indexOf(source);
        if (idx >= 0 && idx != d->playlistCurrentIndex) {
            d->playlistCurrentIndex = idx;
            if (d->m_playbackMode == RandomPlayback)
                d->shufflePosition = d->shuffleOrder.indexOf(idx);
            emit playlistIndexChanged(idx);
        }
    }

    d->setMedia(source, nullptr);
    emit sourceChanged(d->source);
}

void MediaPlayer::setSourceDevice(QIODevice *device, const QUrl &sourceUrl)
{
    Q_D(MediaPlayer);
    stop();

    if (d->source == sourceUrl && d->stream == device)
        return;

    d->source = sourceUrl;
    d->stream = device;

    d->setMedia(d->source, device);
    emit sourceChanged(d->source);
}

void MediaPlayer::setAudioBufferOutput(AudioBufferOutput *output)
{
    Q_D(MediaPlayer);

    AudioBufferOutput *oldOutput = d->audioBufferOutput;
    if (oldOutput == output)
        return;

    d->audioBufferOutput = output;

    if (oldOutput) {
        auto oldPlayer = AudioBufferOutputPrivate::exchangeMediaPlayer(*oldOutput, this);
        if (oldPlayer)
            oldPlayer->setAudioBufferOutput(nullptr);
    }

    if (d->control)
        d->control->setAudioBufferOutput(output);

    emit audioBufferOutputChanged();
}

AudioBufferOutput *MediaPlayer::audioBufferOutput() const
{
    Q_D(const MediaPlayer);
    return d->audioBufferOutput;
}

void MediaPlayer::setAudioOutput(AudioOutput *output)
{
    Q_D(MediaPlayer);
    auto oldOutput = d->audioOutput;
    if (oldOutput == output)
        return;
    d->audioOutput = output;
    if (d->control)
        d->control->setAudioOutput(nullptr);
    if (oldOutput)
        oldOutput->setDisconnectFunction({});
    if (output) {
        output->setDisconnectFunction([this](){ setAudioOutput(nullptr); });
        if (d->control)
            d->control->setAudioOutput(output->handle());
    }
    emit audioOutputChanged();
}

AudioOutput *MediaPlayer::audioOutput() const
{
    Q_D(const MediaPlayer);
    return d->audioOutput;
}

TrackInfoModel *MediaPlayer::audioTracks() const
{
    Q_D(const MediaPlayer);
    return d->m_audioTracks;
}

TrackInfoModel *MediaPlayer::videoTracks() const
{
    Q_D(const MediaPlayer);
    return d->m_videoTracks;
}

TrackInfoModel *MediaPlayer::subtitleTracks() const
{
    Q_D(const MediaPlayer);
    return d->m_subtitleTracks;
}

void MediaPlayer::activateTrack(TrackInfo::TrackType type, int index)
{
    switch (type) {
    case TrackInfo::AudioTrack:
        setActiveAudioTrack(index);
        break;
    case TrackInfo::VideoTrack:
        setActiveVideoTrack(index);
        break;
    case TrackInfo::SubtitleTrack:
        setActiveSubtitleTrack(index);
        break;
    }
}

QList<ChapterInfo> MediaPlayer::chapters() const
{
    Q_D(const MediaPlayer);
    return d->m_chapters;
}

int MediaPlayer::activeAudioTrack() const
{
    Q_D(const MediaPlayer);
    return d->control ? d->control->activeTrack(PlatformMediaPlayer::AudioStream) : 0;
}

int MediaPlayer::activeVideoTrack() const
{
    Q_D(const MediaPlayer);
    return d->control ? d->control->activeTrack(PlatformMediaPlayer::VideoStream) : -1;
}

int MediaPlayer::activeSubtitleTrack() const
{
    Q_D(const MediaPlayer);
    return d->control ? d->control->activeTrack(PlatformMediaPlayer::SubtitleStream) : -1;
}

void MediaPlayer::setActiveAudioTrack(int index)
{
    Q_D(MediaPlayer);
    if (!d->control)
        return;

    if (activeAudioTrack() == index)
        return;
    d->control->setActiveTrack(PlatformMediaPlayer::AudioStream, index);
    emit activeAudioTrackChanged();
}

void MediaPlayer::setActiveVideoTrack(int index)
{
    Q_D(MediaPlayer);
    if (!d->control)
        return;

    if (activeVideoTrack() == index)
        return;
    d->control->setActiveTrack(PlatformMediaPlayer::VideoStream, index);
    emit activeVideoTrackChanged();
}

void MediaPlayer::setActiveSubtitleTrack(int index)
{
    Q_D(MediaPlayer);
    if (!d->control)
        return;

    if (activeSubtitleTrack() == index)
        return;
    d->control->setActiveTrack(PlatformMediaPlayer::SubtitleStream, index);
    emit activeSubtitleTrackChanged();
}

SubtitleStyle MediaPlayer::subtitleStyle() const
{
    Q_D(const MediaPlayer);
    if (d->videoSink)
        return d->videoSink->subtitleStyle();
    return {};
}

void MediaPlayer::setSubtitleStyle(const SubtitleStyle &style)
{
    Q_D(MediaPlayer);
    if (d->control)
        d->control->setSubtitleStyle(style);
    if (d->videoSink)
        d->videoSink->setSubtitleStyle(style);
    emit subtitleStyleChanged();
}

void MediaPlayer::applySubtitleStyle(const QVariantMap &props)
{
    SubtitleStyle style = subtitleStyle();
    if (props.contains(QStringLiteral("fontFamily")))
        style.setFontFamily(props.value(QStringLiteral("fontFamily")).toString());
    if (props.contains(QStringLiteral("fontSize")))
        style.setFontSize(props.value(QStringLiteral("fontSize")).toReal());
    if (props.contains(QStringLiteral("fontColor")))
        style.setFontColor(props.value(QStringLiteral("fontColor")).value<QColor>());
    if (props.contains(QStringLiteral("bold")))
        style.setBold(props.value(QStringLiteral("bold")).toBool());
    if (props.contains(QStringLiteral("italic")))
        style.setItalic(props.value(QStringLiteral("italic")).toBool());
    if (props.contains(QStringLiteral("backgroundColor")))
        style.setBackgroundColor(props.value(QStringLiteral("backgroundColor")).value<QColor>());
    if (props.contains(QStringLiteral("backgroundOpacity")))
        style.setBackgroundOpacity(props.value(QStringLiteral("backgroundOpacity")).toReal());
    if (props.contains(QStringLiteral("cornerRadius")))
        style.setCornerRadius(props.value(QStringLiteral("cornerRadius")).toReal());
    if (props.contains(QStringLiteral("topMargin")))
        style.setTopMargin(props.value(QStringLiteral("topMargin")).toReal());
    if (props.contains(QStringLiteral("bottomMargin")))
        style.setBottomMargin(props.value(QStringLiteral("bottomMargin")).toReal());
    if (props.contains(QStringLiteral("leftMargin")))
        style.setLeftMargin(props.value(QStringLiteral("leftMargin")).toReal());
    if (props.contains(QStringLiteral("rightMargin")))
        style.setRightMargin(props.value(QStringLiteral("rightMargin")).toReal());
    setSubtitleStyle(style);
}

QObject *MediaPlayer::videoOutput() const
{
    Q_D(const MediaPlayer);
    return d->videoOutput;
}

void MediaPlayer::setVideoOutput(QObject *output)
{
    Q_D(MediaPlayer);
    if (d->videoOutput == output)
        return;

    auto *sink = qobject_cast<VideoSink *>(output);
    if (!sink && output) {
        auto *mo = output->metaObject();
        mo->invokeMethod(output, "videoSink", Q_RETURN_ARG(VideoSink *, sink));
    }
    d->videoOutput = output;
    d->setVideoSink(sink);
}

void MediaPlayer::setVideoSink(VideoSink *sink)
{
    Q_D(MediaPlayer);
    d->videoOutput = nullptr;
    d->setVideoSink(sink);
}

VideoSink *MediaPlayer::videoSink() const
{
    Q_D(const MediaPlayer);
    return d->videoSink;
}

#if 0

void MediaPlayer::setVideoOutput(const QList<VideoSink *> &sinks)
{

    Q_UNUSED(sinks);

}
#endif

bool MediaPlayer::isAvailable() const
{
    Q_D(const MediaPlayer);
    return bool(d->control);
}

MediaMetaData MediaPlayer::metaData() const
{
    Q_D(const MediaPlayer);
    return d->control ? d->control->metaData() : MediaMetaData{};
}

bool MediaPlayer::pitchCompensation() const
{
    Q_D(const MediaPlayer);
    return d->control ? d->control->pitchCompensation() : false;
}

void MediaPlayer::setPitchCompensation(bool enabled) const
{
    Q_D(const MediaPlayer);
    if (d->control)
        d->control->setPitchCompensation(enabled);
}

MediaPlayer::PitchCompensationAvailability MediaPlayer::pitchCompensationAvailability() const
{
    Q_D(const MediaPlayer);
    return d->control->pitchCompensationAvailability();
}

void MediaPlayer::setPlaybackOptions(const PlaybackOptions &options)
{
    Q_D(MediaPlayer);
    if (std::exchange(d->playbackOptions, options) != options) {
        if (d->control)
            d->control->setPlaybackOptions(options);
        if (d->videoSink)
            d->videoSink->setHdrPolicy(options.hdrPolicy());
        emit playbackOptionsChanged();
    }
}

QVector<PlaybackOptions::VideoDecoderPolicy> MediaPlayer::videoDecoderPriority() const
{
    Q_D(const MediaPlayer);
    return d->playbackOptions.videoDecoderPriority();
}

void MediaPlayer::prioritizeDecoder(PlaybackOptions::VideoDecoderPolicy policy)
{
    Q_D(MediaPlayer);
    d->playbackOptions.prioritizeDecoder(policy);
    if (d->control)
        d->control->setPlaybackOptions(d->playbackOptions);
}

void MediaPlayer::deprioritizeDecoder(PlaybackOptions::VideoDecoderPolicy policy)
{
    Q_D(MediaPlayer);
    d->playbackOptions.deprioritizeDecoder(policy);
    if (d->control)
        d->control->setPlaybackOptions(d->playbackOptions);
}

PlaybackOptions::VideoDecoderPolicy MediaPlayer::activeDecoder() const
{
    Q_D(const MediaPlayer);
    return d->m_activeDecoder;
}

QImage MediaPlayer::getMediaCover(QSize size, bool decodeFrame)
{
    Q_D(MediaPlayer);
    return d->control ? d->control->getMediaCover(size, decodeFrame) : QImage{};
}

void MediaPlayer::cancelCoverRequest()
{
    Q_D(MediaPlayer);
    if (d->control)
        d->control->cancelCoverRequest();
}

// --- 播放列表 ---

QList<QUrl> MediaPlayer::playlist() const
{
    return d_func()->playlistUrls;
}

void MediaPlayer::setPlaylist(const QList<QUrl> &urls)
{
    Q_D(MediaPlayer);

    if (d->playlistUrls == urls)
        return;

    d->playlistUrls = urls;
    d->playlistCurrentIndex = urls.isEmpty() ? -1 : 0;

    // 随机模式下重新生成随机序列
    if (d->m_playbackMode == RandomPlayback && !urls.isEmpty())
        d->regenerateShuffleOrder();

    emit playlistChanged();
    emit playlistIndexChanged(d->playlistCurrentIndex);

    // 自动加载第一首
    if (!urls.isEmpty())
        setSource(urls.first());
}

int MediaPlayer::playlistIndex() const
{
    return d_func()->playlistCurrentIndex;
}

void MediaPlayer::setPlaylistIndex(int index)
{
    Q_D(MediaPlayer);
    if (index < 0 || index >= d->playlistUrls.size() || index == d->playlistCurrentIndex)
        return;

    d->playlistCurrentIndex = index;

    if (d->m_playbackMode == RandomPlayback)
        d->shufflePosition = d->shuffleOrder.indexOf(index);

    setSource(d->playlistUrls.at(index));
    emit playlistIndexChanged(index);
}

int MediaPlayer::playlistCount() const
{
    return d_func()->playlistUrls.size();
}

MediaPlayer::PlaybackMode MediaPlayer::playbackMode() const
{
    return d_func()->m_playbackMode;
}

void MediaPlayer::setPlaybackMode(PlaybackMode mode)
{
    Q_D(MediaPlayer);
    if (d->m_playbackMode == mode)
        return;

    d->m_playbackMode = mode;

    // 切换到随机模式时生成随机序列
    if (mode == RandomPlayback && !d->playlistUrls.isEmpty())
        d->regenerateShuffleOrder();

    emit playbackModeChanged(mode);
}

void MediaPlayer::addToPlaylist(const QUrl &url)
{
    Q_D(MediaPlayer);
    d->playlistUrls.append(url);
    emit playlistChanged();
}

void MediaPlayer::addToPlaylist(const QList<QUrl> &urls)
{
    Q_D(MediaPlayer);
    if (urls.isEmpty())
        return;
    d->playlistUrls.append(urls);
    emit playlistChanged();
}

void MediaPlayer::clearPlaylist()
{
    Q_D(MediaPlayer);
    d->playlistUrls.clear();
    d->playlistCurrentIndex = -1;
    d->shuffleOrder.clear();
    d->shufflePosition = -1;
    emit playlistChanged();
    emit playlistIndexChanged(-1);
}

void MediaPlayer::next()
{
    Q_D(MediaPlayer);
    if (d->playlistUrls.isEmpty())
        return;

    int nextIdx = d->nextPlaylistIndex();
    if (nextIdx >= 0) {
        setPlaylistIndex(nextIdx);
        play();
    }
}

void MediaPlayer::previous()
{
    Q_D(MediaPlayer);
    if (d->playlistUrls.isEmpty())
        return;

    const int prevIdx = d->previousPlaylistIndex();
    if (prevIdx >= 0) {
        setPlaylistIndex(prevIdx);
        play();
    }
}

void MediaPlayer::shufflePlaylist()
{
    Q_D(MediaPlayer);
    if (d->playlistUrls.isEmpty())
        return;
    d->regenerateShuffleOrder();
    d->m_playbackMode = RandomPlayback;
    emit playbackModeChanged(RandomPlayback);
}

qint64 MediaPlayer::opening() const
{
    Q_D(const MediaPlayer);
    return d->control ? d->control->opening() : 0;
}

qint64 MediaPlayer::ending() const
{
    Q_D(const MediaPlayer);
    return d->control ? d->control->ending() : 0;
}

void MediaPlayer::setOpening(qint64 ms)
{
    Q_D(MediaPlayer);
    if (d->control)
        d->control->setOpening(ms);
}

void MediaPlayer::setEnding(qint64 ms)
{
    Q_D(MediaPlayer);
    if (d->control)
        d->control->setEnding(ms);
}

bool MediaPlayer::hdrEnabled() const
{
    Q_D(const MediaPlayer);
    return d->playbackOptions.hdrPolicy() == PlaybackOptions::HdrPolicy::Enabled;
}

void MediaPlayer::setHdrEnabled(bool enabled)
{
    Q_D(MediaPlayer);
    auto policy = enabled ? PlaybackOptions::HdrPolicy::Enabled : PlaybackOptions::HdrPolicy::Disabled;
    if (d->playbackOptions.hdrPolicy() == policy)
        return;
    d->playbackOptions.setHdrPolicy(policy);
    if (d->videoSink)
        d->videoSink->setHdrPolicy(policy);
    if (d->control)
        d->control->setPlaybackOptions(d->playbackOptions);
    emit hdrEnabledChanged();
}

bool MediaPlayer::zeroCopyEnabled() const
{
    Q_D(const MediaPlayer);
    return d->playbackOptions.zeroCopy() == PlaybackOptions::ZeroCopy::Enabled;
}

void MediaPlayer::setZeroCopyEnabled(bool enabled)
{
    Q_D(MediaPlayer);
    auto mode = enabled ? PlaybackOptions::ZeroCopy::Enabled : PlaybackOptions::ZeroCopy::Disabled;
    if (d->playbackOptions.zeroCopy() == mode)
        return;
    d->playbackOptions.setZeroCopy(mode);
    if (d->control)
        d->control->setPlaybackOptions(d->playbackOptions);
    emit zeroCopyEnabledChanged();
}

bool MediaPlayer::lowLatencyStreamingEnabled() const
{
    Q_D(const MediaPlayer);
    return d->playbackOptions.playbackIntent() == PlaybackOptions::PlaybackIntent::LowLatencyStreaming;
}

void MediaPlayer::setLowLatencyStreamingEnabled(bool enabled)
{
    Q_D(MediaPlayer);
    auto intent = enabled ? PlaybackOptions::PlaybackIntent::LowLatencyStreaming
                          : PlaybackOptions::PlaybackIntent::Playback;
    if (d->playbackOptions.playbackIntent() == intent)
        return;
    d->playbackOptions.setPlaybackIntent(intent);
    if (d->control)
        d->control->setPlaybackOptions(d->playbackOptions);
    emit lowLatencyStreamingEnabledChanged();
}

#include "moc_MediaPlayer.cpp"
