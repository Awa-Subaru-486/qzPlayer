// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_VIDEO_VIDEOFRAMEFORMAT_H
#define QT_VIDEO_VIDEOFRAMEFORMAT_H
#include <qzMultimedia/MultimediaGlobal.h>
#include <qzMultimedia/QtVideo.h>

#include <QtCore/qlist.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qsize.h>
#include <QtGui/qimage.h>

class QDebug;

class VideoFrameFormatPrivate;
class VideoFrame;
class QMatrix4x4;

QT_DECLARE_QESDP_SPECIALIZATION_DTOR_WITH_EXPORT(VideoFrameFormatPrivate, QZ_MULTIMEDIA_EXPORT)

// 视频帧格式：描述视频帧的像素格式、尺寸、色彩空间等参数
class QZ_MULTIMEDIA_EXPORT VideoFrameFormat
{
    Q_GADGET
public:
    // 像素格式枚举：支持各种 YUV/RGB 格式
    enum PixelFormat
    {
        Format_Invalid,
        Format_ARGB8888,
        Format_ARGB8888_Premultiplied,
        Format_XRGB8888,
        Format_BGRA8888,
        Format_BGRA8888_Premultiplied,
        Format_BGRX8888,
        Format_ABGR8888,
        Format_XBGR8888,
        Format_RGBA8888,
        Format_RGBX8888,

        Format_AYUV,
        Format_AYUV_Premultiplied,
        Format_YUV420P,
        Format_YUV422P,
        Format_YV12,
        Format_UYVY,
        Format_YUYV,
        Format_NV12,
        Format_NV21,
        Format_IMC1,
        Format_IMC2,
        Format_IMC3,
        Format_IMC4,
        Format_Y8,
        Format_Y16,

        Format_P010,
        Format_P016,

        Format_SamplerExternalOES,
        Format_Jpeg,
        Format_SamplerRect,

        Format_YUV420P10
    };
    Q_ENUM(PixelFormat)
#ifndef Q_QDOC
    static constexpr int NPixelFormats = Format_YUV420P10 + 1;
#endif

    // 扫描线方向
    enum Direction
    {
        TopToBottom,
        BottomToTop
    };

#if QT_DEPRECATED_SINCE(6, 4)
    // 已废弃的 YCbCr 色彩空间
    enum YCbCrColorSpace
    {
        YCbCr_Undefined = 0,
        YCbCr_BT601 = 1,
        YCbCr_BT709 = 2,
        YCbCr_xvYCC601 = 3,
        YCbCr_xvYCC709 = 4,
        YCbCr_JPEG = 5,
        YCbCr_BT2020 = 6
    };
#endif

    // 色彩空间
    enum ColorSpace
    {
        ColorSpace_Undefined = 0,
        ColorSpace_BT601 = 1,
        ColorSpace_BT709 = 2,
        ColorSpace_AdobeRgb = 5,
        ColorSpace_BT2020 = 6
    };

    // 色彩传输函数
    enum ColorTransfer
    {
        ColorTransfer_Unknown,
        ColorTransfer_BT709,
        ColorTransfer_BT601,
        ColorTransfer_Linear,
        ColorTransfer_Gamma22,
        ColorTransfer_Gamma28,
        ColorTransfer_ST2084,
        ColorTransfer_STD_B67,
    };

    // 色彩范围
    enum ColorRange
    {
        ColorRange_Unknown,
        ColorRange_Video,
        ColorRange_Full
    };

    VideoFrameFormat();
    VideoFrameFormat(const QSize &size, PixelFormat pixelFormat);
    VideoFrameFormat(const VideoFrameFormat &format);
    ~VideoFrameFormat();

    VideoFrameFormat(VideoFrameFormat &&other) noexcept = default;
    QT_MOVE_ASSIGNMENT_OPERATOR_IMPL_VIA_PURE_SWAP(VideoFrameFormat);
    void swap(VideoFrameFormat &other) noexcept
    { d.swap(other.d); }

    void detach();

    VideoFrameFormat &operator=(const VideoFrameFormat &format);

    bool operator==(const VideoFrameFormat &format) const;
    bool operator!=(const VideoFrameFormat &format) const;

    // 是否有效
    bool isValid() const;

    // 像素格式
    VideoFrameFormat::PixelFormat pixelFormat() const;

    // 帧尺寸
    QSize frameSize() const;
    void setFrameSize(const QSize &size);
    void setFrameSize(int width, int height);

    int frameWidth() const;
    int frameHeight() const;

    // 平面数量
    int planeCount() const;

    // 视口
    QRect viewport() const;
    void setViewport(const QRect &viewport);

    // 扫描线方向
    Direction scanLineDirection() const;
    void setScanLineDirection(Direction direction);

#if QT_DEPRECATED_SINCE(6, 8)
    QT_DEPRECATED_VERSION_X_6_8("Use streamFrameRate()")
    qreal frameRate() const;
    QT_DEPRECATED_VERSION_X_6_8("Use setStreamFrameRate()")
    void setFrameRate(qreal rate);
#endif

    // 流帧率
    qreal streamFrameRate() const;
    void setStreamFrameRate(qreal rate);

#if QT_DEPRECATED_SINCE(6, 4)
    QT_DEPRECATED_VERSION_X_6_4("Use colorSpace()")
    YCbCrColorSpace yCbCrColorSpace() const;
    QT_DEPRECATED_VERSION_X_6_4("Use setColorSpace()")
    void setYCbCrColorSpace(YCbCrColorSpace colorSpace);
#endif

    // 色彩空间
    ColorSpace colorSpace() const;
    void setColorSpace(ColorSpace colorSpace);

    // 色彩传输
    ColorTransfer colorTransfer() const;
    void setColorTransfer(ColorTransfer colorTransfer);

    // 色彩范围
    ColorRange colorRange() const;
    void setColorRange(ColorRange range);

    // 镜像和旋转
    bool isMirrored() const;
    void setMirrored(bool mirrored);

    QtVideo::Rotation rotation() const;
    void setRotation(QtVideo::Rotation rotation);

    // 着色器文件名
    QString vertexShaderFileName() const;
    QString fragmentShaderFileName() const;
    void updateUniformData(QByteArray *dst, const VideoFrame &frame, const QMatrix4x4 &transform, float opacity) const;

    // 最大亮度（HDR）
    float maxLuminance() const;
    void setMaxLuminance(float lum);

    // 格式转换工具
    static PixelFormat pixelFormatFromImageFormat(QImage::Format format);
    static QImage::Format imageFormatFromPixelFormat(PixelFormat format);

    static QString pixelFormatToString(VideoFrameFormat::PixelFormat pixelFormat);

private:
    QExplicitlySharedDataPointer<VideoFrameFormatPrivate> d;
};

Q_DECLARE_SHARED(VideoFrameFormat)

#ifndef QT_NO_DEBUG_STREAM
QZ_MULTIMEDIA_EXPORT QDebug operator<<(QDebug, const VideoFrameFormat &);
QZ_MULTIMEDIA_EXPORT QDebug operator<<(QDebug, VideoFrameFormat::Direction);
#if QT_DEPRECATED_SINCE(6, 4)
QT_DEPRECATED_VERSION_X_6_4("Use VideoFrameFormat::ColorSpace")
QZ_MULTIMEDIA_EXPORT QDebug operator<<(QDebug, VideoFrameFormat::YCbCrColorSpace);
#endif
QZ_MULTIMEDIA_EXPORT QDebug operator<<(QDebug, VideoFrameFormat::ColorSpace);
QZ_MULTIMEDIA_EXPORT QDebug operator<<(QDebug, VideoFrameFormat::PixelFormat);
#endif

Q_DECLARE_METATYPE(VideoFrameFormat)

#endif
