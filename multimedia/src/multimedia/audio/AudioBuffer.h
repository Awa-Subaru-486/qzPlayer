// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_AUDIOBUFFER_H
#define QT_AUDIO_AUDIOBUFFER_H
#include <QtCore/qshareddata.h>

#include <qzMultimedia/MultimediaGlobal.h>

#include <qzMultimedia/QtAudio.h>
#include <qzMultimedia/AudioFormat.h>

namespace QtPrivate {
template <AudioFormat::SampleFormat> struct AudioSampleFormatHelper
{
};

template <> struct AudioSampleFormatHelper<AudioFormat::UInt8>
{
    using value_type = unsigned char;
    static constexpr value_type Default = 128;
};

template <> struct AudioSampleFormatHelper<AudioFormat::Int16>
{
    using value_type = short;
    static constexpr value_type Default = 0;
};

template <> struct AudioSampleFormatHelper<AudioFormat::Int32>
{
    using value_type = int;
    static constexpr value_type Default = 0;
};

template <> struct AudioSampleFormatHelper<AudioFormat::Float>
{
    using value_type = float;
    static constexpr value_type Default = 0.;
};

}

template <AudioFormat::ChannelConfig config, AudioFormat::SampleFormat format>
struct AudioFrame
{
private:

    static constexpr int constexprPopcount(quint32 i)
    {
        i = i - ((i >> 1) & 0x55555555);
        i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
        i = (i + (i >> 4)) & 0x0F0F0F0F;
        return (i * 0x01010101) >> 24;
    }
    static constexpr int nChannels = constexprPopcount(config);
public:
    using value_type = typename QtPrivate::AudioSampleFormatHelper<format>::value_type;
    value_type channels[nChannels];
    static constexpr int positionToIndex(AudioFormat::AudioChannelPosition pos)
    {
        if (!(config & (1u << pos)))
            return -1;

        uint maskedChannels = config & ((1u << pos) - 1);
        return qPopulationCount(maskedChannels);
    }

    value_type value(AudioFormat::AudioChannelPosition pos) const {
        int idx = positionToIndex(pos);
        if (idx < 0)
            return QtPrivate::AudioSampleFormatHelper<format>::Default;
        return channels[idx];
    }
    void setValue(AudioFormat::AudioChannelPosition pos, value_type val) {
        int idx = positionToIndex(pos);
        if (idx < 0)
            return;
        channels[idx] = val;
    }
    value_type operator[](AudioFormat::AudioChannelPosition pos) const {
        return value(pos);
    }
    constexpr void clear() {
        for (int i = 0; i < nChannels; ++i)
            channels[i] = QtPrivate::AudioSampleFormatHelper<format>::Default;
    }
};

template <AudioFormat::SampleFormat Format>
using AudioFrameMono = AudioFrame<AudioFormat::ChannelConfigMono, Format>;

template <AudioFormat::SampleFormat Format>
using AudioFrameStereo = AudioFrame<AudioFormat::ChannelConfigStereo, Format>;

template <AudioFormat::SampleFormat Format>
using QAudioFrame2Dot1 = AudioFrame<AudioFormat::ChannelConfig2Dot1, Format>;

template <AudioFormat::SampleFormat Format>
using QAudioFrameSurround5Dot1 = AudioFrame<AudioFormat::ChannelConfigSurround5Dot1, Format>;

template <AudioFormat::SampleFormat Format>
using QAudioFrameSurround7Dot1 = AudioFrame<AudioFormat::ChannelConfigSurround7Dot1, Format>;

class AudioBufferPrivate;
QT_DECLARE_QESDP_SPECIALIZATION_DTOR_WITH_EXPORT(AudioBufferPrivate, QZ_MULTIMEDIA_EXPORT)

class QZ_MULTIMEDIA_EXPORT AudioBuffer
{
public:
    AudioBuffer() noexcept;
    AudioBuffer(const AudioBuffer &other) noexcept;
    AudioBuffer(const QByteArray &data, const AudioFormat &format, qint64 startTime = -1);
    AudioBuffer(int numFrames, const AudioFormat &format, qint64 startTime = -1);
    ~AudioBuffer();

    AudioBuffer& operator=(const AudioBuffer &other);

    AudioBuffer(AudioBuffer &&other) noexcept = default;
    QT_MOVE_ASSIGNMENT_OPERATOR_IMPL_VIA_PURE_SWAP(AudioBuffer)
    void swap(AudioBuffer &other) noexcept
    { d.swap(other.d); }

    // 是否有效
    bool isValid() const noexcept { return d != nullptr; };

    void detach();

    // 获取音频格式
    AudioFormat format() const noexcept;

    // 获取帧计数
    qsizetype frameCount() const noexcept;
    // 获取采样计数
    qsizetype sampleCount() const noexcept;
    // 获取字节计数
    qsizetype byteCount() const noexcept;

    // 获取持续时间（微秒）
    qint64 duration() const noexcept;
    // 获取起始时间（微秒）
    qint64 startTime() const noexcept;

    typedef AudioFrameMono<AudioFormat::UInt8> U8M;
    typedef AudioFrameMono<AudioFormat::Int16> S16M;
    typedef AudioFrameMono<AudioFormat::Int32> S32M;
    typedef AudioFrameMono<AudioFormat::Float> F32M;

    typedef AudioFrameStereo<AudioFormat::UInt8> U8S;
    typedef AudioFrameStereo<AudioFormat::Int16> S16S;
    typedef AudioFrameStereo<AudioFormat::Int32> S32S;
    typedef AudioFrameStereo<AudioFormat::Float> F32S;

    template <typename T> const T* constData() const {
        return static_cast<const T*>(constData());
    }
    template <typename T> const T* data() const {
        return static_cast<const T*>(data());
    }
    template <typename T> T* data() {
        return static_cast<T*>(data());
    }
private:
    const void* constData() const noexcept;
    const void* data() const noexcept;
    void *data();

    QExplicitlySharedDataPointer<AudioBufferPrivate> d;
};

Q_DECLARE_METATYPE(AudioBuffer)

#endif
