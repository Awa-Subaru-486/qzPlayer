// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "AudioSystemPlatformStreamSupport_p.h"

#include <QtCore/qdebug.h>
#include <qzMultimedia/private/AudioHelpers_p.h>
#include <qzMultimedia/private/AudioQIODeviceSupport_p.h>
#include <qzMultimedia/private/MultimediaAssume_p.h>

#include <stdlib.h>
#if __has_include(<alloca.h>)
#  include <alloca.h>
#endif
#if __has_include(<malloc.h>)
#  include <malloc.h>
#endif

#ifdef Q_CC_MSVC
#  define alloca _alloca
#endif

namespace QtMultimediaPrivate {

using namespace std::chrono_literals;

PlatformAudioIOStream::PlatformAudioIOStream(AudioDevice m_audioDevice, AudioFormat m_format,
                                               std::optional<int> ringbufferSize,
                                               std::optional<int32_t> hardwareBufferFrames,
                                               float volume)
    : m_audioDevice{
          std::move(m_audioDevice),
      },
      m_format{
          m_format,
      },
      m_hardwareBufferFrames{
          hardwareBufferFrames,
      },
      m_volume{
          volume,
      }
{
    prepareRingbuffer(ringbufferSize);
}

PlatformAudioIOStream::~PlatformAudioIOStream()
{
    Q_ASSERT(m_stopRequested);
}

void PlatformAudioIOStream::setVolume(float volume)
{
    m_volume.store(volume, std::memory_order_relaxed);
}

void PlatformAudioIOStream::prepareRingbuffer(std::optional<int> ringbufferSize)
{
    using SampleFormat = AudioFormat::SampleFormat;

    int ringbufferElements = inferRingbufferFrames(ringbufferSize, m_hardwareBufferFrames, m_format)
            * m_format.channelCount();

    switch (m_format.sampleFormat()) {
    case SampleFormat::Float:
        m_ringbuffer.emplace<AudioRingBuffer<float>>(ringbufferElements);
        break;
    case SampleFormat::Int16:
        m_ringbuffer.emplace<AudioRingBuffer<int16_t>>(ringbufferElements);
        break;
    case SampleFormat::Int32:
        m_ringbuffer.emplace<AudioRingBuffer<int32_t>>(ringbufferElements);
        break;
    case SampleFormat::UInt8:
        m_ringbuffer.emplace<AudioRingBuffer<uint8_t>>(ringbufferElements);
        break;

    default:
        qCritical() << "invalid sample format";
        Q_UNREACHABLE_RETURN();
    }
}

void PlatformAudioIOStream::requestStop()
{
    m_stopRequested.store(true, std::memory_order_release);
}

qsizetype
PlatformAudioIOStream::inferRingbufferFrames(const std::optional<int> &ringbufferSize,
                                              const std::optional<int32_t> &hardwareBufferFrames,
                                              const AudioFormat &format)
{
    int bytesPerFrame = format.bytesPerFrame();
    QT_MM_ASSUME(bytesPerFrame > 0);

    return inferRingbufferBytes(ringbufferSize, hardwareBufferFrames, format) / bytesPerFrame;
}

qsizetype
PlatformAudioIOStream::inferRingbufferBytes(const std::optional<int> &ringbufferSize,
                                             const std::optional<int32_t> &hardwareBufferFrames,
                                             const AudioFormat &format)
{

    const int minimumRingbufferFrames = hardwareBufferFrames ? *hardwareBufferFrames * 2 : 32;
    const int minimumRingbufferBytes = format.bytesForFrames(minimumRingbufferFrames);
    if (ringbufferSize)
        return ringbufferSize >= minimumRingbufferBytes ? *ringbufferSize : minimumRingbufferBytes;

    using namespace std::chrono;
    static constexpr auto defaultBufferDuration = 250ms;

    return format.bytesForDuration(microseconds(defaultBufferDuration).count());
}

int PlatformAudioIOStream::ringbufferSizeInBytes()
{
    return visitRingbuffer([](auto &ringbuffer) {
        using SampleType = typename std::decay_t<decltype(ringbuffer)>::ValueType;
        return ringbuffer.size() * sizeof(SampleType);
    });
}

PlatformAudioSinkStream::PlatformAudioSinkStream(AudioDevice audioDevice,
                                                   const AudioFormat &format,
                                                   std::optional<int> ringbufferSize,
                                                   std::optional<int32_t> hardwareBufferFrames,
                                                   float volume)
    : PlatformAudioIOStream{
          std::move(audioDevice), format, ringbufferSize, hardwareBufferFrames, volume,
      }
{
    m_streamIdleDetectionConnection = m_streamIdleDetectionNotifier.callOnActivated([this] {
        if (isStopRequested())
            return;

        bool sinkIsIdle = m_streamIsIdle.load();

        if (sinkIsIdle) {

            bool ringbufferIsEmpty = visitRingbuffer([&](auto &ringbuffer) {
                return ringbuffer.free() == ringbuffer.size();
            });

            sinkIsIdle = ringbufferIsEmpty;
        }

        updateStreamIdle(sinkIsIdle);
    });
}

PlatformAudioSinkStream::~PlatformAudioSinkStream() = default;

uint64_t
PlatformAudioSinkStream::process(std::span<std::byte> hostBuffer, qsizetype totalNumberOfFrames,
                                  std::optional<NativeSampleFormat> nativeFormat) noexcept QT_MM_NONBLOCKING
{
    qsizetype totalNumberOfSamples = totalNumberOfFrames * m_format.channelCount();

    const float vol = volume();

    int samplesConsumedFromRingbuffer = visitRingbuffer([&](auto &ringbuffer) {
        return ringbuffer.consume(totalNumberOfSamples, [&](auto ringbufferRange) {
            if (nativeFormat) {

                const qsizetype samplesInChunk = ringbufferRange.size();
                const qsizetype bytesInChunk = samplesInChunk * bytesPerSample(*nativeFormat);

                std::span<std::byte> outputByteRange = take(hostBuffer, bytesInChunk);
                hostBuffer = drop(hostBuffer, bytesInChunk);
                convertToNative(std::as_bytes(ringbufferRange), outputByteRange, vol, *nativeFormat);
            } else {
                std::span<std::byte> outputByteRange = take(hostBuffer, ringbufferRange.size_bytes());
                hostBuffer = drop(hostBuffer, ringbufferRange.size_bytes());
                AudioHelperInternal::applyVolume(vol, m_format, std::as_bytes(ringbufferRange),
                                                  outputByteRange);
            }
        });
    });

    if (m_ringbufferWriterDevice) {
        qint64 bytes = samplesConsumedFromRingbuffer * m_format.bytesPerSample();
        m_ringbufferWriterDevice->bytesConsumedFromRingbuffer(bytes);
    }

    if (!isStopRequested()) {
        if (notificationThresholdBytes == 0 || bytesFree() > notificationThresholdBytes)
            m_ringbufferHasSpace.set();

        bool streamIsIdle = m_streamIsIdle.load(std::memory_order_relaxed);
        if (streamIsIdle && samplesConsumedFromRingbuffer) {
            m_streamIsIdle.store(false);
            m_streamIdleDetectionNotifier.set();
        } else if (!streamIsIdle && !samplesConsumedFromRingbuffer) {
            m_streamIsIdle.store(true);
            m_streamIdleDetectionNotifier.set();
        }
    }
    if (!hostBuffer.empty())
        std::fill_n(hostBuffer.data(), hostBuffer.size(), std::byte{});

    uint64_t consumedFrames = samplesConsumedFromRingbuffer / m_format.channelCount();
    m_processedFrameCount += consumedFrames;
    m_totalFrameCount += totalNumberOfFrames;

    return consumedFrames;
}

quint64 PlatformAudioSinkStream::bytesFree() const
{
    return visitRingbuffer([](auto &ringbuffer) {
        using SampleType = typename std::decay_t<decltype(ringbuffer)>::ValueType;
        return ringbuffer.free() * sizeof(SampleType);
    });
}

std::chrono::microseconds PlatformAudioSinkStream::processedDuration() const
{
    return std::chrono::microseconds{
        m_processedFrameCount * 1'000'000 / m_format.sampleRate(),
    };
}

void PlatformAudioSinkStream::pullFromQIODevice()
{
    withPullIODeviceReentrancyGuard([this] {
        pullFromQIODeviceImpl();
    });
}

void PlatformAudioSinkStream::pullFromQIODeviceImpl()
{
    Q_ASSERT(thread()->isCurrentThread());
    Q_ASSERT(m_device);
    Q_ASSERT(m_pullIODeviceReentrancyGuard);

    visitRingbuffer([&](auto &ringbuffer) {
        int elementsPulled = pullFromQIODeviceToRingbuffer(*m_device, ringbuffer);
        if (elementsPulled)
            updateStreamIdle(false);
    });
}

void PlatformAudioSinkStream::createQIODeviceConnections(QIODevice *device)
{

    m_ringbufferHasSpaceConnection = m_ringbufferHasSpace.callOnActivated(device, [this] {
        pullFromQIODevice();
    });

    m_iodeviceHasNewDataConnection =
            QObject::connect(device, &QIODevice::readyRead, device, [this] {
        withPullIODeviceReentrancyGuard([this] {
            pullFromQIODeviceImpl();
            updateStreamIdle(false);
        });
    });
}

void PlatformAudioSinkStream::disconnectQIODeviceConnections()
{
    QObject::disconnect(m_ringbufferHasSpaceConnection);
    QObject::disconnect(m_iodeviceHasNewDataConnection);
}

QIODevice *PlatformAudioSinkStream::createRingbufferWriterDevice()
{
    m_ringbufferWriterDevice = visitRingbuffer(
            [&](auto &ringbuffer) -> std::unique_ptr<QtPrivate::IODeviceRingBufferWriterBase> {
        using SampleType = typename std::decay_t<decltype(ringbuffer)>::ValueType;
        return std::make_unique<QtPrivate::IODeviceRingBufferWriter<SampleType>>(&ringbuffer);
    });

    return m_ringbufferWriterDevice.get();
}

void PlatformAudioSinkStream::setQIODevice(QIODevice *device)
{
    m_device = device;
}

void PlatformAudioSinkStream::setIdleState(bool x)
{
    m_streamIsIdle.store(x);
}

void PlatformAudioSinkStream::stopIdleDetection()
{
    QObject::disconnect(m_streamIdleDetectionConnection);
}

QThread *PlatformAudioSinkStream::thread() const
{

    return m_streamIdleDetectionNotifier.thread();
}

static constexpr qsizetype scratchpadBufferSizeLimit = 512 * 1024;
static_assert(scratchpadBufferSizeLimit > 4092 * 32 * sizeof(float));

void PlatformAudioSinkStream::convertToNative(std::span<const std::byte> internal,
                                               std::span<std::byte> native, float volume,
                                               NativeSampleFormat nativeFormat) noexcept QT_MM_NONBLOCKING
{
    using namespace AudioHelperInternal;

    if (volume == 1.f) {
        convertSampleFormat(internal, toNativeSampleFormat(m_format.sampleFormat()), native,
                            nativeFormat);
        return;
    }

    Q_ASSERT(internal.size() <= scratchpadBufferSizeLimit);
    std::byte *scratchpadMemory = reinterpret_cast<std::byte *>(alloca(internal.size()));
    std::span scratchpadBuffer{ scratchpadMemory, internal.size() };

    applyVolume(volume, m_format, internal, scratchpadBuffer);
    convertSampleFormat(scratchpadBuffer, toNativeSampleFormat(m_format.sampleFormat()), native,
                        nativeFormat);
}

PlatformAudioSourceStream::PlatformAudioSourceStream(AudioDevice audioDevice,
                                                       const AudioFormat &format,
                                                       std::optional<int> ringbufferSize,
                                                       std::optional<int32_t> hardwareBufferFrames,
                                                       float volume)
    : PlatformAudioIOStream{
          std::move(audioDevice), format, ringbufferSize, hardwareBufferFrames, volume,
      }
{
}

PlatformAudioSourceStream::~PlatformAudioSourceStream() = default;

uint64_t PlatformAudioSourceStream::process(
        std::span<const std::byte> hostBuffer, qsizetype numberOfFrames,
        std::optional<NativeSampleFormat> nativeFormat) noexcept QT_MM_NONBLOCKING
{
    qsizetype remainingNumberOfSamples = numberOfFrames * m_format.channelCount();

    const float vol = volume();
    using namespace QtMultimediaPrivate;

    uint64_t totalSamplesWritten = visitRingbuffer([&](auto &rb) {
        using SampleType = typename std::decay_t<decltype(rb)>::ValueType;

        return rb.produceSome([&](std::span<SampleType> ringbufferRange) {
            if (nativeFormat) {

                const qsizetype samplesInChunk = ringbufferRange.size();
                const qsizetype bytesInChunk = samplesInChunk * bytesPerSample(*nativeFormat);

                std::span<const std::byte> inputByteRange = take(hostBuffer, bytesInChunk);
                hostBuffer = drop(hostBuffer, bytesInChunk);
                convertFromNative(inputByteRange, std::as_writable_bytes(ringbufferRange), vol,
                                  *nativeFormat);
            } else {
                std::span<const std::byte> inputByteRange =
                        take(hostBuffer, ringbufferRange.size_bytes());
                hostBuffer = drop(hostBuffer, ringbufferRange.size_bytes());
                AudioHelperInternal::applyVolume(vol, m_format, inputByteRange,
                                                  std::as_writable_bytes(ringbufferRange));
            }
            return ringbufferRange;
        }, remainingNumberOfSamples);

    });

    if (totalSamplesWritten)
        m_ringbufferHasData.set();

    uint64_t framesWritten = totalSamplesWritten / m_format.channelCount();
    m_totalNumberOfFramesPushedToRingbuffer += framesWritten;
    return framesWritten;
}

void PlatformAudioSourceStream::pushToIODevice()
{
    Q_ASSERT(thread()->isCurrentThread());

    qsizetype bytesPushed = visitRingbuffer([&](auto &ringbuffer) {
        return QtPrivate::pushToQIODeviceFromRingbuffer(*m_device, ringbuffer);
    });

    if (bytesPushed)
        Q_EMIT m_device->readyRead();
}

bool PlatformAudioSourceStream::deviceIsRingbufferReader() const
{
    return m_device == m_ringbufferReaderDevice.get();
}

void PlatformAudioSourceStream::finalizeQIODevice(ShutdownPolicy shutdownPolicy)
{
    switch (shutdownPolicy) {
    case ShutdownPolicy::DiscardRingbuffer:
        return;
    case ShutdownPolicy::DrainRingbuffer:
        if (!deviceIsRingbufferReader())
            pushToIODevice();
        return;

    default:
        Q_UNREACHABLE_RETURN();
    }
}

void PlatformAudioSourceStream::emptyRingbuffer()
{
    visitRingbuffer([](auto &ringbuffer) {
        ringbuffer.consumeAll([](auto &) {
        });
    });
}

QThread *PlatformAudioSourceStream::thread() const
{

    return m_ringbufferHasData.thread();
}

qsizetype PlatformAudioSourceStream::bytesReady() const
{
    return visitRingbuffer([](const auto &ringbuffer) {
        return ringbuffer.used() * sizeof(typename std::decay_t<decltype(ringbuffer)>::ValueType);
    });
}

std::chrono::microseconds PlatformAudioSourceStream::processedDuration() const
{
    return std::chrono::microseconds{
        m_format.durationForFrames(
                m_totalNumberOfFramesPushedToRingbuffer.load(std::memory_order_relaxed)),
    };
}

void PlatformAudioSourceStream::setQIODevice(QIODevice *device)
{
    m_device = device;
}

void PlatformAudioSourceStream::createQIODeviceConnections(QIODevice *device)
{
    bool pushToDevice = !deviceIsRingbufferReader();

    if (pushToDevice) {
        m_ringbufferHasDataConnection = m_ringbufferHasData.callOnActivated(device, [this] {
            if (!isStopRequested())
                updateStreamIdle(false);
            pushToIODevice();
        });
    } else {
        m_ringbufferHasDataConnection = m_ringbufferHasData.callOnActivated(device, [this] {
            if (!isStopRequested())
                updateStreamIdle(false);
            Q_EMIT m_device->readyRead();
        });
    }

    m_ringbufferIsFullConnection = m_ringbufferHasData.callOnActivated(device, [this] {
        if (!isStopRequested())
            updateStreamIdle(false);
    });
}

void PlatformAudioSourceStream::disconnectQIODeviceConnections()
{
    QObject::disconnect(m_ringbufferHasDataConnection);
    QObject::disconnect(m_ringbufferIsFullConnection);
}

QIODevice *PlatformAudioSourceStream::createRingbufferReaderDevice()
{
    using namespace QtPrivate;

    m_ringbufferReaderDevice = visitRingbuffer([&](auto &rb) -> std::unique_ptr<QIODevice> {
        using SampleType = typename std::decay_t<decltype(rb)>::ValueType;
        return std::make_unique<IODeviceRingBufferReader<SampleType>>(&rb);
    });

    m_ringbufferReaderDevice->open(QIODevice::ReadOnly | QIODevice::Unbuffered);

    return m_ringbufferReaderDevice.get();
}

void PlatformAudioSourceStream::convertFromNative(
        std::span<const std::byte> native, std::span<std::byte> internal, float volume,
        NativeSampleFormat nativeFormat) noexcept QT_MM_NONBLOCKING
{
    using namespace AudioHelperInternal;
    if (volume == 1.f) {
        convertSampleFormat(native, nativeFormat, internal,
                            AudioHelperInternal::toNativeSampleFormat(m_format.sampleFormat()));
        return;
    }

    Q_ASSERT(internal.size() <= scratchpadBufferSizeLimit);
    std::byte *scratchpadMemory = reinterpret_cast<std::byte *>(alloca(internal.size()));
    std::span scratchpadBuffer{ scratchpadMemory, internal.size() };

    convertSampleFormat(native, nativeFormat, scratchpadBuffer,
                        AudioHelperInternal::toNativeSampleFormat(m_format.sampleFormat()));

    applyVolume(volume, m_format, scratchpadBuffer, internal);
}

}

#ifdef Q_CC_MSVC
#  undef alloca
#endif
