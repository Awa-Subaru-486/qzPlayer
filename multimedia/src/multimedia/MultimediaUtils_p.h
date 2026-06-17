// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_MULTIMEDIAUTILS_P_H
#define QT_MULTIMEDIAUTILS_P_H

#include <qzMultimedia/MultimediaGlobal.h>
#include <qzMultimedia/PlaybackOptions.h>
#include <qzMultimedia/private/VideoTransformation_p.h>
#include <QtCore/qsize.h>
#include <QtCore/qurl.h>
#include <QtGui/rhi/qrhi.h>

class QRhiSwapChain;
class VideoFrame;
class VideoFrameFormat;

// 分数结构
struct Fraction {
    int numerator;
    int denominator;
};

// 实数转分数
QZ_MULTIMEDIA_EXPORT Fraction qRealToFraction(qreal value);

// 计算帧尺寸
QZ_MULTIMEDIA_EXPORT QSize qCalculateFrameSize(QSize resolution, Fraction pixelAspectRatio);

// 旋转后的帧尺寸
QZ_MULTIMEDIA_EXPORT QSize qRotatedFrameSize(QSize size, int rotation);

inline QSize qRotatedFrameSize(QSize size, QtVideo::Rotation rotation)
{
    return qRotatedFrameSize(size, qToUnderlying(rotation));
}

// 旋转后的帧呈现尺寸
QZ_MULTIMEDIA_EXPORT QSize qRotatedFramePresentationSize(const VideoFrame &frame);

// 从用户输入构建媒体 URL
QZ_MULTIMEDIA_EXPORT QUrl qMediaFromUserInput(QUrl fileName);

// 获取所需的交换链格式
QZ_MULTIMEDIA_EXPORT QRhiSwapChain::Format
qGetRequiredSwapChainFormat(const VideoFrameFormat &format);

QZ_MULTIMEDIA_EXPORT bool
qShouldUpdateSwapChainFormat(QRhiSwapChain *swapChain,
                             QRhiSwapChain::Format requiredSwapChainFormat,
                             PlaybackOptions::HdrPolicy hdrPolicy);

// 规范化表面变换
QZ_MULTIMEDIA_EXPORT VideoTransformation
qNormalizedSurfaceTransformation(const VideoFrameFormat &format);

// 规范化帧变换
QZ_MULTIMEDIA_EXPORT VideoTransformation qNormalizedFrameTransformation(
        const VideoFrame &frame, VideoTransformation videoOutputTransformation = {});

// 从角度获取视频旋转
QZ_MULTIMEDIA_EXPORT QtVideo::Rotation
qVideoRotationFromDegrees(int clockwiseDegrees);

inline VideoTransformation qNormalizedFrameTransformation(const VideoFrame &frame,
                                                          int videoOutputRotation)
{
    return qNormalizedFrameTransformation(
            frame, VideoTransformation{ qVideoRotationFromDegrees(videoOutputRotation) });
}

// 从矩阵获取视频变换
QZ_MULTIMEDIA_EXPORT VideoTransformationOpt qVideoTransformationFromMatrix(const QTransform &matrix);

#endif
