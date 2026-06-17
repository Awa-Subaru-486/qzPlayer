// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_AUDIOSYSTEMPLATFORMSTREAMSUPPORT_P_H
#define QT_AUDIO_AUDIOSYSTEMPLATFORMSTREAMSUPPORT_P_H

#include <qzMultimedia/MultimediaGlobal.h>
#include <qzMultimedia/AudioFormat.h>
#include <qzMultimedia/AudioDevice.h>
#include <qzMultimedia/private/AutoResetEvent_p.h>
#include <qzMultimedia/private/AudioQIODeviceSupport_p.h>
#include <qzMultimedia/private/AudioRtsanSupport_p.h>
#include <qzMultimedia/private/AudioSystem_p.h>
#include <qzMultimedia/private/AudioHelpers_p.h>
#include <qzMultimedia/private/AudioRingBuffer_p.h>
#include <QtCore/qscopedvaluerollback.h>
#include <QtCore/qthread.h>

#include <optional>
#include <variant>

namespace QtPrivate {
class IODeviceRingBufferWriterBase;
}

namespace QtMultimediaPrivate {

class PlatformAudioIOStream
{
    template <typename T>
    using AudioRingBuffer = QtPrivate::AudioRingBuffer<T>;

    using Ringbuffer = std::variant<AudioRingBuffer<float>, AudioRingBuffer<int32_t>,
                                    AudioRingBuffer<int16_t>, AudioRingBuffer<uint8_t>>;

public:
    static qsizetype inferRingbufferFrames(const std::optional<int> &ringbufferSize,
                                           const std::optional<int32_t> &hardwareBufferFrames,
                                           const AudioFormat &);
    static qsizetype inferRingbufferBytes(const std::optional<int> &ringbufferSize,
                                          const std::optional<int32_t> &hardwareBufferFrames,
                                          const AudioFormat &);

protected:
    using NativeSampleFormat = AudioHelperInternal::NativeSampleFormat;
    using QAutoResetEvent = QtPrivate::QAutoResetEvent;

    PlatformAudioIOStream(AudioDevice m_audioDevice, AudioFormat m_format,
                           std::optional<int> ringbufferSize,
                           std::optional<int32_t> hardwareBufferFrames, float volume);
    ~PlatformAudioIOStream();
    Q_DISABLE_COPY_MOVE(PlatformAudioIOStream)

    void setVolume(float);
    float volume() const { return m_volume.load(std::memory_order_relaxed); }

    template <typename Functor>
    auto visitRingbuffer(Functor &&f)
    {
        return std::visit(f, m_ringbuffer);
    }

    template <typename Functor>
    auto visitRingbuffer(Functor &&f) const
    {
        return std::visit(f, m_ringbuffer);
    }

    void prepareRingbuffer(std::optional<int> ringbufferSize);
    int ringbufferSizeInBytes();

    void requestStop();
    bool isStopRequested(std::memory_order memory_order = std::memory_order_relaxed) const
    {
        return m_stopRequested.load(memory_order);
    }

    const AudioDevice m_audioDevice;
    const AudioFormat m_format;
    const std::optional<int32_t> m_hardwareBufferFrames;

private:
    std::atomic<float> m_volume{
        1.f,
    };

    Ringbuffer m_ringbuffer{
        std::in_place_type_t<AudioRingBuffer<float>>{},
        0,
    };

    std::atomic<bool> m_stopRequested = false;

public:
    enum class ShutdownPolicy : uint8_t
    {
        DrainRingbuffer,
        DiscardRingbuffer,
    };
};

class PlatformAudioSinkStream : protected PlatformAudioIOStream
{
public:
    using PlatformAudioIOStream::ShutdownPolicy;
    using AudioCallback = PlatformAudioSink::AudioCallback;

    using PlatformAudioIOStream::requestStop;

protected:
    PlatformAudioSinkStream(AudioDevice, const AudioFormat &, std::optional<int> ringbufferSize,
                             std::optional<int32_t> hardwareBufferFrames, float volume);
    ~PlatformAudioSinkStream();
    Q_DISABLE_COPY_MOVE(PlatformAudioSinkStream)

    uint64_t process(std::span<std::byte> hostBuffer, qsizetype totalNumberOfFrames,
                     std::optional<NativeSampleFormat> = {}) noexcept QT_MM_NONBLOCKING;

    quint64 bytesFree() const;
    std::chrono::microseconds processedDuration() const;

    virtual void updateStreamIdle(bool) = 0;

    QIODevice *createRingbufferWriterDevice();
    void setQIODevice(QIODevice *device);
    void createQIODeviceConnections(QIODevice *device);
    void disconnectQIODeviceConnections();
    void pullFromQIODevice();

    static constexpr int notificationThresholdBytes = 0;

    void setIdleState(bool);
    bool isIdle(std::memory_order order = std::memory_order_relaxed) const
    {
        return m_streamIsIdle.load(order);
    }
    void stopIdleDetection();

    template <typename Functor>
    auto connectIdleHandler(Functor &&f)
    {
        return m_streamIdleDetectionNotifier.callOnActivated(std::forward<Functor>(f));
    }

    template <typename ParentType>
    void handleIOError(ParentType *parent)
    {
        if (parent) {
            Q_ASSERT(thread()->isCurrentThread());
            parent->m_stream = {};
            parent->setError(Audio::IOError);
            parent->updateStreamState(QtAudio::State::StoppedState);
        }
    }

    QThread *thread() const;

    template <typename Functor>
    void invokeOnAppThread(Functor &&f)
    {

        QMetaObject::invokeMethod(&m_streamIdleDetectionNotifier, std::forward<Functor>(f));
    }

private:

    QIODevice *m_device = nullptr;

    std::atomic<bool> m_streamIsIdle = false;
    QAutoResetEvent m_streamIdleDetectionNotifier;
    QMetaObject::Connection m_streamIdleDetectionConnection;

    QAutoResetEvent m_ringbufferHasSpace;
    QMetaObject::Connection m_ringbufferHasSpaceConnection;
    QMetaObject::Connection m_iodeviceHasNewDataConnection;

    std::unique_ptr<QtPrivate::IODeviceRingBufferWriterBase> m_ringbufferWriterDevice;

    std::atomic_int64_t m_totalFrameCount{};
    std::atomic_int64_t m_processedFrameCount{};

    void convertToNative(std::span<const std::byte> internal, std::span<std::byte> native, float volume,
                         NativeSampleFormat) noexcept QT_MM_NONBLOCKING;

    template <typename Functor>
    void withPullIODeviceReentrancyGuard(Functor f)
    {
        if (!m_pullIODeviceReentrancyGuard) {
            QScopedValueRollback<bool> guard{
                m_pullIODeviceReentrancyGuard,
                true,
            };
            f();
        } else {
            QMetaObject::invokeMethod(&m_streamIdleDetectionNotifier,
                                      [this, f = std::move(f)]() mutable {
                withPullIODeviceReentrancyGuard(std::move(f));
            }, Qt::QueuedConnection);
        }
    }
    bool m_pullIODeviceReentrancyGuard = false;

    void pullFromQIODeviceImpl();
};

class PlatformAudioSourceStream : protected PlatformAudioIOStream
{
public:
    using PlatformAudioIOStream::ShutdownPolicy;
    using AudioCallback = PlatformAudioSource::AudioCallback;

    using PlatformAudioIOStream::requestStop;

protected:
    PlatformAudioSourceStream(AudioDevice, const AudioFormat &,
                               std::optional<int> ringbufferSize,
                               std::optional<int32_t> hardwareBufferFrames, float volume);
    ~PlatformAudioSourceStream();

    Q_DISABLE_COPY_MOVE(PlatformAudioSourceStream)

    uint64_t process(std::span<const std::byte> hostBuffer, qsizetype numberOfFrames,
                     std::optional<NativeSampleFormat> = {}) noexcept QT_MM_NONBLOCKING;

    qsizetype bytesReady() const;
    std::chrono::microseconds processedDuration() const;

    void setQIODevice(QIODevice *device);
    void createQIODeviceConnections(QIODevice *device);
    void disconnectQIODeviceConnections();
    QIODevice *createRingbufferReaderDevice();
    void pushToIODevice();
    bool deviceIsRingbufferReader() const;
    void finalizeQIODevice(ShutdownPolicy);
    void emptyRingbuffer();

    virtual void updateStreamIdle(bool) = 0;

    template <typename ParentType>
    void handleIOError(ParentType *parent)
    {
        if (parent) {
            Q_ASSERT(thread()->isCurrentThread());

            if (deviceIsRingbufferReader())

                parent->m_retiredStream = std::move(parent->m_stream);
            else
                parent->m_stream = {};

            parent->setError(Audio::IOError);
            parent->updateStreamState(QtAudio::State::StoppedState);
        }
    }

    QThread *thread() const;

    template <typename Functor>
    void invokeOnAppThread(Functor &&f)
    {

        QMetaObject::invokeMethod(&m_ringbufferHasData, std::forward<Functor>(f));
    }

private:

    QIODevice *m_device = nullptr;
    std::unique_ptr<QIODevice> m_ringbufferReaderDevice;

    QAutoResetEvent m_ringbufferHasData;
    QAutoResetEvent m_ringbufferIsFull;

    QMetaObject::Connection m_ringbufferHasDataConnection;
    QMetaObject::Connection m_ringbufferIsFullConnection;

    std::atomic_uint64_t m_totalNumberOfFramesPushedToRingbuffer{};

    void convertFromNative(std::span<const std::byte> native, std::span<std::byte> internal, float volume,
                           NativeSampleFormat) noexcept QT_MM_NONBLOCKING;
};

}

#endif
