// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_AUDIOQIODEVICESUPPORT_P_H
#define QT_AUDIO_AUDIOQIODEVICESUPPORT_P_H

#include <QtCore/qdebug.h>
#include <QtCore/qglobal.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qmutex.h>
#include <span>

#include <qzMultimedia/private/AudioAlignmentSupport_p.h>
#include <qzMultimedia/private/AudioQSpanSupport_p.h>
#include <qzMultimedia/private/AudioRingBuffer_p.h>
#include <qzMultimedia/private/AutoResetEvent_p.h>

#include <deque>
#include <mutex>

namespace QtPrivate {

class IODeviceRingBufferWriterBase : public QIODevice
{
public:
    explicit IODeviceRingBufferWriterBase(QObject *parent = nullptr) : QIODevice(parent)
    {
        setOpenMode(QIODevice::WriteOnly | QIODevice::Unbuffered);

        m_bytesConsumed.callOnActivated([&] {
            qint64 bytes = m_bytesConsumedFromRingbuffer.exchange(0, std::memory_order_relaxed);
            if (bytes > 0)
                emit bytesWritten(bytes);
        });
    }

    void bytesConsumedFromRingbuffer(qint64 bytes)
    {
        m_bytesConsumedFromRingbuffer.fetch_add(bytes, std::memory_order_relaxed);
        m_bytesConsumed.set();
    }

    bool isSequential() const override { return true; }

private:
    QtPrivate::QAutoResetEvent m_bytesConsumed;
    std::atomic<qint64> m_bytesConsumedFromRingbuffer{ 0 };
};

template <typename SampleType>
class IODeviceRingBufferWriter final : public IODeviceRingBufferWriterBase
{
public:
    using Ringbuffer = QtPrivate::AudioRingBuffer<SampleType>;

    explicit IODeviceRingBufferWriter(Ringbuffer *rb, QObject *parent = nullptr)
        : IODeviceRingBufferWriterBase(parent), m_ringbuffer(rb)
    {
        Q_ASSERT(rb);
    }

    qint64 readData(char * , qint64 ) override { return -1; }
    qint64 writeData(const char *data, qint64 len) override
    {
        using namespace QtMultimediaPrivate;

        int64_t usableLength = alignDown(len, sizeof(SampleType));
        auto readRegion = std::span<const SampleType>{
            reinterpret_cast<const SampleType *>(data),
            static_cast<size_t>(usableLength / sizeof(SampleType)),
        };

        qint64 bytesWritten = m_ringbuffer->write(readRegion) * sizeof(SampleType);
        if (bytesWritten)
            emit readyRead();

        return bytesWritten;
    }

    qint64 bytesToWrite() const override { return m_ringbuffer->free() * sizeof(SampleType); }

private:
    Ringbuffer *const m_ringbuffer;
};

template <typename SampleType>
class IODeviceRingBufferReader final : public QIODevice
{
public:
    using Ringbuffer = QtPrivate::AudioRingBuffer<SampleType>;

    explicit IODeviceRingBufferReader(Ringbuffer *rb, QObject *parent = nullptr)
        : QIODevice(parent), m_ringbuffer(rb)
    {
        Q_ASSERT(rb);
    }

    qint64 readData(char *data, qint64 maxlen) override
    {
        using namespace QtMultimediaPrivate;

        std::span<std::byte> outputRegion = std::as_writable_bytes(std::span{ data, static_cast<size_t>(maxlen) });

        qsizetype maxSizeToRead = outputRegion.size_bytes() / sizeof(SampleType);

        int samplesConsumed = m_ringbuffer->consumeSome([&](auto readRegion) {
            std::span readByteRegion = std::as_bytes(readRegion);
            std::copy(readByteRegion.begin(), readByteRegion.end(), outputRegion.begin());
            outputRegion = drop(outputRegion, readByteRegion.size());

            return readRegion;
        }, maxSizeToRead);

        return samplesConsumed * sizeof(SampleType);
    }

    qint64 writeData(const char * , qint64 ) override { return -1; }
    qint64 bytesAvailable() const override { return m_ringbuffer->used() * sizeof(SampleType); }
    bool isSequential() const override { return true; }

private:
    Ringbuffer *const m_ringbuffer;
};

class QDequeIODevice final : public QIODevice
{
public:
    using Deque = std::deque<char>;

    explicit QDequeIODevice(QObject *parent = nullptr) : QIODevice(parent) { }

    qint64 bytesAvailable() const override { return qint64(m_deque.size()); }

private:
    qint64 readData(char *data, qint64 maxlen) override
    {
        std::lock_guard guard{ m_mutex };

        size_t bytesToRead = std::min<size_t>(m_deque.size(), maxlen);
        std::copy_n(m_deque.begin(), bytesToRead, data);

        m_deque.erase(m_deque.begin(), m_deque.begin() + bytesToRead);
        return qint64(bytesToRead);
    }

    qint64 writeData(const char *data, qint64 len) override
    {
        std::lock_guard guard{ m_mutex };
        m_deque.insert(m_deque.end(), data, data + len);
        return len;
    }

    QMutex m_mutex;
    Deque m_deque;
};

inline qint64 writeToDevice(QIODevice &device, std::span<const std::byte> data)
{
    return device.write(reinterpret_cast<const char *>(data.data()), data.size());
}

inline qint64 readFromDevice(QIODevice &device, std::span<std::byte> outputBuffer)
{
    return device.read(reinterpret_cast<char *>(outputBuffer.data()), outputBuffer.size());
}

template <typename SampleType>
qsizetype pullFromQIODeviceToRingbuffer(QIODevice &device, AudioRingBuffer<SampleType> &ringbuffer)
{
    using namespace QtMultimediaPrivate;

    int totalSamplesWritten = ringbuffer.produceSome([&](std::span<SampleType> writeRegion) {
        qint64 bytesAvailableInDevice = alignDown(device.bytesAvailable(), sizeof(SampleType));
        if (!bytesAvailableInDevice)
            return std::span<SampleType>{};

        qint64 samplesAvailableInDevice = bytesAvailableInDevice / sizeof(SampleType);
        writeRegion = take(writeRegion, samplesAvailableInDevice);

        qint64 bytesRead = readFromDevice(device, std::as_writable_bytes(writeRegion));
        if (bytesRead < 0) {
            qWarning() << "pullFromQIODeviceToRingbuffer cannot read from QIODevice:"
                       << device.errorString();
            return std::span<SampleType>{};
        }

        return take(writeRegion, bytesRead / sizeof(SampleType));
    });

    return totalSamplesWritten * sizeof(SampleType);
}

template <typename SampleType>
qsizetype pushToQIODeviceFromRingbuffer(QIODevice &device, AudioRingBuffer<SampleType> &ringbuffer)
{
    using namespace QtMultimediaPrivate;

    int totalSamplesWritten = ringbuffer.consumeSome([&](std::span<SampleType> region) {

        const quint64 bytesToWrite = [&] {
            const qint64 deviceBytesToWrite = device.bytesToWrite();
            return (deviceBytesToWrite > 0) ? alignDown(deviceBytesToWrite, sizeof(SampleType))
                                            : region.size_bytes();
        }();

        std::span<const std::byte> bufferByteRegion = take(std::as_bytes(region), bytesToWrite);
        int bytesWritten = writeToDevice(device, bufferByteRegion);
        if (bytesWritten < 0) {
            qWarning() << "pushToQIODeviceFromRingbuffer cannot push data to QIODevice:"
                       << device.errorString();
            return std::span<SampleType>{};
        }
        Q_ASSERT(isAligned(bytesWritten, sizeof(SampleType)));
        int samplesWritten = bytesWritten / sizeof(SampleType);
        return take(region, samplesWritten);
    });

    return totalSamplesWritten * sizeof(SampleType);
}

}

#endif
