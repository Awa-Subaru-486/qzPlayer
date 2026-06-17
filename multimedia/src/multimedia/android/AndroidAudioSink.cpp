// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "AndroidAudioSink_p.h"

#include "AndroidAudioUtil_p.h"

#include <qzMultimedia/private/AudioHelpers_p.h>
#include <qzMultimedia/private/AudioDeviceMonitor_p.h>

import qzLog;
#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>

QT_BEGIN_NAMESPACE

namespace QtAAudio {

qz::Log::LogCategory qLcAndroidAudioSink("qz.multimedia.android.audiosink");

AndroidAudioSinkStream::AndroidAudioSinkStream(AudioDevice device, const AudioFormat &format,
                                                 std::optional<qsizetype> ringbufferSize,
                                                 AndroidAudioSink *parent, float volume,
                                                 std::optional<int32_t> hardwareBufferFrames,
                                                 AudioEndpointRole role)
    : PlatformAudioSinkStream(std::move(device), format,
                               std::optional<int>(ringbufferSize),
                               hardwareBufferFrames, volume),
      m_parent(parent),
      m_role(role)
{
    QtAAudio::StreamBuilder builder(format);

    qz::Log::cat_debug(qLcAndroidAudioSink, "Creating sink for device id:{} , description:{}", m_audioDevice.id().toStdString(), m_audioDevice.description().toStdString());

    // NOTE: Don't set device when creating a stream for the default bluetooth device
    if (!AndroidAudioUtil::isDefaultBluetoothDevice(m_audioDevice))
        builder.deviceId = m_audioDevice.id().toInt();

    // Set buffer parameters
    builder.bufferCapacity = m_hardwareBufferFrames
            ? *m_hardwareBufferFrames
            : 1024;

    // NOTE: AAudio doesn't support UINT8, so convert to INT16 if that's requested
    if (format.sampleFormat() == AudioFormat::UInt8) {
        m_hostFormat = format;
        m_hostFormat->setSampleFormat(AudioFormat::Int16);
    }

    // Set builder parameters for audio sink
    builder.params.sharingMode = AAUDIO_SHARING_MODE_SHARED;
    builder.params.direction = AAUDIO_DIRECTION_OUTPUT;
    switch (m_role) {
    case QtMultimediaPrivate::AudioEndpointRole::SoundEffect:
        builder.params.outputUsage = AAUDIO_USAGE_GAME;
        builder.params.outputContentType = AAUDIO_CONTENT_TYPE_SONIFICATION;
        break;
    case QtMultimediaPrivate::AudioEndpointRole::Accessibility:
        builder.params.outputUsage = AAUDIO_USAGE_ASSISTANCE_ACCESSIBILITY;
        builder.params.outputContentType = AAUDIO_CONTENT_TYPE_SPEECH;
        break;
    case QtMultimediaPrivate::AudioEndpointRole::MediaPlayback:
    case QtMultimediaPrivate::AudioEndpointRole::Other:
        builder.params.outputUsage = AAUDIO_USAGE_MEDIA;
        builder.params.outputContentType = AAUDIO_CONTENT_TYPE_MUSIC;
        break;
    }

    builder.userData = this;
    builder.callback = [](AAudioStream *, void *userData, void *audioData,
                          int32_t numFrames) -> int {
        auto *stream = reinterpret_cast<AndroidAudioSinkStream *>(userData);
        Q_ASSERT(stream);
        QSpan<std::byte> audioSpan = stream->getHostSpan(audioData, numFrames);
        return stream->m_audioCallback ? stream->processCallback(audioSpan)
                                       : stream->processRingbuffer(audioSpan, numFrames);
    };
    builder.errorCallback = [](AAudioStream *, void *userData, aaudio_result_t error) -> void {
        auto *stream = reinterpret_cast<AndroidAudioSinkStream *>(userData);
        Q_ASSERT(stream);
        stream->handleError(error);
    };

    builder.setupBuilder();
    m_stream = std::make_unique<QtAAudio::Stream>(builder);
    if (builder.format.sampleFormat() != format.sampleFormat()) {
        // Original sample format unsupported, so doing sample format conversion
        Q_ASSERT(builder.format.sampleFormat() == AudioFormat::Float);
        m_hostFormat = builder.format;
    }
}

bool AndroidAudioSinkStream::open()
{
    if (!m_stream->isOpen()) {
        qz::Log::cat_warn(qLcAndroidAudioSink, "Stream null");
        requestStop();
        return false;
    }

    if (!m_stream->areStreamParametersRespected())
        qz::Log::cat_warn(qLcAndroidAudioSink, "Stream parameters not correct");

    return true;
}

bool AndroidAudioSinkStream::start(QIODevice *device)
{
    Q_ASSERT(thread()->isCurrentThread());
    setQIODevice(device);
    pullFromQIODevice();
    createQIODeviceConnections(device);

    // TODO: Fill host ringbuffer before starting
    if (!m_stream->start()) {
        requestStop();
        return false;
    }

    return true;
}

QIODevice *AndroidAudioSinkStream::start()
{
    auto *writer = createRingbufferWriterDevice();

    m_parent->updateStreamIdle(true, AndroidAudioSink::EmitStateSignal::False);

    setQIODevice(writer);
    createQIODeviceConnections(writer);

    if (!m_stream->start()) {
        requestStop();
        return nullptr;
    }

    return writer;
}

bool AndroidAudioSinkStream::start(AudioCallback cb)
{
    Q_ASSERT(thread()->isCurrentThread());
    m_audioCallback = std::move(cb);

    if (!m_stream->start()) {
        requestStop();
        return false;
    }

    return true;
}

void AndroidAudioSinkStream::suspend()
{
    Q_ASSERT(thread()->isCurrentThread());
    m_stream->stop();
}

void AndroidAudioSinkStream::resume()
{
    Q_ASSERT(thread()->isCurrentThread());
    m_stream->start();
}

void AndroidAudioSinkStream::stop(ShutdownPolicy policy)
{
    requestStop();
    disconnectQIODeviceConnections();

    switch (policy) {
    case ShutdownPolicy::DrainRingbuffer:
        stop();
        break;
    case ShutdownPolicy::DiscardRingbuffer:
        reset();
        break;
    default:
        Q_UNREACHABLE_RETURN();
    }
}

void AndroidAudioSinkStream::stop()
{
    if (isIdle() || m_audioCallback)
        return reset();

    stopIdleDetection();
    connectIdleHandler([this] {
        Q_ASSERT(thread()->isCurrentThread());
        if (!isIdle()) // Only handle <not idle> -> <idle> transitions
            return;

        // We have written everything we want to write, synchronous stop on application thread
        m_stream->stop();
        m_self = nullptr; // might delete the instance
    });

    m_parent = nullptr;

    // Take ownership of self to avoid deletion until AAudio stream is stopped
    m_self = shared_from_this();
}

void AndroidAudioSinkStream::reset()
{
    Q_ASSERT(thread()->isCurrentThread());
    m_stream->stop();
}

void AndroidAudioSinkStream::updateStreamIdle(bool arg)
{
    if (m_parent)
        m_parent->updateStreamIdle(arg);
}

QSpan<std::byte>
AndroidAudioSinkStream::getHostSpan(void *audioData,
                                     int numFrames) const noexcept QT_MM_NONBLOCKING
{
    qsizetype byteAmount = m_hostFormat ? m_hostFormat->bytesForFrames(numFrames)
                                        : m_format.bytesForFrames(numFrames);
    return QSpan{ reinterpret_cast<std::byte *>(audioData), byteAmount };
}

aaudio_data_callback_result_t
AndroidAudioSinkStream::processRingbuffer(QSpan<std::byte> audioSpan,
                                           int numFrames) noexcept QT_MM_NONBLOCKING
{
    auto consumedFrames = m_hostFormat
            ? PlatformAudioSinkStream::process(
                      audioSpan, numFrames,
                      AudioHelperInternal::toNativeSampleFormat(m_hostFormat->sampleFormat()))
            : PlatformAudioSinkStream::process(audioSpan, numFrames);
    if (consumedFrames != static_cast<uint64_t>(numFrames) && isStopRequested())
        return AAUDIO_CALLBACK_RESULT_STOP;

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

aaudio_data_callback_result_t
AndroidAudioSinkStream::processCallback(QSpan<std::byte> audioSpan) noexcept QT_MM_NONBLOCKING
{
    if (isStopRequested())
        return AAUDIO_CALLBACK_RESULT_STOP;

    if (m_hostFormat) {
        // When host format differs from the user format, we need to:
        // 1) Run the callback with a buffer in the user format
        // 2) Convert from user format to host format
        // 3) Apply volume
        const AudioFormat &hostFormat = *m_hostFormat;
        const qsizetype numFrames = hostFormat.framesForBytes(audioSpan.size());
        const qsizetype userBufferSize = m_format.bytesForFrames(numFrames);

        // Allocate temporary buffer for user-format data
        QtPrivate::ScopedRTSanDisabler allowAllocations;
        auto userBuffer = q20::make_unique_for_overwrite<std::byte[]>(userBufferSize);
        std::span<std::byte> userSpan{ userBuffer.get(), static_cast<size_t>(userBufferSize) };

        // Run callback with user format
        QtMultimediaPrivate::runAudioCallback(*m_audioCallback, userSpan, m_format, volume());

        // Convert from user format to host format
        AudioHelperInternal::convertSampleFormat(
                userSpan,
                AudioHelperInternal::toNativeSampleFormat(m_format.sampleFormat()),
                audioSpan,
                AudioHelperInternal::toNativeSampleFormat(hostFormat.sampleFormat()));
    } else {
        QtMultimediaPrivate::runAudioCallback(*m_audioCallback, audioSpan, m_format, volume());
    }

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AndroidAudioSinkStream::handleError(aaudio_result_t error)
{
    if (error == AAUDIO_ERROR_DISCONNECTED) {
        // Device disconnected: stop the stream, notify AudioDeviceMonitor
        // to pause the MediaPlayer, and also rebuild the stream on the new
        // default device so it's ready when playback resumes.
        requestStop();
        auto self = shared_from_this();
        invokeOnAppThread([self] {
            self->handleIOError(self->m_parent);
        });
        AudioDeviceMonitor::notifyOutputDeviceChanged();
    } else {
        requestStop();
        auto self = shared_from_this();
        invokeOnAppThread([self] {
            self->handleIOError(self->m_parent);
        });
    }
}

AndroidAudioSink::AndroidAudioSink(AudioDevice device, const AudioFormat &fmt, QObject *parent)
    : BaseClass(std::move(device), fmt, parent)
{
}

AndroidAudioSink::~AndroidAudioSink()
    = default;

} // namespace QtAAudio

QT_END_NAMESPACE
