// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_AUDIOFORMAT_P_H
#define QT_AUDIO_AUDIOFORMAT_P_H

#include <qzMultimedia/MultimediaGlobal.h>
#include <qzMultimedia/AudioFormat.h>

#include <array>

namespace QtMultimediaPrivate {

inline constexpr std::array allSupportedSampleRates{
    8'000,  11'025, 12'000, 16'000, 22'050,  24'000,  32'000,  44'100,
    48'000, 64'000, 88'200, 96'000, 128'000, 176'400, 192'000,
};

inline constexpr std::array allSupportedSampleFormats{
    AudioFormat::UInt8,
    AudioFormat::Int16,
    AudioFormat::Int32,
    AudioFormat::Float,
};

template <typename T>
int findClosestSamplingRate(int rate, std::span<const T> supportedRates)
{
    Q_ASSERT(!supportedRates.empty());

    auto exactMatchIt = std::find(supportedRates.begin(), supportedRates.end(), T(rate));
    if (exactMatchIt != supportedRates.end())
        return int(*exactMatchIt);

    auto ratioToRate = [&](int arg) {
        return arg > rate ? float(arg) / rate : rate / float(arg);
    };

    return *std::min_element(supportedRates.begin(), supportedRates.end(), [&](auto lhs, auto rhs) {
        return ratioToRate(lhs) < ratioToRate(rhs);
    });
}

}

#endif
