// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "VideoFrame.h"

#include "VideoFrame_p.h"
#include "VideoTextureHelper_p.h"
#include "MultimediaUtils_p.h"
#include "MemoryVideoBuffer_p.h"
#include "VideoFrameConverter_p.h"
#include "ImageVideoBuffer_p.h"
#include "qpainter.h"
#include <qtextlayout.h>

#include <qimage.h>
#include <qsize.h>
#include <qvariant.h>
#include <rhi/qrhi.h>

#include <QDebug>

QT_DEFINE_QESDP_SPECIALIZATION_DTOR(VideoFramePrivate);

VideoFrame::VideoFrame()
{
}

#if QT_DEPRECATED_SINCE(6, 8)

VideoFrame::VideoFrame(AbstractVideoBuffer *buffer, const VideoFrameFormat &format)
    : d(new VideoFramePrivate(format, std::unique_ptr<AbstractVideoBuffer>(buffer)))
{
}

AbstractVideoBuffer *VideoFrame::videoBuffer() const
{
    return d ? d->videoBuffer.get() : nullptr;
}

#endif

VideoFrame::VideoFrame(const VideoFrameFormat &format)
    : d(new VideoFramePrivate(format))
{
    auto *textureDescription = VideoTextureHelper::textureDescription(format.pixelFormat());
    qsizetype bytes = textureDescription->bytesForSize(format.frameSize());
    if (bytes > 0) {
        QByteArray data;
        data.resize(bytes);

        if (!data.isEmpty())
            d->videoBuffer = std::make_unique<MemoryVideoBuffer>(
                    data, textureDescription->strideForWidth(format.frameWidth()));
    }
}

VideoFrame::VideoFrame(const QImage &image)
{
    auto buffer = std::make_unique<ImageVideoBuffer>(image);

    const QImage &bufferImage = buffer->underlyingImage();

    if (bufferImage.isNull())
        return;

    VideoFrameFormat format = {
        bufferImage.size(), VideoFrameFormat::pixelFormatFromImageFormat(bufferImage.format())
    };

    Q_ASSERT(format.isValid());

    d = new VideoFramePrivate{ std::move(format), std::move(buffer) };
}

VideoFrame::VideoFrame(std::unique_ptr<AbstractVideoBuffer> videoBuffer)
{
    if (!videoBuffer)
        return;

    VideoFrameFormat format = videoBuffer->format();
    if (!format.isValid())
        return;

    d = new VideoFramePrivate{ std::move(format), std::move(videoBuffer) };
}

VideoFrame::VideoFrame(const VideoFrame &other) = default;

VideoFrame &VideoFrame::operator =(const VideoFrame &other) = default;

bool VideoFrame::operator==(const VideoFrame &other) const
{

    return d == other.d;
}

bool VideoFrame::operator!=(const VideoFrame &other) const
{
    return d != other.d;
}

VideoFrame::~VideoFrame() = default;

bool VideoFrame::isValid() const
{
    return d && d->videoBuffer && d->format.pixelFormat() != VideoFrameFormat::Format_Invalid;
}

VideoFrameFormat::PixelFormat VideoFrame::pixelFormat() const
{
    return d ? d->format.pixelFormat() : VideoFrameFormat::Format_Invalid;
}

VideoFrameFormat VideoFrame::surfaceFormat() const
{
    return d ? d->format : VideoFrameFormat{};
}

VideoFrame::HandleType VideoFrame::handleType() const
{
    return (d && d->hwVideoBuffer) ? d->hwVideoBuffer->handleType() : VideoFrame::NoHandle;
}

QSize VideoFrame::size() const
{
    return d ? d->format.frameSize() : QSize();
}

int VideoFrame::width() const
{
    return size().width();
}

int VideoFrame::height() const
{
    return size().height();
}

bool VideoFrame::isMapped() const
{
    return d && d->mapMode != VideoFrame::NotMapped;
}

bool VideoFrame::isWritable() const
{
    return d && (d->mapMode & VideoFrame::WriteOnly);
}

bool VideoFrame::isReadable() const
{
    return d && (d->mapMode & VideoFrame::ReadOnly);
}

VideoFrame::MapMode VideoFrame::mapMode() const
{
    return d ? d->mapMode : VideoFrame::NotMapped;
}

bool VideoFrame::map(VideoFrame::MapMode mode)
{
    if (!d || !d->videoBuffer || d->format.frameSize().isEmpty())
        return false;

    QMutexLocker lock(&d->mapMutex);
    if (mode == VideoFrame::NotMapped)
        return false;

    if (d->mappedCount > 0) {

        if (d->mapMode == VideoFrame::ReadOnly && mode == VideoFrame::ReadOnly) {
            d->mappedCount++;
            return true;
        }

        return false;
    }

    Q_ASSERT(d->mapData.data[0] == nullptr);
    Q_ASSERT(d->mapData.bytesPerLine[0] == 0);
    Q_ASSERT(d->mapData.planeCount == 0);
    Q_ASSERT(d->mapData.dataSize[0] == 0);

    d->mapData = d->videoBuffer->map(mode);
    if (d->mapData.planeCount == 0)
        return false;

    d->mapMode = mode;

    if (d->mapData.planeCount == 1) {
        auto pixelFmt = d->format.pixelFormat();

        switch (pixelFmt) {
        case VideoFrameFormat::Format_Invalid:
        case VideoFrameFormat::Format_ARGB8888:
        case VideoFrameFormat::Format_ARGB8888_Premultiplied:
        case VideoFrameFormat::Format_XRGB8888:
        case VideoFrameFormat::Format_BGRA8888:
        case VideoFrameFormat::Format_BGRA8888_Premultiplied:
        case VideoFrameFormat::Format_BGRX8888:
        case VideoFrameFormat::Format_ABGR8888:
        case VideoFrameFormat::Format_XBGR8888:
        case VideoFrameFormat::Format_RGBA8888:
        case VideoFrameFormat::Format_RGBX8888:
        case VideoFrameFormat::Format_AYUV:
        case VideoFrameFormat::Format_AYUV_Premultiplied:
        case VideoFrameFormat::Format_UYVY:
        case VideoFrameFormat::Format_YUYV:
        case VideoFrameFormat::Format_Y8:
        case VideoFrameFormat::Format_Y16:
        case VideoFrameFormat::Format_Jpeg:
        case VideoFrameFormat::Format_SamplerExternalOES:
        case VideoFrameFormat::Format_SamplerRect:

            break;
        case VideoFrameFormat::Format_YUV420P:
        case VideoFrameFormat::Format_YUV420P10:
        case VideoFrameFormat::Format_YUV422P:
        case VideoFrameFormat::Format_YV12: {

            const int height = this->height();
            const int yStride = d->mapData.bytesPerLine[0];
            const int uvHeight = pixelFmt == VideoFrameFormat::Format_YUV422P ? height : height / 2;
            Q_ASSERT(uvHeight > 0);
            const int uvSize = d->mapData.dataSize[0] - yStride * height;
            const int uvStride = uvSize / 2 / uvHeight;

            d->mapData.planeCount = 3;
            d->mapData.bytesPerLine[2] = d->mapData.bytesPerLine[1] = uvStride;
            d->mapData.dataSize[0] = yStride * height;
            d->mapData.dataSize[2] = d->mapData.dataSize[1] = uvStride * uvHeight;
            d->mapData.data[1] = d->mapData.data[0] + d->mapData.dataSize[0];
            d->mapData.data[2] = d->mapData.data[1] + d->mapData.dataSize[1];
            break;
        }
        case VideoFrameFormat::Format_NV12:
        case VideoFrameFormat::Format_NV21:
        case VideoFrameFormat::Format_IMC2:
        case VideoFrameFormat::Format_IMC4:
        case VideoFrameFormat::Format_P010:
        case VideoFrameFormat::Format_P016: {

            d->mapData.planeCount = 2;
            d->mapData.bytesPerLine[1] = d->mapData.bytesPerLine[0];
            int size = d->mapData.dataSize[0];
            d->mapData.dataSize[0] = (d->mapData.bytesPerLine[0] * height());
            d->mapData.dataSize[1] = size - d->mapData.dataSize[0];
            d->mapData.data[1] = d->mapData.data[0] + d->mapData.dataSize[0];
            break;
        }
        case VideoFrameFormat::Format_IMC1:
        case VideoFrameFormat::Format_IMC3: {

            d->mapData.planeCount = 3;
            d->mapData.bytesPerLine[2] = d->mapData.bytesPerLine[1] = d->mapData.bytesPerLine[0];
            d->mapData.dataSize[0] = (d->mapData.bytesPerLine[0] * height());
            d->mapData.dataSize[1] = (d->mapData.bytesPerLine[0] * height() / 2);
            d->mapData.dataSize[2] = (d->mapData.bytesPerLine[0] * height() / 2);
            d->mapData.data[1] = d->mapData.data[0] + d->mapData.dataSize[0];
            d->mapData.data[2] = d->mapData.data[1] + d->mapData.dataSize[1];
            break;
        }
        }
    }

    d->mappedCount++;

    lock.unlock();

    if ((mode & VideoFrame::WriteOnly) != 0) {
        QMutexLocker lock(&d->imageMutex);
        d->image = {};
    }

    return true;
}

void VideoFrame::unmap()
{
    if (!d || !d->videoBuffer)
        return;

    QMutexLocker lock(&d->mapMutex);

    if (d->mappedCount == 0) {
        qWarning() << "VideoFrame::unmap() was called more times then VideoFrame::map()";
        return;
    }

    d->mappedCount--;

    if (d->mappedCount == 0) {
        d->mapData = {};
        d->mapMode = VideoFrame::NotMapped;
        d->videoBuffer->unmap();
    }
}

int VideoFrame::bytesPerLine(int plane) const
{
    if (!d)
        return 0;
    return plane >= 0 && plane < d->mapData.planeCount ? d->mapData.bytesPerLine[plane] : 0;
}

uchar *VideoFrame::bits(int plane)
{
    if (!d)
        return nullptr;
    return plane >= 0 && plane < d->mapData.planeCount ? d->mapData.data[plane] : nullptr;
}

const uchar *VideoFrame::bits(int plane) const
{
    if (!d)
        return nullptr;
    return plane >= 0 && plane < d->mapData.planeCount ?  d->mapData.data[plane] : nullptr;
}

int VideoFrame::mappedBytes(int plane) const
{
    if (!d)
        return 0;
    return plane >= 0 && plane < d->mapData.planeCount ? d->mapData.dataSize[plane] : 0;
}

int VideoFrame::planeCount() const
{
    if (!d)
        return 0;
    return d->format.planeCount();
}

qint64 VideoFrame::startTime() const
{
    if (!d)
        return -1;
    return d->startTime;
}

void VideoFrame::setStartTime(qint64 time)
{
    if (!d)
        return;
    d->startTime = time;
}

qint64 VideoFrame::endTime() const
{
    if (!d)
        return -1;
    return d->endTime;
}

void VideoFrame::setEndTime(qint64 time)
{
    if (!d)
        return;
    d->endTime = time;
}

#if QT_DEPRECATED_SINCE(6, 7)

#endif

void VideoFrame::setRotation(QtVideo::Rotation angle)
{
    if (d)
        d->presentationTransformation.rotation = angle;
}

QtVideo::Rotation VideoFrame::rotation() const
{
    return d ? d->presentationTransformation.rotation : QtVideo::Rotation::None;
}

void VideoFrame::setMirrored(bool mirrored)
{
    if (d)
        d->presentationTransformation.mirroredHorizontallyAfterRotation = mirrored;
}

bool VideoFrame::mirrored() const
{
    return d && d->presentationTransformation.mirroredHorizontallyAfterRotation;
}

void VideoFrame::setStreamFrameRate(qreal rate)
{
    if (d)
        d->format.setStreamFrameRate(rate);
}

qreal VideoFrame::streamFrameRate() const
{
    return d ? d->format.streamFrameRate() : 0.;
}

QImage VideoFrame::toImage() const
{
    if (!isValid())
        return {};

    QMutexLocker lock(&d->imageMutex);

    if (d->image.isNull())
        d->image = qImageFromVideoFrame(*this, qNormalizedSurfaceTransformation(d->format));

    return d->image;
}

QString VideoFrame::subtitleText() const
{
    return d ? d->subtitleText : QString();
}

void VideoFrame::setSubtitleText(const QString &text)
{
    if (!d)
        return;
    d->subtitleText = text;
}

QImage VideoFrame::subtitleImage() const
{
    return d ? d->subtitleImage : QImage();
}

void VideoFrame::setSubtitleImage(const QImage &image)
{
    if (!d)
        return;
    d->subtitleImage = image;
}

QRect VideoFrame::subtitleRect() const
{
    return d ? d->subtitleRect : QRect();
}

void VideoFrame::setSubtitleRect(const QRect &rect)
{
    if (!d)
        return;
    d->subtitleRect = rect;
}

SubtitleBitmapData VideoFrame::subtitleBitmapData() const
{
    return d ? d->subtitleBitmapData : SubtitleBitmapData{};
}

void VideoFrame::setSubtitleBitmapData(const SubtitleBitmapData &data)
{
    if (!d)
        return;
    d->subtitleBitmapData = data;
}

bool VideoFrame::hasSubtitleBitmapData() const
{
    return d && d->subtitleBitmapData.isValid();
}

SubtitleStyle VideoFrame::subtitleStyle() const
{
    return d ? d->subtitleStyle : SubtitleStyle{};
}

void VideoFrame::setSubtitleStyle(const SubtitleStyle &style)
{
    if (!d)
        return;
    d->subtitleStyle = style;
}

void VideoFrame::paint(QPainter *painter, const QRectF &rect, const PaintOptions &options)
{
    if (!isValid()) {
        painter->fillRect(rect, options.backgroundColor);
        return;
    }

    QRectF targetRect = rect;
    QSizeF size = qRotatedFramePresentationSize(*this);

    size.scale(targetRect.size(), options.aspectRatioMode);

    if (options.aspectRatioMode == Qt::KeepAspectRatio) {
        targetRect = QRect(0, 0, size.width(), size.height());
        targetRect.moveCenter(rect.center());

        if (options.backgroundColor != Qt::transparent && rect != targetRect) {
            if (targetRect.top() > rect.top()) {
                QRectF top(rect.left(), rect.top(), rect.width(), targetRect.top() - rect.top());
                painter->fillRect(top, Qt::black);
            }
            if (targetRect.left() > rect.left()) {
                QRectF top(rect.left(), targetRect.top(), targetRect.left() - rect.left(), targetRect.height());
                painter->fillRect(top, Qt::black);
            }
            if (targetRect.right() < rect.right()) {
                QRectF top(targetRect.right(), targetRect.top(), rect.right() - targetRect.right(), targetRect.height());
                painter->fillRect(top, Qt::black);
            }
            if (targetRect.bottom() < rect.bottom()) {
                QRectF top(rect.left(), targetRect.bottom(), rect.width(), rect.bottom() - targetRect.bottom());
                painter->fillRect(top, Qt::black);
            }
        }
    }

    if (map(VideoFrame::ReadOnly)) {
        const QTransform oldTransform = painter->transform();
        QTransform transform = oldTransform;
        transform.translate(targetRect.center().x() - size.width()/2,
                            targetRect.center().y() - size.height()/2);
        painter->setTransform(transform);

        const bool hasPresentationTransformation =
                d->presentationTransformation != VideoTransformation{};

        const QImage image = hasPresentationTransformation
                ? qImageFromVideoFrame(*this, qNormalizedFrameTransformation(*this))
                : toImage();

        painter->drawImage({{}, size}, image, {{},image.size()});
        painter->setTransform(oldTransform);

        unmap();
    } else if (isValid()) {

    } else {
        painter->fillRect(rect, Qt::black);
    }

    if ((options.paintFlags & PaintOptions::DontDrawSubtitles))
        return;

    if (d->subtitleBitmapData.isValid()) {
        // GPU 调色板查找路径的 QPainter 回退：CPU 端展开调色板
        const auto &data = d->subtitleBitmapData;
        const auto &pal = *data.palette;

        // 计算所有 rect 的联合区域
        QRect unionRect;
        for (const auto &rect : data.rects) {
            unionRect = unionRect.united(QRect(rect.x, rect.y, rect.w, rect.h));
        }

        QImage img(unionRect.size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);

        // 逐 rect 展开调色板
        for (const auto &rect : data.rects) {
            const auto &idx = *rect.indexData;
            for (int y = 0; y < rect.h; ++y) {
                auto *line = reinterpret_cast<QRgb *>(img.scanLine(y + rect.y - unionRect.y()));
                const auto *src = idx.constData() + y * rect.w;
                const int xOff = rect.x - unionRect.x();
                for (int x = 0; x < rect.w; ++x) {
                    const uint32_t idxVal = src[x];
                    if (idxVal < static_cast<uint32_t>(data.nbColors)) {
                        const QRgb color = pal[idxVal];
                        const int alpha = qAlpha(color);
                        if (alpha == 0) {
                            line[x + xOff] = 0;
                        } else if (alpha == 255) {
                            line[x + xOff] = color;
                        } else {
                            line[x + xOff] = qRgba(
                                    (qRed(color) * alpha + 127) / 255,
                                    (qGreen(color) * alpha + 127) / 255,
                                    (qBlue(color) * alpha + 127) / 255,
                                    alpha);
                        }
                    } else {
                        line[x + xOff] = 0;
                    }
                }
            }
        }

        QSizeF videoSize = qRotatedFramePresentationSize(*this);
        double scaleX = targetRect.width() / videoSize.width();
        double scaleY = targetRect.height() / videoSize.height();
        QRectF drawRect(targetRect.x() + unionRect.x() * scaleX,
                        targetRect.y() + unionRect.y() * scaleY,
                        unionRect.width() * scaleX,
                        unionRect.height() * scaleY);

        painter->save();
        painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter->drawImage(drawRect, img, QRectF(0, 0, img.width(), img.height()));
        painter->restore();
        return;
    }

    if (!d->subtitleImage.isNull()) {
        const QImage &img = d->subtitleImage;
        const QRect &subRect = d->subtitleRect;
        QSizeF videoSize = qRotatedFramePresentationSize(*this);
        double scaleX = targetRect.width() / videoSize.width();
        double scaleY = targetRect.height() / videoSize.height();

        QRectF drawRect(targetRect.x() + subRect.x() * scaleX,
                        targetRect.y() + subRect.y() * scaleY,
                        subRect.width() * scaleX,
                        subRect.height() * scaleY);

        painter->save();
        painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter->drawImage(drawRect, img, QRectF(0, 0, img.width(), img.height()));
        painter->restore();
        return;
    }

    if (d->subtitleText.isEmpty())
        return;

    auto text = d->subtitleText;
    text.replace(QLatin1Char('\n'), QChar::LineSeparator);

    VideoTextureHelper::SubtitleLayout layout;
    layout.update(targetRect.size().toSize(), this->subtitleText(), d->subtitleStyle);
    layout.draw(painter, targetRect.topLeft());
}

#ifndef QT_NO_DEBUG_STREAM
static QString qFormatTimeStamps(qint64 start, qint64 end)
{

    if (start < 0)
        return QLatin1String("[no timestamp]");

    bool onlyOne = (start == end);

    const int s_millis = start % 1000000;
    start /= 1000000;
    const int s_seconds = start % 60;
    start /= 60;
    const int s_minutes = start % 60;
    start /= 60;

    if (onlyOne) {
        if (start > 0)
            return QStringLiteral("@%1:%2:%3.%4")
                    .arg(start, 1, 10, QLatin1Char('0'))
                    .arg(s_minutes, 2, 10, QLatin1Char('0'))
                    .arg(s_seconds, 2, 10, QLatin1Char('0'))
                    .arg(s_millis, 2, 10, QLatin1Char('0'));
        return QStringLiteral("@%1:%2.%3")
                .arg(s_minutes, 2, 10, QLatin1Char('0'))
                .arg(s_seconds, 2, 10, QLatin1Char('0'))
                .arg(s_millis, 2, 10, QLatin1Char('0'));
    }

    if (end == -1) {

        if (start > 0)
            return QStringLiteral("%1:%2:%3.%4 - forever")
                    .arg(start, 1, 10, QLatin1Char('0'))
                    .arg(s_minutes, 2, 10, QLatin1Char('0'))
                    .arg(s_seconds, 2, 10, QLatin1Char('0'))
                    .arg(s_millis, 2, 10, QLatin1Char('0'));
        return QStringLiteral("%1:%2.%3 - forever")
                .arg(s_minutes, 2, 10, QLatin1Char('0'))
                .arg(s_seconds, 2, 10, QLatin1Char('0'))
                .arg(s_millis, 2, 10, QLatin1Char('0'));
    }

    const int e_millis = end % 1000000;
    end /= 1000000;
    const int e_seconds = end % 60;
    end /= 60;
    const int e_minutes = end % 60;
    end /= 60;

    if (start > 0 || end > 0)
        return QStringLiteral("%1:%2:%3.%4 - %5:%6:%7.%8")
                .arg(start, 1, 10, QLatin1Char('0'))
                .arg(s_minutes, 2, 10, QLatin1Char('0'))
                .arg(s_seconds, 2, 10, QLatin1Char('0'))
                .arg(s_millis, 2, 10, QLatin1Char('0'))
                .arg(end, 1, 10, QLatin1Char('0'))
                .arg(e_minutes, 2, 10, QLatin1Char('0'))
                .arg(e_seconds, 2, 10, QLatin1Char('0'))
                .arg(e_millis, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2.%3 - %4:%5.%6")
            .arg(s_minutes, 2, 10, QLatin1Char('0'))
            .arg(s_seconds, 2, 10, QLatin1Char('0'))
            .arg(s_millis, 2, 10, QLatin1Char('0'))
            .arg(e_minutes, 2, 10, QLatin1Char('0'))
            .arg(e_seconds, 2, 10, QLatin1Char('0'))
            .arg(e_millis, 2, 10, QLatin1Char('0'));
}

QDebug operator<<(QDebug dbg, VideoFrame::HandleType type)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    switch (type) {
    case VideoFrame::NoHandle:
        return dbg << "NoHandle";
    case VideoFrame::RhiTextureHandle:
        return dbg << "RhiTextureHandle";
    }
    return dbg;
}

QDebug operator<<(QDebug dbg, const VideoFrame& f)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    dbg << "VideoFrame(" << f.size() << ", "
               << f.pixelFormat() << ", "
               << f.handleType() << ", "
               << f.mapMode() << ", "
               << qFormatTimeStamps(f.startTime(), f.endTime()).toLatin1().constData();
    dbg << ')';
    return dbg;
}
#endif

