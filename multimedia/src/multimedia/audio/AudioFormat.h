// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_AUDIOFORMAT_H
#define QT_AUDIO_AUDIOFORMAT_H
#include <QtCore/qobject.h>
#include <QtCore/qshareddata.h>

#include <qzMultimedia/MultimediaGlobal.h>

namespace QtPrivate {
template <typename... Args>
constexpr int channelConfig(Args... values) {
    return (0 | ... | (1u << values));
}
}

// 音频格式：描述音频流的采样率、声道数、采样格式等参数
class AudioFormat
{
public:
    // 采样格式枚举
    enum SampleFormat : quint16 {
        Unknown,
        UInt8,
        Int16,
        Int32,
        Float,
        NSampleFormats
    };

    // 声道位置枚举
    enum AudioChannelPosition {
        UnknownPosition,
        FrontLeft,
        FrontRight,
        FrontCenter,
        LFE,
        BackLeft,
        BackRight,
        FrontLeftOfCenter,
        FrontRightOfCenter,
        BackCenter,
        SideLeft,
        SideRight,
        TopCenter,
        TopFrontLeft,
        TopFrontCenter,
        TopFrontRight,
        TopBackLeft,
        TopBackCenter,
        TopBackRight,
        LFE2,
        TopSideLeft,
        TopSideRight,
        BottomFrontCenter,
        BottomFrontLeft,
        BottomFrontRight
    };
    static constexpr int NChannelPositions = BottomFrontRight + 1;

    // 声道配置枚举
    enum ChannelConfig : quint32 {
        ChannelConfigUnknown = 0,
        ChannelConfigMono = QtPrivate::channelConfig(FrontCenter),
        ChannelConfigStereo = QtPrivate::channelConfig(FrontLeft, FrontRight),
        ChannelConfig2Dot1 = QtPrivate::channelConfig(FrontLeft, FrontRight, LFE),
        ChannelConfig3Dot0 = QtPrivate::channelConfig(FrontLeft, FrontRight, FrontCenter),
        ChannelConfig3Dot1 = QtPrivate::channelConfig(FrontLeft, FrontRight, FrontCenter, LFE),
        ChannelConfigSurround5Dot0 = QtPrivate::channelConfig(FrontLeft, FrontRight, FrontCenter, BackLeft, BackRight),
        ChannelConfigSurround5Dot1 = QtPrivate::channelConfig(FrontLeft, FrontRight, FrontCenter, LFE, BackLeft, BackRight),
        ChannelConfigSurround7Dot0 = QtPrivate::channelConfig(FrontLeft, FrontRight, FrontCenter, BackLeft, BackRight, SideLeft, SideRight),
        ChannelConfigSurround7Dot1 = QtPrivate::channelConfig(FrontLeft, FrontRight, FrontCenter, LFE, BackLeft, BackRight, SideLeft, SideRight),
    };

    // 构建声道配置
    template <typename... Args>
    static constexpr ChannelConfig channelConfig(Args... channels)
    {
        return ChannelConfig(QtPrivate::channelConfig(channels...));
    }

    // 是否有效
    constexpr bool isValid() const noexcept
    {
        return m_sampleRate > 0 && m_channelCount > 0 && m_sampleFormat != Unknown;
    }

    // 采样率
    constexpr void setSampleRate(int sampleRate) noexcept { m_sampleRate = sampleRate; }
    constexpr int sampleRate() const noexcept { return m_sampleRate; }

    // 声道配置
    QZ_MULTIMEDIA_EXPORT void setChannelConfig(ChannelConfig config) noexcept;
    constexpr ChannelConfig channelConfig() const noexcept { return m_channelConfig; }

    // 声道数
    constexpr void setChannelCount(int channelCount) noexcept
    {
        m_channelConfig = ChannelConfigUnknown;
        m_channelCount = short(channelCount);
    }
    constexpr int channelCount() const noexcept { return m_channelCount; }

    // 声道偏移
    QZ_MULTIMEDIA_EXPORT int channelOffset(AudioChannelPosition channel) const noexcept;

    // 采样格式
    constexpr void setSampleFormat(SampleFormat f) noexcept { m_sampleFormat = f; }
    constexpr SampleFormat sampleFormat() const noexcept { return m_sampleFormat; }

    // 字节/帧/时间转换
    QZ_MULTIMEDIA_EXPORT qint32 bytesForDuration(qint64 microseconds) const;
    QZ_MULTIMEDIA_EXPORT qint64 durationForBytes(qint32 byteCount) const;

    QZ_MULTIMEDIA_EXPORT qint32 bytesForFrames(qint32 frameCount) const;
    QZ_MULTIMEDIA_EXPORT qint32 framesForBytes(qint32 byteCount) const;

    QZ_MULTIMEDIA_EXPORT qint32 framesForDuration(qint64 microseconds) const;
    QZ_MULTIMEDIA_EXPORT qint64 durationForFrames(qint32 frameCount) const;

    // 每帧/每采样字节数
    constexpr int bytesPerFrame() const { return bytesPerSample()*channelCount(); }
    constexpr int bytesPerSample() const noexcept
    {
        switch (m_sampleFormat) {
        case Unknown:
        case NSampleFormats: return 0;
        case UInt8: return 1;
        case Int16: return 2;
        case Int32:
        case Float: return 4;
        }
        return 0;
    }

    // 归一化采样值
    QZ_MULTIMEDIA_EXPORT float normalizedSampleValue(const void *sample) const;

    friend bool operator==(const AudioFormat &a, const AudioFormat &b)
    {
        return a.m_sampleRate == b.m_sampleRate &&
               a.m_channelCount == b.m_channelCount &&
               a.m_sampleFormat == b.m_sampleFormat;
    }
    friend bool operator!=(const AudioFormat &a, const AudioFormat &b)
    {
        return !(a == b);
    }

    static QZ_MULTIMEDIA_EXPORT ChannelConfig defaultChannelConfigForChannelCount(int channelCount);

private:
    SampleFormat m_sampleFormat = SampleFormat::Unknown;
    short m_channelCount = 0;
    ChannelConfig m_channelConfig = ChannelConfigUnknown;
    int m_sampleRate = 0;
    Q_DECL_UNUSED_MEMBER quint64 reserved = 0;
};

#ifndef QT_NO_DEBUG_STREAM
QZ_MULTIMEDIA_EXPORT QDebug operator<<(QDebug, const AudioFormat &);
QZ_MULTIMEDIA_EXPORT QDebug operator<<(QDebug, AudioFormat::SampleFormat);
#endif

Q_DECLARE_METATYPE(AudioFormat)

#endif
