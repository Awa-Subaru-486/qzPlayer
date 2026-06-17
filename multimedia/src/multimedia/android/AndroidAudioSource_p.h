// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_ANDROID_ANDROIDAUDIOSOURCE_P_H
#define QT_ANDROID_ANDROIDAUDIOSOURCE_P_H

#include <qzMultimedia/private/AudioPlatformImplementationSupport_p.h>
#include "AAudioStream_p.h"

#include <memory>

QT_BEGIN_NAMESPACE

namespace QtAAudio {

class AndroidAudioSource;

class AndroidAudioSourceStream final : public QtMultimediaPrivate::PlatformAudioSourceStream
{
    using PlatformAudioSourceStreamBase = QtMultimediaPrivate::PlatformAudioSourceStream;

public:
    using SourceType = AndroidAudioSource;

    explicit AndroidAudioSourceStream(AudioDevice device, const AudioFormat &format,
                                       std::optional<qsizetype> ringbufferSize,
                                       AndroidAudioSource *parent, float volume,
                                       std::optional<int32_t> hardwareBufferFrames);
    Q_DISABLE_COPY_MOVE(AndroidAudioSourceStream)
    ~AndroidAudioSourceStream();

    bool open();

    bool start(QIODevice *);
    QIODevice *start();
    bool start(AudioCallback &&);

    void suspend();
    void resume();
    void stop(ShutdownPolicy);

    using PlatformAudioSourceStreamBase::bytesReady;
    using PlatformAudioSourceStreamBase::deviceIsRingbufferReader;
    using PlatformAudioSourceStreamBase::processedDuration;
    using PlatformAudioSourceStreamBase::ringbufferSizeInBytes;
    using PlatformAudioSourceStreamBase::setVolume;

private:
    // PlatformAudioSourceStream overrides
    void updateStreamIdle(bool idle) override;

    QSpan<const std::byte> getHostSpan(void *audioData, int numFrames) const noexcept QT_MM_NONBLOCKING;
    aaudio_data_callback_result_t processRingbuffer(QSpan<const std::byte> audioSpan,
                                                    int numFrames) noexcept QT_MM_NONBLOCKING;
    aaudio_data_callback_result_t
    processCallback(QSpan<const std::byte> audioSpan) noexcept QT_MM_NONBLOCKING;
    void handleError(aaudio_result_t error);

    AndroidAudioSource *m_parent;

    std::optional<AudioCallback> m_audioCallback;

    std::unique_ptr<QtAAudio::Stream> m_stream;

    std::optional<AudioFormat> m_hostFormat;
};

class AndroidAudioSource final
    : public QtMultimediaPrivate::PlatformAudioSourceImplementationWithCallback<
              AndroidAudioSourceStream, AndroidAudioSource>
{
    using BaseClass = QtMultimediaPrivate::PlatformAudioSourceImplementationWithCallback<
            AndroidAudioSourceStream, AndroidAudioSource>;

public:
    AndroidAudioSource(AudioDevice device, const AudioFormat &format, QObject *parent);
    ~AndroidAudioSource() override;
};

} // namespace QtAAudio

QT_END_NAMESPACE

#endif // QT_ANDROID_ANDROIDAUDIOSOURCE_P_H
