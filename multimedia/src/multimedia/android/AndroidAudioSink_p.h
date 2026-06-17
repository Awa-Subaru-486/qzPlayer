// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_ANDROID_ANDROIDAUDIOSINK_P_H
#define QT_ANDROID_ANDROIDAUDIOSINK_P_H

#include <qzMultimedia/private/AudioPlatformImplementationSupport_p.h>
#include "AAudioStream_p.h"

#include <memory>

QT_BEGIN_NAMESPACE

namespace QtAAudio {

class AndroidAudioSink;

class AndroidAudioSinkStream final
    : public std::enable_shared_from_this<AndroidAudioSinkStream>
    , public QtMultimediaPrivate::PlatformAudioSinkStream
{
    using PlatformAudioSinkStream = QtMultimediaPrivate::PlatformAudioSinkStream;
    using AudioEndpointRole = QtMultimediaPrivate::AudioEndpointRole;

public:
    using SinkType = AndroidAudioSink;

    explicit AndroidAudioSinkStream(AudioDevice, const AudioFormat &,
                                    std::optional<qsizetype> ringbufferSize,
                                    AndroidAudioSink *parent, float volume,
                                    std::optional<int32_t> hardwareBufferFrames,
                                    AudioEndpointRole);
    Q_DISABLE_COPY_MOVE(AndroidAudioSinkStream)

    bool open();

    bool start(QIODevice *device);
    QIODevice *start();
    bool start(AudioCallback cb);

    void suspend();
    void resume();
    void stop(ShutdownPolicy policy);

    using PlatformAudioSinkStream::bytesFree;
    using PlatformAudioSinkStream::processedDuration;
    using PlatformAudioSinkStream::ringbufferSizeInBytes;
    using PlatformAudioSinkStream::setVolume;

private:
    void stop();
    void reset();

    // PlatformAudioSinkStream overrides
    void updateStreamIdle(bool arg) override;

    QSpan<std::byte> getHostSpan(void *audioData, int numFrames) const noexcept QT_MM_NONBLOCKING;
    aaudio_data_callback_result_t processRingbuffer(QSpan<std::byte> audioSpan,
                                                    int numFrames) noexcept QT_MM_NONBLOCKING;
    aaudio_data_callback_result_t processCallback(QSpan<std::byte> audioSpan) noexcept QT_MM_NONBLOCKING;
    void handleError(aaudio_result_t error);

    AndroidAudioSink *m_parent{ nullptr };
    std::shared_ptr<AndroidAudioSinkStream> m_self;
    std::optional<AudioCallback> m_audioCallback;
    AudioEndpointRole m_role;
    std::unique_ptr<QtAAudio::Stream> m_stream;
    std::optional<AudioFormat> m_hostFormat;
};

class AndroidAudioSink final
    : public QtMultimediaPrivate::PlatformAudioSinkImplementation<AndroidAudioSinkStream,
                                                                   AndroidAudioSink>
{
    using BaseClass = QtMultimediaPrivate::PlatformAudioSinkImplementation<AndroidAudioSinkStream,
                                                                            AndroidAudioSink>;

public:
    AndroidAudioSink(AudioDevice device, const AudioFormat &format, QObject *parent);
    ~AndroidAudioSink() override;
};

} // namespace QtAAudio

QT_END_NAMESPACE

#endif // QT_ANDROID_ANDROIDAUDIOSINK_P_H
