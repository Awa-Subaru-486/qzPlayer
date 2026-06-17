// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_AUDIOHELPERS_P_H
#define QT_AUDIO_AUDIOHELPERS_P_H

#include <span>
#include <qzMultimedia/AudioFormat.h>
#include <qzMultimedia/private/MultimediaGlobal_p.h>
#include <qzMultimedia/private/AudioRtsanSupport_p.h>

namespace AudioHelperInternal {
QZ_MULTIMEDIA_EXPORT void qMultiplySamples(float factor,
                                          const AudioFormat &format,
                                          const void *src,
                                          void *dest,
                                          int len) noexcept QT_MM_NONBLOCKING;

QZ_MULTIMEDIA_EXPORT
void applyVolume(float volume,
                 const AudioFormat &,
                 std::span<const std::byte> source,
                 std::span<std::byte> destination) noexcept QT_MM_NONBLOCKING;

enum class NativeSampleFormat : uint8_t {
    uint8_t,
    int16_t,
    int32_t,
    int24_t_3b,
    int24_t_4b_low,
    float32_t,
};

QZ_MULTIMEDIA_EXPORT
void convertSampleFormat(std::span<const std::byte> source, NativeSampleFormat sourceFormat,
                         std::span<std::byte> destination,
                         NativeSampleFormat destinationFormat) noexcept QT_MM_NONBLOCKING;

QZ_MULTIMEDIA_EXPORT
NativeSampleFormat bestNativeSampleFormat(const AudioFormat &fmt,
                                          std::span<const NativeSampleFormat> supportedNativeFormats);
AudioFormat::SampleFormat bestSampleFormat(NativeSampleFormat);

NativeSampleFormat toNativeSampleFormat(AudioFormat::SampleFormat);

constexpr size_t bytesPerSample(NativeSampleFormat fmt) noexcept QT_MM_NONBLOCKING
{
    switch (fmt) {
    case NativeSampleFormat::uint8_t:
        return 1;
    case NativeSampleFormat::int16_t:
        return 2;
    case NativeSampleFormat::int24_t_3b:
        return 3;
    case NativeSampleFormat::int24_t_4b_low:
    case NativeSampleFormat::float32_t:
    case NativeSampleFormat::int32_t:
        return 4;
    default:
        Q_UNREACHABLE_RETURN(0);
    }
}

std::optional<float> sanitizeVolume(float volume, float lastVolume);

}

QZ_MULTIMEDIA_EXPORT
QDebug operator<<(QDebug dbg, AudioHelperInternal::NativeSampleFormat);

#endif
