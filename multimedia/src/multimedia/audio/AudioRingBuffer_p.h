// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.


#ifndef QT_AUDIO_AUDIORINGBUFFER_P_H
#define QT_AUDIO_AUDIORINGBUFFER_P_H
#include <span>
#include <QtCore/qtclasshelpermacros.h>
#include <qzMultimedia/private/AudioQSpanSupport_p.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <limits>
#include <type_traits>

namespace QtPrivate {

template <typename T>
class AudioRingBuffer
{
    static constexpr bool isTriviallyDestructible = std::is_trivially_destructible_v<T>;

public:
    using ValueType = T;
    using Region = std::span<T>;
    using ConstRegion = std::span<const T>;

    explicit AudioRingBuffer(int bufferSize) : m_bufferSize(bufferSize)
    {
        if (bufferSize)
            m_buffer = reinterpret_cast<T *>(
                    ::operator new(sizeof(T) * bufferSize, std::align_val_t(alignof(T))));
    }

    Q_DISABLE_COPY_MOVE(AudioRingBuffer)

    ~AudioRingBuffer()
    {
        if constexpr (!isTriviallyDestructible) {
            consumeAll([](auto ) {
            });
        }

        ::operator delete(reinterpret_cast<void *>(m_buffer), std::align_val_t(alignof(T)));
    }

    int write(ConstRegion region)
    {
        using namespace QtMultimediaPrivate;
        return produceSome([&](Region writeRegion) {
            qsizetype writeSize = std::min(region.size(), writeRegion.size());
            std::uninitialized_copy_n(region.data(), writeSize, writeRegion.data());
            region = drop(region, writeSize);

            return Region{
                writeRegion.data(),
                static_cast<size_t>(writeSize),
            };
        });
    }

    bool write(T element)
    {
        return produceOne([&] {
            return std::move(element);
        });
    }

    template <typename Functor>
    bool produceOne(Functor &&producer)
#ifdef __cpp_concepts
            requires
            std::is_invocable_v<Functor> &&std::is_constructible_v<T, std::invoke_result_t<Functor>>
#endif
    {
        Region writeRegion = acquireWriteRegion(1);
        if (writeRegion.empty())
            return false;
        T *writeElement = writeRegion.data();
        new (writeElement) T{ producer() };
        releaseWriteRegion(1);
        return true;
    }

    template <typename Functor>
    int produceSome(Functor &&producer, int elements = std::numeric_limits<int>::max())
#ifdef __cpp_concepts
            requires std::is_invocable_v<Functor, Region>
                    &&std::is_same_v<std::invoke_result_t<Functor, Region>, Region>
#endif
    {
        int elementsRemain = elements;
        int elementsWritten = 0;
        while (elementsRemain) {
            Region writeRegion = acquireWriteRegion(elementsRemain);
            if (writeRegion.empty())
                break;

            Region writtenRegion = producer(writeRegion);
            if (writtenRegion.empty())
                break;

            Q_ASSERT(writtenRegion.data() == writeRegion.data());
            Q_ASSERT(writtenRegion.size() <= writeRegion.size());

            elementsRemain -= writtenRegion.size();
            elementsWritten += writtenRegion.size();
            releaseWriteRegion(writtenRegion.size());
        }
        return elementsWritten;
    }

    template <typename Functor>
    int consumeAll(Functor &&consumer)
    {
        return consumeSome([&](Region region) {
            consumer(region);
            return region;
        });
    }

    template <typename Functor>
    int consume(int elements, Functor &&consumer)
    {
        int remainingElemnts = elements;
        return consumeSome([&](Region region) {
            if (remainingElemnts == 0)
                return Region{};

            Region regionToConsume = QtMultimediaPrivate::take(region, remainingElemnts);
            consumer(regionToConsume);
            remainingElemnts -= regionToConsume.size();
            return regionToConsume;
        });
    }

    template <typename Functor>
    int consumeSome(Functor &&consumer, int elements = std::numeric_limits<int>::max())
#ifdef __cpp_concepts
            requires std::is_invocable_v<Functor, Region>
                    &&std::is_same_v<std::invoke_result_t<Functor, Region>, Region>
#endif
    {
        int elementsConsumed = 0;

        while (elements > elementsConsumed) {
            Region readRegion = acquireReadRegion(elements - elementsConsumed);
            if (readRegion.empty())
                break;

            Region consumedRegion = consumer(readRegion);
            if (consumedRegion.empty())
                break;
            Q_ASSERT(consumedRegion.data() == readRegion.data());
            Q_ASSERT(consumedRegion.size() <= readRegion.size());

            if constexpr (!isTriviallyDestructible)
                std::destroy(consumedRegion.begin(), consumedRegion.end());

            elementsConsumed += consumedRegion.size();
            releaseReadRegion(consumedRegion.size());
        }

        return elementsConsumed;
    }

    int used() const { return m_bufferUsed.load(std::memory_order_relaxed); }
    int free() const { return m_bufferSize - m_bufferUsed.load(std::memory_order_relaxed); }

    int size() const { return m_bufferSize; }

    void reset()
#ifdef __cpp_concepts
            requires isTriviallyDestructible
#endif
    {
        m_readPos = 0;
        m_writePos = 0;
        m_bufferUsed.store(0, std::memory_order_relaxed);
    }

private:
    Region acquireWriteRegion(int size)
    {
        const int free = m_bufferSize - m_bufferUsed.load(std::memory_order_acquire);

        Region output;
        if (free > 0) {
            const int writeSize = qMin(size, qMin(m_bufferSize - m_writePos, free));
            output = writeSize > 0 ? Region(m_buffer + m_writePos, static_cast<size_t>(writeSize)) : Region();
        } else {
            output = Region();
        }
        return output;
    }

    void releaseWriteRegion(int elementsRead)
    {
        m_writePos = (m_writePos + elementsRead) % m_bufferSize;
        m_bufferUsed.fetch_add(elementsRead, std::memory_order_release);
    }

    Region acquireReadRegion(int size)
    {
        const int used = m_bufferUsed.load(std::memory_order_acquire);

        if (used > 0) {
            const int readSize = qMin(size, qMin(m_bufferSize - m_readPos, used));
            return readSize > 0 ? Region(m_buffer + m_readPos, static_cast<size_t>(readSize)) : Region();
        }

        return Region();
    }

    void releaseReadRegion(int elementsWritten)
    {
        m_readPos = (m_readPos + elementsWritten) % m_bufferSize;
        m_bufferUsed.fetch_sub(elementsWritten, std::memory_order_release);
    }

    const int m_bufferSize;
    int m_readPos{};
    int m_writePos{};
    T *m_buffer{};
    std::atomic_int m_bufferUsed{};
};

}

#endif
