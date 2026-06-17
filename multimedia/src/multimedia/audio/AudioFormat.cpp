// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "AudioFormat.h"

#include <QtCore/qdebug.h>
#include <qzMultimedia/private/MultimediaAssume_p.h>

void AudioFormat::setChannelConfig(ChannelConfig config) noexcept
{
    m_channelConfig = config;
    if (config != ChannelConfigUnknown)
        m_channelCount = qPopulationCount(config);
}

int AudioFormat::channelOffset(AudioChannelPosition channel) const noexcept
{
    if (!(m_channelConfig & (1u << channel)))
        return -1;

    uint maskedChannels = m_channelConfig & ((1u << channel) - 1);
    return qPopulationCount(maskedChannels);
}

qint32 AudioFormat::bytesForDuration(qint64 microseconds) const
{
    return bytesPerFrame() * framesForDuration(microseconds);
}

qint64 AudioFormat::durationForBytes(qint32 bytes) const
{
    if (!isValid() || bytes <= 0)
        return 0;

    const int bytesPerFrame = this->bytesPerFrame();
    const int sampleRate = this->sampleRate();
    QT_MM_ASSUME(bytesPerFrame > 0);
    QT_MM_ASSUME(sampleRate > 0);
    return qint64(1000000LL * (bytes / bytesPerFrame)) / sampleRate;
}

qint32 AudioFormat::bytesForFrames(qint32 frameCount) const
{
    return frameCount * bytesPerFrame();
}

qint32 AudioFormat::framesForBytes(qint32 byteCount) const
{
    int size = bytesPerFrame();

    switch (size) {
    case 0:
        return 0;
    case 1:
        return byteCount;
    case 2:
        return byteCount / 2;
    case 4:
        return byteCount / 4;
    case 8:
        return byteCount / 8;
    case 16:
        return byteCount / 16;
    default:
        return byteCount / size;
    }
}

qint32 AudioFormat::framesForDuration(qint64 microseconds) const
{
    if (!isValid())
        return 0;

    return qint32((microseconds * sampleRate()) / 1000000LL);
}

qint64 AudioFormat::durationForFrames(qint32 frameCount) const
{
    if (!isValid() || frameCount <= 0)
        return 0;

    return (frameCount * 1000000LL) / sampleRate();
}

float AudioFormat::normalizedSampleValue(const void *sample) const
{
    switch (m_sampleFormat) {
    case UInt8:
        return ((float)*reinterpret_cast<const quint8 *>(sample))
                / (float)std::numeric_limits<qint8>::max()
                - 1.;
    case Int16:
        return ((float)*reinterpret_cast<const qint16 *>(sample))
                / (float)std::numeric_limits<qint16>::max();
    case Int32:
        return ((float)*reinterpret_cast<const qint32 *>(sample))
                / (float)std::numeric_limits<qint32>::max();
    case Float:
        return *reinterpret_cast<const float *>(sample);
    case Unknown:
    case NSampleFormats:
        break;
    }

    return 0.;
}

AudioFormat::ChannelConfig AudioFormat::defaultChannelConfigForChannelCount(int channelCount)
{
    AudioFormat::ChannelConfig config;
    switch (channelCount) {
    case 0:
        config = AudioFormat::ChannelConfigUnknown;
        break;
    case 1:
        config = AudioFormat::ChannelConfigMono;
        break;
    case 2:
        config = AudioFormat::ChannelConfigStereo;
        break;
    case 3:
        config = AudioFormat::ChannelConfig2Dot1;
        break;
    case 4:
        config = AudioFormat::channelConfig(AudioFormat::FrontLeft, AudioFormat::FrontRight,
                                             AudioFormat::BackLeft, AudioFormat::BackRight);
        break;
    case 5:
        config = AudioFormat::ChannelConfigSurround5Dot0;
        break;
    case 6:
        config = AudioFormat::ChannelConfigSurround5Dot1;
        break;
    case 7:
        config = AudioFormat::ChannelConfigSurround7Dot0;
        break;
    case 8:
        config = AudioFormat::ChannelConfigSurround7Dot1;
        break;
    default:

        config = AudioFormat::ChannelConfig(((1 << channelCount) - 1) << 1);
    }
    return config;
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, AudioFormat::SampleFormat type)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    switch (type) {
    case AudioFormat::UInt8:
        dbg << "UInt8";
        break;
    case AudioFormat::Int16:
        dbg << "Int16";
        break;
    case AudioFormat::Int32:
        dbg << "Int32";
        break;
    case AudioFormat::Float:
        dbg << "Float";
        break;
    default:
        dbg << "Unknown";
        break;
    }
    return dbg;
}

QDebug operator<<(QDebug dbg, const AudioFormat &f)
{
    QDebugStateSaver s(dbg);
    dbg.nospace();
    dbg << "AudioFormat(" << f.sampleRate() << "Hz, " << f.channelCount() << " Channels, "
        << f.sampleFormat() << "Format)";
    return dbg;
}
#endif

