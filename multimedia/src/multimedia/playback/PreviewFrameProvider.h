// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef PREVIEWFRAMEPROVIDER_H
#define PREVIEWFRAMEPROVIDER_H

#include <qzMultimedia/MultimediaGlobal.h>
#include <qzMultimedia/private/PlatformPreviewFrameProvider_p.h>

#include <QtCore/qobject.h>
#include <QtCore/qurl.h>
#include <QtCore/qsize.h>
#include <QtCore/qfuture.h>

QT_BEGIN_NAMESPACE

// 预览帧提供者：公共 API
// 通过平台后端异步提取视频指定位置的帧，支持 YUV 和 RGBA 格式
class QZ_MULTIMEDIA_EXPORT PreviewFrameProvider : public QObject
{
    Q_OBJECT

public:
    explicit PreviewFrameProvider(QObject *parent = nullptr);
    ~PreviewFrameProvider() override;

    // 设置媒体源
    void setSource(const QUrl &source);

    // 异步请求指定位置（毫秒）的预览帧
    // maxSize 为期望最大尺寸，为空表示原始尺寸
    // 返回 future，完成后携带 PreviewFrameData
    QFuture<PreviewFrameData> requestFrame(qint64 positionMs, const QSize &maxSize = {});

    // 取消所有进行中的请求
    void cancel();

private:
    PlatformPreviewFrameProvider *m_backend = nullptr;
};

QT_END_NAMESPACE

#endif // PREVIEWFRAMEPROVIDER_H
