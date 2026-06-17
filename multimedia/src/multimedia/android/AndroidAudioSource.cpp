// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "AndroidAudioSource_p.h"

#include "AndroidAudioJniTypes_p.h"
#include "AndroidAudioUtil_p.h"

#include <qzMultimedia/private/AudioHelpers_p.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qpermissions.h>
import qzLog;

QT_BEGIN_NAMESPACE

namespace QtAAudio {

qz::Log::LogCategory qLcAndroidAudioSource("qz.multimedia.android.audiosource");

AndroidAudioSourceStream::AndroidAudioSourceStream(AudioDevice device,
                                                     const AudioFormat &format,
                                                     std::optional<qsizetype> ringbufferSize,
                                                     AndroidAudioSource *parent, float volume,
                                                     std::optional<int32_t> hardwareBufferFrames)
    : PlatformAudioSourceStreamBase(std::move(device), format,
                                     std::optional<int>(ringbufferSize),
                                     hardwareBufferFrames, volume),
      m_parent(parent)
{
    QtAAudio::StreamBuilder builder(format);

    qz::Log::cat_debug(qLcAndroidAudioSource, "Creating source for device id:{} , description:{}", m_audioDevice.id().toStdString(), m_audioDevice.description().toStdString());

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

    // Set builder parameters for audio source
    builder.params.sharingMode = AAUDIO_SHARING_MODE_SHARED;
    builder.params.direction = AAUDIO_DIRECTION_INPUT;

    // TODO: Set input preset based on device

    builder.userData = this;
    builder.callback = [](AAudioStream *, void *userData, void *audioData,
                          int32_t numFrames) -> int {
        auto *stream = reinterpret_cast<AndroidAudioSourceStream *>(userData);
        Q_ASSERT(stream);
        auto audioSpan = stream->getHostSpan(audioData, numFrames);
        return stream->m_audioCallback ? stream->processCallback(audioSpan)
                                       : stream->processRingbuffer(audioSpan, numFrames);
    };
    builder.errorCallback = [](AAudioStream *, void *userData, aaudio_result_t error) -> void {
        auto *stream = reinterpret_cast<AndroidAudioSourceStream *>(userData);
        Q_ASSERT(stream);
        stream->handleError(error);
    };

    builder.setupBuilder();

    if (!QtJniTypes::QtAudioDeviceManager::callStaticMethod<jboolean>("prepareAudioInput",
                                                                      m_audioDevice.id().toInt()))
        qz::Log::cat_warn(qLcAndroidAudioSource, "Preparation failed for device:{}", m_audioDevice.id().toInt());

    m_stream = std::make_unique<QtAAudio::Stream>(builder);
    if (builder.format.sampleFormat() != format.sampleFormat()) {
        // Original sample format unsupported, so doing sample format conversion
        Q_ASSERT(builder.format.sampleFormat() == AudioFormat::Float);
        m_hostFormat = builder.format;
    }
}

AndroidAudioSourceStream::~AndroidAudioSourceStream()
{
    QtJniTypes::QtAudioDeviceManager::callStaticMethod<void>("releaseAudioDevice",
                                                             m_audioDevice.id().toInt());
}

bool AndroidAudioSourceStream::open()
{
    QMicrophonePermission permission;

    const bool permitted = qApp->checkPermission(permission) == Qt::PermissionStatus::Granted;
    if (!permitted) {
        qWarning("Missing microphone permission!");
        requestStop();
        return false;
    }

    if (!m_stream->isOpen()) {
        qz::Log::cat_warn(qLcAndroidAudioSource, "Stream null");
        requestStop();
        return false;
    }

    if (!m_stream->areStreamParametersRespected())
        qz::Log::cat_warn(qLcAndroidAudioSource, "Stream parameters not correct");

    return true;
}

bool AndroidAudioSourceStream::start(QIODevice *device)
{
    Q_ASSERT(thread()->isCurrentThread());
    setQIODevice(device);
    createQIODeviceConnections(device);

    if (!m_stream->start()) {
        requestStop();
        return false;
    }

    return true;
}

QIODevice *AndroidAudioSourceStream::start()
{
    auto *device = createRingbufferReaderDevice();

    m_parent->updateStreamIdle(true, AndroidAudioSource::EmitStateSignal::False);

    setQIODevice(device);
    createQIODeviceConnections(device);

    if (!m_stream->start()) {
        requestStop();
        return nullptr;
    }

    return device;
}

bool AndroidAudioSourceStream::start(AudioCallback &&callback)
{
    Q_ASSERT(thread()->isCurrentThread());
    m_audioCallback = std::move(callback);

    if (!m_stream->start()) {
        requestStop();
        return false;
    }

    return true;
}

void AndroidAudioSourceStream::suspend()
{
    Q_ASSERT(thread()->isCurrentThread());
    m_stream->stop();
}

void AndroidAudioSourceStream::resume()
{
    Q_ASSERT(thread()->isCurrentThread());
    m_stream->start();
}

void AndroidAudioSourceStream::stop(ShutdownPolicy policy)
{
    Q_ASSERT(thread()->isCurrentThread());
    requestStop();

    m_stream->stop();

    disconnectQIODeviceConnections();
    finalizeQIODevice(policy);

    if (policy == ShutdownPolicy::DiscardRingbuffer)
        emptyRingbuffer();
}

void AndroidAudioSourceStream::updateStreamIdle(bool idle)
{
    if (m_parent)
        m_parent->updateStreamIdle(idle);
}

QSpan<const std::byte>
AndroidAudioSourceStream::getHostSpan(void *audioData,
                                       int numFrames) const noexcept QT_MM_NONBLOCKING
{
    qsizetype byteAmount = m_hostFormat ? m_hostFormat->bytesForFrames(numFrames)
                                        : m_format.bytesForFrames(numFrames);
    return QSpan{ reinterpret_cast<const std::byte *>(audioData), byteAmount };
}

aaudio_data_callback_result_t
AndroidAudioSourceStream::processRingbuffer(QSpan<const std::byte> audioSpan,
                                             int numFrames) noexcept QT_MM_NONBLOCKING
{
    auto framesWritten = m_hostFormat
            ? PlatformAudioSourceStreamBase::process(
                      audioSpan, numFrames,
                      AudioHelperInternal::toNativeSampleFormat(m_hostFormat->sampleFormat()))
            : PlatformAudioSourceStreamBase::process(audioSpan, numFrames);

    if (framesWritten != static_cast<uint64_t>(numFrames) && isStopRequested())
        return AAUDIO_CALLBACK_RESULT_STOP;

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

aaudio_data_callback_result_t
AndroidAudioSourceStream::processCallback(QSpan<const std::byte> audioSpan) noexcept QT_MM_NONBLOCKING
{
    if (isStopRequested())
        return AAUDIO_CALLBACK_RESULT_STOP;

    if (m_hostFormat) {
        // When host format differs from the user format, we need to:
        // 1) Convert from host format to user format
        // 2) Run the callback with a buffer in the user format
        const AudioFormat &hostFormat = *m_hostFormat;
        const qsizetype numFrames = hostFormat.framesForBytes(audioSpan.size());
        const qsizetype userBufferSize = m_format.bytesForFrames(numFrames);

        // Allocate temporary buffer for user-format data
        QtPrivate::ScopedRTSanDisabler allowAllocations;
        auto userBuffer = q20::make_unique_for_overwrite<std::byte[]>(userBufferSize);
        std::span<std::byte> userSpan{ userBuffer.get(), static_cast<size_t>(userBufferSize) };

        // Convert from host format to user format
        AudioHelperInternal::convertSampleFormat(
                audioSpan,
                AudioHelperInternal::toNativeSampleFormat(hostFormat.sampleFormat()),
                userSpan,
                AudioHelperInternal::toNativeSampleFormat(m_format.sampleFormat()));

        // Run callback with user format
        QtMultimediaPrivate::runAudioCallback(*m_audioCallback, userSpan, m_format, volume());
    } else {
        QtMultimediaPrivate::runAudioCallback(*m_audioCallback, audioSpan, m_format, volume());
    }

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AndroidAudioSourceStream::handleError(aaudio_result_t)
{
    // Handle as IO error which closes the stream
    requestStop();
    invokeOnAppThread([this] {
        // clang-format off
        handleIOError(m_parent);
        // clang-format on
    });
}

AndroidAudioSource::AndroidAudioSource(AudioDevice device, const AudioFormat &format,
                                         QObject *parent)
    : BaseClass(std::move(device), format, parent)
{
}

AndroidAudioSource::~AndroidAudioSource()
    = default;

} // namespace QtAAudio

QT_END_NAMESPACE
