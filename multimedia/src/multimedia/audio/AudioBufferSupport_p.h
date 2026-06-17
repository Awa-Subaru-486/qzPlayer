// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_AUDIOBUFFERSUPPORT_P_H
#define QT_AUDIO_AUDIOBUFFERSUPPORT_P_H

#include <qzMultimedia/AudioBuffer.h>
#include <span>

namespace QtPrivate {

enum class Mutability {
    Mutable,
    Immutable,
};

constexpr Mutability AudioBufferMutable = Mutability::Mutable;
constexpr Mutability AudioBufferImmutable = Mutability::Immutable;

template <typename SampleType>
struct AudioBufferChannelView
{
    SampleType &operator[](int frame) { return m_buffer[frame * m_numberOfChannels + m_channel]; }

    std::span<SampleType> m_buffer;
    const int m_channel;
    const int m_numberOfChannels;
};

template <typename SampleType>
void validateBufferFormat(const AudioBuffer &buffer, int channel)
{
    Q_ASSERT(channel < buffer.format().channelCount());

    if constexpr (std::is_same_v<std::remove_const_t<SampleType>, float>) {
        Q_ASSERT(buffer.format().sampleFormat() == AudioFormat::SampleFormat::Float);
    } else if constexpr (std::is_same_v<std::remove_const_t<SampleType>, int32_t>) {
        Q_ASSERT(buffer.format().sampleFormat() == AudioFormat::SampleFormat::Int32);
    } else if constexpr (std::is_same_v<std::remove_const_t<SampleType>, int16_t>) {
        Q_ASSERT(buffer.format().sampleFormat() == AudioFormat::SampleFormat::Int16);
    } else if constexpr (std::is_same_v<std::remove_const_t<SampleType>, uint8_t>) {
        Q_ASSERT(buffer.format().sampleFormat() == AudioFormat::SampleFormat::UInt8);
    }
}

template <typename T, bool Predicate>
using add_const_if_t = std::conditional_t<Predicate, std::add_const_t<T>, T>;

template <typename SampleType>
auto makeChannelView(add_const_if_t<AudioBuffer, std::is_const_v<SampleType>> &buffer, int channel)
{
    validateBufferFormat<SampleType>(buffer, channel);
    return AudioBufferChannelView<SampleType>{
        std::span{ buffer.template data<SampleType>(), static_cast<size_t>(buffer.sampleCount()) },
        channel,
        buffer.format().channelCount(),
    };
}

template <typename SampleType>
struct AudioBufferDeinterleaveAdaptor
{
    using BufferType = add_const_if_t<AudioBuffer, std::is_const_v<SampleType>>;

    AudioBufferChannelView<SampleType> operator[](int channel)
    {
        return makeChannelView<SampleType>(m_buffer, channel);
    }

    AudioBufferChannelView<const SampleType> operator[](int channel) const
    {
        return makeChannelView<const SampleType>(m_buffer, channel);
    }

    explicit AudioBufferDeinterleaveAdaptor(BufferType &buffer) : m_buffer(buffer) { }

private:
    BufferType &m_buffer;
    int m_numberOfChannels = m_buffer.format().channelCount();
};

}

#endif
