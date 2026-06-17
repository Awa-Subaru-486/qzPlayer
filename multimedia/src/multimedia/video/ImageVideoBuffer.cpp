// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "ImageVideoBuffer_p.h"
#include "VideoFrameFormat.h"

namespace {

QImage::Format fixImageFormat(QImage::Format format)
{
    switch (format) {
    case QImage::Format_ARGB32_Premultiplied:
    case QImage::Format_ARGB8565_Premultiplied:
    case QImage::Format_ARGB6666_Premultiplied:
    case QImage::Format_ARGB8555_Premultiplied:
    case QImage::Format_ARGB4444_Premultiplied:
    case QImage::Format_RGBA8888_Premultiplied:
    case QImage::Format_A2BGR30_Premultiplied:
    case QImage::Format_A2RGB30_Premultiplied:
    case QImage::Format_RGBA64_Premultiplied:
    case QImage::Format_RGBA16FPx4_Premultiplied:
    case QImage::Format_RGBA32FPx4_Premultiplied:
        return QImage::Format_ARGB32_Premultiplied;
    case QImage::Format_ARGB32:
    case QImage::Format_RGBA8888:
    case QImage::Format_Alpha8:
    case QImage::Format_RGBA64:
    case QImage::Format_RGBA16FPx4:
    case QImage::Format_RGBA32FPx4:
        return QImage::Format_ARGB32;
    case QImage::Format_Invalid:
        return QImage::Format_Invalid;
    default:
        return QImage::Format_RGB32;
    }
}

QImage fixImage(QImage image)
{
    if (image.format() == QImage::Format_Invalid)
        return image;

    const auto frameFormat = VideoFrameFormat::pixelFormatFromImageFormat(image.format());
    if (frameFormat != VideoFrameFormat::Format_Invalid)
        return image;

    return image.convertToFormat(fixImageFormat(image.format()));
}

}

ImageVideoBuffer::ImageVideoBuffer(QImage image) : m_image(fixImage(std::move(image))) { }

AbstractVideoBuffer::MapData ImageVideoBuffer::map(VideoFrame::MapMode mode)
{
    MapData mapData;

    if (!m_image.isNull()) {
        mapData.planeCount = 1;
        mapData.bytesPerLine[0] = m_image.bytesPerLine();
        if (mode == VideoFrame::ReadOnly)
            mapData.data[0] = const_cast<uint8_t *>(m_image.constBits());
        else
            mapData.data[0] = m_image.bits();
        mapData.dataSize[0] = m_image.sizeInBytes();
    }

    return mapData;
}

QImage ImageVideoBuffer::underlyingImage() const
{
    return m_image;
}

