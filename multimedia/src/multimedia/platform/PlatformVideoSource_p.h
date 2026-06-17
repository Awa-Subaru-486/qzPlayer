// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_PLATFORM_PLATFORMVIDEOSOURCE_P_H
#define QT_PLATFORM_PLATFORMVIDEOSOURCE_P_H

#include "VideoFrameFormat.h"

#include <QtCore/qobject.h>
#include <QtCore/qnativeinterface.h>
#include <QtCore/private/qglobal_p.h>

#include <optional>

class VideoFrame;
class PlatformMediaCaptureSession;

// 平台视频源抽象接口：摄像头等视频采集
class QZ_MULTIMEDIA_EXPORT PlatformVideoSource : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    // 设置/获取激活状态
    virtual void setActive(bool active) = 0;
    virtual bool isActive() const = 0;

    // 获取帧格式
    virtual VideoFrameFormat frameFormat() const = 0;

    // 获取 FFmpeg 硬件像素格式
    virtual std::optional<int> ffmpegHWPixelFormat() const;

    // 设置采集会话
    virtual void setCaptureSession(PlatformMediaCaptureSession *) { }

    // 获取错误信息
    virtual QString errorString() const = 0;

    bool hasError() const { return !errorString().isEmpty(); }

Q_SIGNALS:
    void newVideoFrame(const VideoFrame &);
    void activeChanged(bool);
    void errorChanged();
};

#endif
