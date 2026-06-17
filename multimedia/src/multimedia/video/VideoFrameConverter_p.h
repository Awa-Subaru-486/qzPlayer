// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_VIDEO_VIDEOFRAMECONVERTER_P_H
#define QT_VIDEO_VIDEOFRAMECONVERTER_P_H

#include <VideoFrame.h>

// 视频帧转换器：将视频帧转换为 QImage
struct VideoTransformation;

// 从视频帧创建 QImage
QZ_MULTIMEDIA_EXPORT QImage qImageFromVideoFrame(const VideoFrame &frame,
                                                const VideoTransformation &transformation,
                                                bool forceCpu = false);

QZ_MULTIMEDIA_EXPORT QImage qImageFromVideoFrame(const VideoFrame &frame, bool forceCpu = false);

// 将视频帧平面作为图像
QZ_MULTIMEDIA_EXPORT QImage videoFramePlaneAsImage(VideoFrame &frame, int plane,
                                                  QImage::Format targetFromat, QSize targetSize);

#endif
