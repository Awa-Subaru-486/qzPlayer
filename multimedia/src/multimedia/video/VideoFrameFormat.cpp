// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "VideoFrameFormat.h"

#include <qzMultimedia/private/VideoTextureHelper_p.h>
#include <qzMultimedia/private/VideoTransformation_p.h>
#include <qzMultimedia/private/MultimediaEnumToStringConverter_p.h>

#include <algorithm>

#include <qdebug.h>
#include <qlist.h>
#include <qmetatype.h>
#include <qvariant.h>
#include <qmatrix4x4.h>

static void initResource() {
    Q_INIT_RESOURCE(qtmultimedia_shaders);
}

class VideoFrameFormatPrivate : public QSharedData
{
public:
    VideoFrameFormatPrivate() = default;

    VideoFrameFormatPrivate(
            const QSize &size,
            VideoFrameFormat::PixelFormat format)
        : pixelFormat(format)
        , frameSize(size)
        , viewport(QPoint(0, 0), size)
    {
    }

    bool operator ==(const VideoFrameFormatPrivate &other) const
    {
        if (pixelFormat == other.pixelFormat && scanLineDirection == other.scanLineDirection
            && frameSize == other.frameSize && viewport == other.viewport
            && frameRatesEqual(frameRate, other.frameRate) && colorSpace == other.colorSpace
            && transformation == other.transformation)
            return true;

        return false;
    }

    inline static bool frameRatesEqual(qreal r1, qreal r2)
    {
        return qAbs(r1 - r2) <= 0.00001 * qMin(qAbs(r1), qAbs(r2));
    }

    VideoFrameFormat::PixelFormat pixelFormat = VideoFrameFormat::Format_Invalid;
    VideoFrameFormat::Direction scanLineDirection = VideoFrameFormat::TopToBottom;
    QSize frameSize;
    VideoFrameFormat::ColorSpace colorSpace = VideoFrameFormat::ColorSpace_Undefined;
    VideoFrameFormat::ColorTransfer colorTransfer = VideoFrameFormat::ColorTransfer_Unknown;
    VideoFrameFormat::ColorRange colorRange = VideoFrameFormat::ColorRange_Unknown;
    QRect viewport;
    float frameRate = 0.0;
    float maxLuminance = -1.;
    VideoTransformation transformation;
};

QT_DEFINE_QESDP_SPECIALIZATION_DTOR(VideoFrameFormatPrivate);

VideoFrameFormat::VideoFrameFormat()
    : d(new VideoFrameFormatPrivate)
{
    initResource();
}

VideoFrameFormat::VideoFrameFormat(
        const QSize& size, VideoFrameFormat::PixelFormat format)
    : d(new VideoFrameFormatPrivate(size, format))
{
}

VideoFrameFormat::VideoFrameFormat(const VideoFrameFormat &other) = default;

VideoFrameFormat &VideoFrameFormat::operator =(const VideoFrameFormat &other) = default;

VideoFrameFormat::~VideoFrameFormat() = default;

bool VideoFrameFormat::isValid() const
{
    return d->pixelFormat != Format_Invalid && d->frameSize.isValid();
}

bool VideoFrameFormat::operator ==(const VideoFrameFormat &other) const
{
    return d == other.d || *d == *other.d;
}

bool VideoFrameFormat::operator !=(const VideoFrameFormat &other) const
{
    return d != other.d && !(*d == *other.d);
}

void VideoFrameFormat::detach()
{
    d.detach();
}

VideoFrameFormat::PixelFormat VideoFrameFormat::pixelFormat() const
{
    return d->pixelFormat;
}

QSize VideoFrameFormat::frameSize() const
{
    return d->frameSize;
}

int VideoFrameFormat::frameWidth() const
{
    return d->frameSize.width();
}

int VideoFrameFormat::frameHeight() const
{
    return d->frameSize.height();
}

int VideoFrameFormat::planeCount() const
{
    return VideoTextureHelper::textureDescription(d->pixelFormat)->nplanes;
}

void VideoFrameFormat::setFrameSize(const QSize &size)
{
    detach();
    d->frameSize = size;
    d->viewport = QRect(QPoint(0, 0), size);
}

void VideoFrameFormat::setFrameSize(int width, int height)
{
    detach();
    d->frameSize = QSize(width, height);
    d->viewport = QRect(0, 0, width, height);
}

QRect VideoFrameFormat::viewport() const
{
    return d->viewport;
}

void VideoFrameFormat::setViewport(const QRect &viewport)
{
    detach();
    d->viewport = viewport;
}

VideoFrameFormat::Direction VideoFrameFormat::scanLineDirection() const
{
    return d->scanLineDirection;
}

void VideoFrameFormat::setScanLineDirection(Direction direction)
{
    detach();
    d->scanLineDirection = direction;
}

#if QT_DEPRECATED_SINCE(6, 8)

qreal VideoFrameFormat::frameRate() const
{
    return streamFrameRate();
}

void VideoFrameFormat::setFrameRate(qreal rate)
{
    setStreamFrameRate(rate);
}
#endif

qreal VideoFrameFormat::streamFrameRate() const
{
    return d->frameRate;
}

void VideoFrameFormat::setStreamFrameRate(qreal rate)
{
    detach();
    d->frameRate = rate;
}

#if QT_DEPRECATED_SINCE(6, 4)

VideoFrameFormat::YCbCrColorSpace VideoFrameFormat::yCbCrColorSpace() const
{
    return YCbCrColorSpace(d->colorSpace);
}

void VideoFrameFormat::setYCbCrColorSpace(VideoFrameFormat::YCbCrColorSpace space)
{
    detach();
    d->colorSpace = ColorSpace(space);
}
#endif

VideoFrameFormat::ColorSpace VideoFrameFormat::colorSpace() const
{
    return d->colorSpace;
}

void VideoFrameFormat::setColorSpace(ColorSpace colorSpace)
{
    detach();
    d->colorSpace = colorSpace;
}

VideoFrameFormat::ColorTransfer VideoFrameFormat::colorTransfer() const
{
    return d->colorTransfer;
}

void VideoFrameFormat::setColorTransfer(ColorTransfer colorTransfer)
{
    detach();
    d->colorTransfer = colorTransfer;
}

VideoFrameFormat::ColorRange VideoFrameFormat::colorRange() const
{
    return d->colorRange;
}

void VideoFrameFormat::setColorRange(ColorRange range)
{
    detach();
    d->colorRange = range;
}

bool VideoFrameFormat::isMirrored() const
{
    return d->transformation.mirroredHorizontallyAfterRotation;
}

void VideoFrameFormat::setMirrored(bool mirrored)
{
    detach();
    d->transformation.mirroredHorizontallyAfterRotation = mirrored;
}

QtVideo::Rotation VideoFrameFormat::rotation() const
{
    return d->transformation.rotation;
}

void VideoFrameFormat::setRotation(QtVideo::Rotation angle)
{
    detach();
    d->transformation.rotation = angle;
}

QString VideoFrameFormat::vertexShaderFileName() const
{
    return VideoTextureHelper::vertexShaderFileName(*this);
}

QString VideoFrameFormat::fragmentShaderFileName() const
{
    return VideoTextureHelper::fragmentShaderFileName(*this, nullptr);
}

void VideoFrameFormat::updateUniformData(QByteArray *dst, const VideoFrame &frame, const QMatrix4x4 &transform, float opacity) const
{
    VideoTextureHelper::updateUniformData(dst, nullptr, *this, frame, transform, opacity);
}

float VideoFrameFormat::maxLuminance() const
{
    if (d->maxLuminance <= 0) {
        if (d->colorTransfer == ColorTransfer_ST2084)
            return 10000.;
        if (d->colorTransfer == ColorTransfer_STD_B67)
            return 1500.;
        return 100;
    }

    float clamped = d->maxLuminance;
    if (d->colorTransfer == ColorTransfer_ST2084)
        clamped = std::min(clamped, 10000.f);
    else if (d->colorTransfer == ColorTransfer_STD_B67)
        clamped = std::min(clamped, 4000.f);
    else
        clamped = std::min(clamped, 100.f);

    return clamped;
}

void VideoFrameFormat::setMaxLuminance(float lum)
{
    detach();
    d->maxLuminance = lum;
}

VideoFrameFormat::PixelFormat VideoFrameFormat::pixelFormatFromImageFormat(QImage::Format format)
{
    switch (format) {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    case QImage::Format_RGB32:
        return VideoFrameFormat::Format_BGRX8888;
    case QImage::Format_ARGB32:
        return VideoFrameFormat::Format_BGRA8888;
    case QImage::Format_ARGB32_Premultiplied:
        return VideoFrameFormat::Format_BGRA8888_Premultiplied;
#else
    case QImage::Format_RGB32:
        return VideoFrameFormat::Format_XRGB8888;
    case QImage::Format_ARGB32:
        return VideoFrameFormat::Format_ARGB8888;
    case QImage::Format_ARGB32_Premultiplied:
        return VideoFrameFormat::Format_ARGB8888_Premultiplied;
#endif
    case QImage::Format_RGBA8888:
        return VideoFrameFormat::Format_RGBA8888;
    case QImage::Format_RGBA8888_Premultiplied:

        return VideoFrameFormat::Format_RGBX8888;
    case QImage::Format_RGBX8888:
        return VideoFrameFormat::Format_RGBX8888;
    case QImage::Format_Grayscale8:
        return VideoFrameFormat::Format_Y8;
    case QImage::Format_Grayscale16:
        return VideoFrameFormat::Format_Y16;
    default:
        return VideoFrameFormat::Format_Invalid;
    }
}

QImage::Format VideoFrameFormat::imageFormatFromPixelFormat(VideoFrameFormat::PixelFormat format)
{
    switch (format) {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    case VideoFrameFormat::Format_BGRA8888:
        return QImage::Format_ARGB32;
    case VideoFrameFormat::Format_BGRA8888_Premultiplied:
        return QImage::Format_ARGB32_Premultiplied;
    case VideoFrameFormat::Format_BGRX8888:
        return QImage::Format_RGB32;
    case VideoFrameFormat::Format_ARGB8888:
    case VideoFrameFormat::Format_ARGB8888_Premultiplied:
    case VideoFrameFormat::Format_XRGB8888:
        return QImage::Format_Invalid;
#else
    case VideoFrameFormat::Format_ARGB8888:
        return QImage::Format_ARGB32;
    case VideoFrameFormat::Format_ARGB8888_Premultiplied:
        return QImage::Format_ARGB32_Premultiplied;
    case VideoFrameFormat::Format_XRGB8888:
        return QImage::Format_RGB32;
    case VideoFrameFormat::Format_BGRA8888:
    case VideoFrameFormat::Format_BGRA8888_Premultiplied:
    case VideoFrameFormat::Format_BGRX8888:
        return QImage::Format_Invalid;
#endif
    case VideoFrameFormat::Format_RGBA8888:
        return QImage::Format_RGBA8888;
    case VideoFrameFormat::Format_RGBX8888:
        return QImage::Format_RGBX8888;
    case VideoFrameFormat::Format_Y8:
        return QImage::Format_Grayscale8;
    case VideoFrameFormat::Format_Y16:
        return QImage::Format_Grayscale16;
    case VideoFrameFormat::Format_ABGR8888:
    case VideoFrameFormat::Format_XBGR8888:
    case VideoFrameFormat::Format_AYUV:
    case VideoFrameFormat::Format_AYUV_Premultiplied:
    case VideoFrameFormat::Format_YUV420P:
    case VideoFrameFormat::Format_YUV420P10:
    case VideoFrameFormat::Format_YUV422P:
    case VideoFrameFormat::Format_YV12:
    case VideoFrameFormat::Format_UYVY:
    case VideoFrameFormat::Format_YUYV:
    case VideoFrameFormat::Format_NV12:
    case VideoFrameFormat::Format_NV21:
    case VideoFrameFormat::Format_IMC1:
    case VideoFrameFormat::Format_IMC2:
    case VideoFrameFormat::Format_IMC3:
    case VideoFrameFormat::Format_IMC4:
    case VideoFrameFormat::Format_P010:
    case VideoFrameFormat::Format_P016:
    case VideoFrameFormat::Format_Jpeg:
    case VideoFrameFormat::Format_Invalid:
    case VideoFrameFormat::Format_SamplerExternalOES:
    case VideoFrameFormat::Format_SamplerRect:
        return QImage::Format_Invalid;
    }
    return QImage::Format_Invalid;
}

QT_MM_MAKE_STRING_RESOLVER(VideoFrameFormat::PixelFormat, QtMultimediaPrivate::EnumName,
    (VideoFrameFormat::Format_Invalid,                 "Invalid")
    (VideoFrameFormat::Format_ARGB8888,                "ARGB8888")
    (VideoFrameFormat::Format_ARGB8888_Premultiplied,  "ARGB8888 Premultiplied")
    (VideoFrameFormat::Format_XRGB8888,                "XRGB8888")
    (VideoFrameFormat::Format_BGRA8888,                "BGRA8888")
    (VideoFrameFormat::Format_BGRX8888,                "BGRX8888")
    (VideoFrameFormat::Format_BGRA8888_Premultiplied,  "BGRA8888 Premultiplied")
    (VideoFrameFormat::Format_RGBA8888,                "RGBA8888")
    (VideoFrameFormat::Format_RGBX8888,                "RGBX8888")
    (VideoFrameFormat::Format_ABGR8888,                "ABGR8888")
    (VideoFrameFormat::Format_XBGR8888,                "XBGR8888")
    (VideoFrameFormat::Format_AYUV,                    "AYUV")
    (VideoFrameFormat::Format_AYUV_Premultiplied,      "AYUV Premultiplied")
    (VideoFrameFormat::Format_YUV420P,                 "YUV420P")
    (VideoFrameFormat::Format_YUV420P10,               "YUV420P10")
    (VideoFrameFormat::Format_YUV422P,                 "YUV422P")
    (VideoFrameFormat::Format_YV12,                    "YV12")
    (VideoFrameFormat::Format_UYVY,                    "UYVY")
    (VideoFrameFormat::Format_YUYV,                    "YUYV")
    (VideoFrameFormat::Format_NV12,                    "NV12")
    (VideoFrameFormat::Format_NV21,                    "NV21")
    (VideoFrameFormat::Format_IMC1,                    "IMC1")
    (VideoFrameFormat::Format_IMC2,                    "IMC2")
    (VideoFrameFormat::Format_IMC3,                    "IMC3")
    (VideoFrameFormat::Format_IMC4,                    "IMC4")
    (VideoFrameFormat::Format_Y8,                      "Y8")
    (VideoFrameFormat::Format_Y16,                     "Y16")
    (VideoFrameFormat::Format_P010,                    "P010")
    (VideoFrameFormat::Format_P016,                    "P016")
    (VideoFrameFormat::Format_SamplerExternalOES,      "SamplerExternalOES")
    (VideoFrameFormat::Format_Jpeg,                    "Jpeg")
    (VideoFrameFormat::Format_SamplerRect,             "SamplerRect")
);

#ifndef QT_NO_DEBUG_STREAM
# if QT_DEPRECATED_SINCE(6, 4)
QT_MM_MAKE_STRING_RESOLVER(VideoFrameFormat::YCbCrColorSpace, QtMultimediaPrivate::EnumName,
    (VideoFrameFormat::YCbCr_Undefined,   "YCbCr_Undefined")
    (VideoFrameFormat::YCbCr_BT601,       "YCbCr_BT601")
    (VideoFrameFormat::YCbCr_BT709,       "YCbCr_BT709")
    (VideoFrameFormat::YCbCr_xvYCC601,    "YCbCr_xvYCC601")
    (VideoFrameFormat::YCbCr_xvYCC709,    "YCbCr_xvYCC709")
    (VideoFrameFormat::YCbCr_JPEG,        "YCbCr_JPEG")
    (VideoFrameFormat::YCbCr_BT2020,      "YCbCr_BT2020")
);
QT_MM_DEFINE_QDEBUG_ENUM(VideoFrameFormat::YCbCrColorSpace);
# endif

QT_MM_MAKE_STRING_RESOLVER(VideoFrameFormat::ColorSpace, QtMultimediaPrivate::EnumName,
                           (VideoFrameFormat::ColorSpace_BT601,     "ColorSpace_BT601")
                           (VideoFrameFormat::ColorSpace_BT709,     "ColorSpace_BT709")
                           (VideoFrameFormat::ColorSpace_AdobeRgb,  "ColorSpace_AdobeRgb")
                           (VideoFrameFormat::ColorSpace_BT2020,    "ColorSpace_BT2020")
                           (VideoFrameFormat::ColorSpace_Undefined, "ColorSpace_Undefined")
                           );
QT_MM_DEFINE_QDEBUG_ENUM(VideoFrameFormat::ColorSpace);

QT_MM_MAKE_STRING_RESOLVER(VideoFrameFormat::ColorTransfer, QtMultimediaPrivate::EnumName,
    (VideoFrameFormat::ColorTransfer_Unknown,    "ColorTransfer_Unknown")
    (VideoFrameFormat::ColorTransfer_BT709,      "ColorTransfer_BT709")
    (VideoFrameFormat::ColorTransfer_BT601,      "ColorTransfer_BT601")
    (VideoFrameFormat::ColorTransfer_Linear,     "ColorTransfer_Linear")
    (VideoFrameFormat::ColorTransfer_Gamma22,    "ColorTransfer_Gamma22")
    (VideoFrameFormat::ColorTransfer_Gamma28,    "ColorTransfer_Gamma28")
    (VideoFrameFormat::ColorTransfer_ST2084,     "ColorTransfer_ST2084")
    (VideoFrameFormat::ColorTransfer_STD_B67,    "ColorTransfer_STD_B67")
);
QT_MM_DEFINE_QDEBUG_ENUM(VideoFrameFormat::ColorTransfer);

QT_MM_MAKE_STRING_RESOLVER(VideoFrameFormat::ColorRange, QtMultimediaPrivate::EnumName,
    (VideoFrameFormat::ColorRange_Unknown,   "ColorRange_Unknown")
    (VideoFrameFormat::ColorRange_Video,     "ColorRange_Video")
    (VideoFrameFormat::ColorRange_Full,      "ColorRange_Full")
);
QT_MM_DEFINE_QDEBUG_ENUM(VideoFrameFormat::ColorRange);

QT_MM_MAKE_STRING_RESOLVER(VideoFrameFormat::Direction, QtMultimediaPrivate::EnumName,
    (VideoFrameFormat::TopToBottom,    "TopToBottom")
    (VideoFrameFormat::BottomToTop,    "BottomToTop")
);
QT_MM_DEFINE_QDEBUG_ENUM(VideoFrameFormat::Direction);

QZ_MULTIMEDIA_EXPORT QString VideoFrameFormat::pixelFormatToString(VideoFrameFormat::PixelFormat pixelFormat)
{
    auto str = QtMultimediaPrivate::StringResolver<VideoFrameFormat::PixelFormat>::toQString(pixelFormat);
    return str.value_or(QString());
}

QDebug operator<<(QDebug dbg, const VideoFrameFormat &f)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    dbg << "VideoFrameFormat(" << f.pixelFormat() << ", " << f.frameSize()
        << ", viewport=" << f.viewport()
        <<  ", colorSpace=" << f.colorSpace()
        << ')'
        << "\n    pixel format=" << f.pixelFormat()
        << "\n    frame size=" << f.frameSize()
        << "\n    viewport=" << f.viewport()
        << "\n    colorSpace=" << f.colorSpace()
        << "\n    frameRate=" << f.streamFrameRate()
        << "\n    mirrored=" << f.isMirrored()
        << "\n    range=" << f.colorRange()
        << "\n    colorTransfer=" << f.colorTransfer();

    return dbg;
}

QDebug operator<<(QDebug dbg, VideoFrameFormat::PixelFormat pf)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();

    auto format = VideoFrameFormat::pixelFormatToString(pf);
    if (format.isEmpty())
        return dbg;

    dbg.noquote() << QStringLiteral("Format_") << format;
    return dbg;
}
#endif

