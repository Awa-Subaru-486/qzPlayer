// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_PLATFORM_PLATFORMPREVIEWFRAMEPROVIDER_P_H
#define QT_PLATFORM_PLATFORMPREVIEWFRAMEPROVIDER_P_H

#include <QtCore/private/qglobal_p.h>
#include <QtGui/qimage.h>
#include <QtCore/qurl.h>
#include <QtCore/qsize.h>

#include <qzMultimedia/private/MultimediaGlobal_p.h>

#include <memory>
#include <functional>

// 预览帧数据：支持 YUV 多平面和 RGBA 两种格式
// YUV 格式下，planes 持有各平面的数据指针和 linesize，避免 sws_scale 转换
struct QZ_MULTIMEDIA_EXPORT PreviewFrameData
{
    enum class Format
    {
        Invalid,    // 无效
        RGBA,       // 单平面 RGBA8888（QImage 兼容）
        YUV420P,    // 三平面 YUV420P
        NV12        // 双平面 NV12（Y + 交织 UV）
    };

    Format format = Format::Invalid;

    // 原始平面数据（YUV 模式下各平面独立持有；RGBA 模式下仅 planes[0] 有效）
    // 每个平面由独立的 buffer 持有，确保线程安全
    struct Plane
    {
        std::shared_ptr<const uchar> data;  // 共享所有权，引用计数归零时释放
        int linesize = 0;                   // 行跨度（字节）
        int width = 0;                      // 平面宽度（像素）
        int height = 0;                     // 平面高度（像素）
    };

    Plane planes[3];     // 最多 3 个平面（Y/U/V 或 Y/UV/unused）

    int width = 0;       // 图像总宽度
    int height = 0;      // 图像总高度

    bool isValid() const { return format != Format::Invalid && width > 0 && height > 0; }

    // 便捷方法：如果是 RGBA 格式，构造 QImage（零拷贝，共享数据）
    QImage toImage() const
    {
        if (format != Format::RGBA || !planes[0].data)
            return {};
        return QImage(planes[0].data.get(), width, height, planes[0].linesize,
                      QImage::Format_RGBA8888);
    }
};

// 平台预览帧提供者抽象接口
// 后端实现负责根据 source + position 快速提取视频帧
class QZ_MULTIMEDIA_EXPORT PlatformPreviewFrameProvider
{
public:
    virtual ~PlatformPreviewFrameProvider() = default;

    // 设置媒体源
    virtual void setSource(const QUrl &source) = 0;

    // 异步请求指定位置的预览帧
    // maxSize 为期望的最大尺寸（用于缩放优化），为空表示原始尺寸
    // 返回的 PreviewFrameData 通过回调传递，回调在调用线程执行
    virtual void requestFrame(qint64 positionMs, const QSize &maxSize,
                              std::function<void(const PreviewFrameData &)> callback) = 0;

    // 取消所有进行中的请求
    virtual void cancel() = 0;
};

#endif // QT_PLATFORM_PLATFORMPREVIEWFRAMEPROVIDER_P_H
