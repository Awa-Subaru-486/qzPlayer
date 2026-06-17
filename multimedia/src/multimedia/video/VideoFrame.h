// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_VIDEO_VIDEOFRAME_H
#define QT_VIDEO_VIDEOFRAME_H
#include <qzMultimedia/MultimediaGlobal.h>
#include <qzMultimedia/QtVideo.h>
#include <qzMultimedia/VideoFrameFormat.h>
#include <qzMultimedia/SubtitleStyle.h>

#include <QtCore/qmetatype.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qvector.h>
#include <QtGui/qimage.h>
#include <memory>

class QSize;
class VideoFramePrivate;
class AbstractVideoBuffer;
class QRhi;
class QRhiResourceUpdateBatch;
class QRhiTexture;

// 原始调色板索引位图数据，用于 GPU 端调色板查找渲染
// 使用 shared_ptr 实现跨线程零拷贝传递
// 支持多 rect：多个字幕矩形合并到一张索引纹理中 GPU 渲染
struct QZ_MULTIMEDIA_EXPORT SubtitleBitmapData
{
    using IndexBuffer = std::shared_ptr<const QVector<uint8_t>>;
    using PaletteBuffer = std::shared_ptr<const QVector<uint32_t>>;

    // 单个字幕矩形区域
    struct Rect {
        int x = 0, y = 0, w = 0, h = 0;
        IndexBuffer indexData;                   // 该 rect 的调色板索引位图
    };

    QVector<Rect> rects;                        // 所有字幕矩形
    PaletteBuffer palette;                      // 共享调色板 (最多 256 色, 0xAARRGGBB)
    int nbColors = 0;                           // 调色板颜色数

    // 单 rect 快捷访问（向后兼容）
    int x = 0, y = 0, w = 0, h = 0;

    bool isValid() const { return !rects.isEmpty() && palette && nbColors > 0; }
    bool isEmpty() const { return rects.isEmpty(); }
    int rectCount() const { return rects.size(); }

    // 去重比较：指针相同即为相同数据
    bool isSameData(const SubtitleBitmapData &other) const {
        if (rects.size() != other.rects.size())
            return false;
        for (int i = 0; i < rects.size(); ++i) {
            if (rects[i].indexData.get() != other.rects[i].indexData.get())
                return false;
        }
        return palette.get() == other.palette.get();
    }
};

QT_DECLARE_QESDP_SPECIALIZATION_DTOR_WITH_EXPORT(VideoFramePrivate, QZ_MULTIMEDIA_EXPORT)

// 视频帧：封装一帧视频数据及其格式、时间戳、旋转等元信息
class QZ_MULTIMEDIA_EXPORT VideoFrame
{
    Q_GADGET
public:

    // 句柄类型：无句柄或 RHI 纹理句柄
    enum HandleType
    {
        NoHandle,
        RhiTextureHandle
    };

    // 映射模式：只读、只写、读写
    enum MapMode
    {
        NotMapped = 0x00,
        ReadOnly  = 0x01,
        WriteOnly = 0x02,
        ReadWrite = ReadOnly | WriteOnly
    };
    Q_ENUM(MapMode)

#if QT_DEPRECATED_SINCE(6, 7)
    enum RotationAngle
    {
        Rotation0 Q_DECL_ENUMERATOR_DEPRECATED_X("Use QtVideo::Rotation::None instead") = 0,
        Rotation90 Q_DECL_ENUMERATOR_DEPRECATED_X("Use QtVideo::Rotation::Clockwise90 instead") = 90,
        Rotation180 Q_DECL_ENUMERATOR_DEPRECATED_X("Use QtVideo::Rotation::Clockwise180 instead") = 180,
        Rotation270 Q_DECL_ENUMERATOR_DEPRECATED_X("Use QtVideo::Rotation::Clockwise270 instead") = 270
    };
#endif

    VideoFrame();
    VideoFrame(const VideoFrameFormat &format);
    explicit VideoFrame(const QImage &image);
    explicit VideoFrame(std::unique_ptr<AbstractVideoBuffer> videoBuffer);
    VideoFrame(const VideoFrame &other);
    ~VideoFrame();

    VideoFrame(VideoFrame &&other) noexcept = default;
    QT_MOVE_ASSIGNMENT_OPERATOR_IMPL_VIA_PURE_SWAP(VideoFrame)
    void swap(VideoFrame &other) noexcept
    { d.swap(other.d); }

    VideoFrame &operator =(const VideoFrame &other);
    bool operator==(const VideoFrame &other) const;
    bool operator!=(const VideoFrame &other) const;

    // 是否有效
    bool isValid() const;

    // 像素格式
    VideoFrameFormat::PixelFormat pixelFormat() const;

    // 表面格式
    VideoFrameFormat surfaceFormat() const;
    VideoFrame::HandleType handleType() const;

    // 尺寸
    QSize size() const;
    int width() const;
    int height() const;

    // 映射状态
    bool isMapped() const;
    bool isReadable() const;
    bool isWritable() const;

    VideoFrame::MapMode mapMode() const;

    // 内存映射
    bool map(VideoFrame::MapMode mode);
    void unmap();

    // 平面数据访问
    int bytesPerLine(int plane) const;

    uchar *bits(int plane);
    const uchar *bits(int plane) const;
    int mappedBytes(int plane) const;
    int planeCount() const;

    // 时间戳
    qint64 startTime() const;
    void setStartTime(qint64 time);

    qint64 endTime() const;
    void setEndTime(qint64 time);

#if QT_DEPRECATED_SINCE(6, 7)
    QT_DEPRECATED_VERSION_X_6_7("Use VideoFrame::setRotation(QtVideo::Rotation) instead")
    void setRotationAngle(RotationAngle angle) { setRotation(QtVideo::Rotation(angle)); }

    QT_DEPRECATED_VERSION_X_6_7("Use VideoFrame::rotation() instead")
    RotationAngle rotationAngle() const { return RotationAngle(rotation()); }
#endif

    // 旋转角度
    void setRotation(QtVideo::Rotation angle);
    QtVideo::Rotation rotation() const;

    // 镜像
    void setMirrored(bool);
    bool mirrored() const;

    // 流帧率
    void setStreamFrameRate(qreal rate);
    qreal streamFrameRate() const;

    // 转换为 QImage
    QImage toImage() const;

    // 绘制选项
    struct PaintOptions {
        QColor backgroundColor = Qt::transparent;
        Qt::AspectRatioMode aspectRatioMode = Qt::KeepAspectRatio;
        enum PaintFlag {
            DontDrawSubtitles = 0x1
        };
        Q_DECLARE_FLAGS(PaintFlags, PaintFlag)
        PaintFlags paintFlags = {};
    };

    // 字幕文本
    QString subtitleText() const;
    void setSubtitleText(const QString &text);

    // 字幕位图(PGS等图形字幕)
    QImage subtitleImage() const;
    void setSubtitleImage(const QImage &image);
    QRect subtitleRect() const;
    void setSubtitleRect(const QRect &rect);

    // 字幕原始调色板索引位图(GPU端调色板查找渲染)
    SubtitleBitmapData subtitleBitmapData() const;
    void setSubtitleBitmapData(const SubtitleBitmapData &data);
    bool hasSubtitleBitmapData() const;

    // 字幕样式
    SubtitleStyle subtitleStyle() const;
    void setSubtitleStyle(const SubtitleStyle &style);

    // 绘制到指定区域
    void paint(QPainter *painter, const QRectF &rect, const PaintOptions &options);

#if QT_DEPRECATED_SINCE(6, 8)
    QT_DEPRECATED_VERSION_X_6_8("The constructor is internal and deprecated")
    VideoFrame(AbstractVideoBuffer *buffer, const VideoFrameFormat &format);

    QT_DEPRECATED_VERSION_X_6_8("The method is internal and deprecated")
    AbstractVideoBuffer *videoBuffer() const;
#endif
private:
    friend class VideoFramePrivate;
    QExplicitlySharedDataPointer<VideoFramePrivate> d;
};

Q_DECLARE_SHARED(VideoFrame)

#ifndef QT_NO_DEBUG_STREAM
QZ_MULTIMEDIA_EXPORT QDebug operator<<(QDebug, const VideoFrame&);
QZ_MULTIMEDIA_EXPORT QDebug operator<<(QDebug, VideoFrame::HandleType);
#endif

Q_DECLARE_METATYPE(VideoFrame)

#endif
