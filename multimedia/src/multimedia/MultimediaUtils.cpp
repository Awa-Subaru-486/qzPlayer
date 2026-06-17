// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "MultimediaUtils_p.h"
#include "VideoFrame.h"
#include "VideoFrameFormat.h"

#include <QtCore/qdir.h>
import qzLog;

#include <cmath>

static qz::Log::LogCategory qLcMultimediaUtils("qz.multimedia.utils");

Fraction qRealToFraction(qreal value)
{
    int integral = int(floor(value));
    value -= qreal(integral);
    if (value == 0.)
        return {integral, 1};

    const int dMax = 1000;
    int n1 = 0, d1 = 1, n2 = 1, d2 = 1;
    qreal mid = 0.;
    while (d1 <= dMax && d2 <= dMax) {
        mid = qreal(n1 + n2) / (d1 + d2);

        if (qAbs(value - mid) < 0.000001) {
            break;
        } else if (value > mid) {
            n1 = n1 + n2;
            d1 = d1 + d2;
        } else {
            n2 = n1 + n2;
            d2 = d1 + d2;
        }
    }

    if (d1 + d2 <= dMax)
        return {n1 + n2 + integral * (d1 + d2), d1 + d2};
    else if (d2 < d1)
        return { n2 + integral * d2, d2 };
    else
        return { n1 + integral * d1, d1 };
}

QSize qCalculateFrameSize(QSize resolution, Fraction par)
{
    if (par.numerator == par.denominator || par.numerator < 1 || par.denominator < 1)
        return resolution;

    if (par.numerator > par.denominator)
        return { resolution.width() * par.numerator / par.denominator, resolution.height() };

    return { resolution.width(), resolution.height() * par.denominator / par.numerator };
}

QSize qRotatedFrameSize(QSize size, int rotation)
{
    Q_ASSERT(rotation % 90 == 0);
    return rotation % 180 ? size.transposed() : size;
}

QSize qRotatedFramePresentationSize(const VideoFrame &frame)
{

    const int rotation = qToUnderlying(frame.rotation()) + qToUnderlying(frame.surfaceFormat().rotation());
    return qRotatedFrameSize(frame.size(), rotation);
}

QUrl qMediaFromUserInput(QUrl url)
{
    return QUrl::fromUserInput(url.toString(), QDir::currentPath(), QUrl::AssumeLocalFile);
}

QRhiSwapChain::Format qGetRequiredSwapChainFormat(const VideoFrameFormat &format)
{
    constexpr auto sdrMaxLuminance = 100.0f;
    const auto formatMaxLuminance = format.maxLuminance();

    return formatMaxLuminance > sdrMaxLuminance ? QRhiSwapChain::HDRExtendedSrgbLinear
                                                : QRhiSwapChain::SDR;
}

bool qShouldUpdateSwapChainFormat(QRhiSwapChain *swapChain,
                                  QRhiSwapChain::Format requiredSwapChainFormat,
                                  PlaybackOptions::HdrPolicy hdrPolicy)
{
    if (!swapChain)
        return false;

    // if (hdrPolicy == PlaybackOptions::HdrPolicy::Disabled) {
    //     return swapChain->format() != QRhiSwapChain::SDR
    //             && swapChain->isFormatSupported(QRhiSwapChain::SDR);
    // }

    return swapChain->format() != requiredSwapChainFormat
            && swapChain->isFormatSupported(requiredSwapChainFormat);
}

QZ_MULTIMEDIA_EXPORT VideoTransformation
qNormalizedSurfaceTransformation(const VideoFrameFormat &format)
{
    VideoTransformation result;
    result.mirrorVertically(format.scanLineDirection() == VideoFrameFormat::BottomToTop);
    result.rotate(format.rotation());
    result.mirrorHorizontally(format.isMirrored());
    return result;
}

VideoTransformation qNormalizedFrameTransformation(const VideoFrame &frame,
                                                   VideoTransformation videoOutputTransformation)
{
    VideoTransformation result = qNormalizedSurfaceTransformation(frame.surfaceFormat());
    result.rotate(frame.rotation());
    result.mirrorHorizontally(frame.mirrored());
    result.rotate(videoOutputTransformation.rotation);
    result.mirrorHorizontally(videoOutputTransformation.mirroredHorizontallyAfterRotation);
    return result;
}

QtVideo::Rotation qVideoRotationFromDegrees(int clockwiseDegrees)
{
    if (clockwiseDegrees % 90 != 0) {
        qz::Log::cat_warn(qLcMultimediaUtils, "qVideoRotationFromAngle(int) received input not divisible by 90. Input was: {}", clockwiseDegrees);
        return QtVideo::Rotation::None;
    }

    int newDegrees = clockwiseDegrees % 360;

    if (newDegrees < 0)
        newDegrees += 360;
    return static_cast<QtVideo::Rotation>(newDegrees);
}

VideoTransformationOpt qVideoTransformationFromMatrix(const QTransform &matrix)
{
    const qreal absScaleX = std::hypot(matrix.m11(), matrix.m12());
    const qreal absScaleY = std::hypot(matrix.m21(), matrix.m22());

    if (qFuzzyIsNull(absScaleX) || qFuzzyIsNull(absScaleY))
        return {};

    qreal cos1 = matrix.m11() / absScaleX;
    qreal sin1 = matrix.m12() / absScaleX;

    const qreal sin2 = -matrix.m21() / absScaleY;
    const qreal cos2 = matrix.m22() / absScaleY;

    VideoTransformation result;

    if (std::abs(cos1) + std::abs(cos2) > std::abs(sin1) + std::abs(sin2))
        result.mirroredHorizontallyAfterRotation = std::signbit(cos1) != std::signbit(cos2);
    else
        result.mirroredHorizontallyAfterRotation = std::signbit(sin1) != std::signbit(sin2);

    if (result.mirroredHorizontallyAfterRotation) {
        cos1 *= -1;
        sin1 *= -1;
    }

    const qreal maxDiscrepancy = 0.2;

    if (std::abs(cos1 - cos2) > maxDiscrepancy || std::abs(sin1 - sin2) > maxDiscrepancy)
        return {};

    const qreal angle = atan2(sin1 + sin2, cos1 + cos2);
    Q_ASSERT(!std::isnan(angle));

    result.rotation = qVideoRotationFromDegrees(qRound(angle / M_PI_2) * 90);
    return result;
}

