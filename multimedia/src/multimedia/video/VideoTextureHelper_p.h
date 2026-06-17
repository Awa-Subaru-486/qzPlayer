// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_VIDEO_VIDEOTEXTUREHELPER_P_H
#define QT_VIDEO_VIDEOTEXTUREHELPER_P_H

#include <VideoFrameFormat.h>
#include <SubtitleStyle.h>
#include <rhi/qrhi.h>

#include <QtGui/qtextlayout.h>

class VideoFrame;
class QTextLayout;
class VideoFrameTextures;
using VideoFrameTexturesUPtr = std::unique_ptr<VideoFrameTextures>;
class VideoFrameTexturesHandles;
using VideoFrameTexturesHandlesUPtr = std::unique_ptr<VideoFrameTexturesHandles>;

namespace VideoTextureHelper
{

struct QZ_MULTIMEDIA_EXPORT TextureDescription
{
    static constexpr int maxPlanes = 3;
    struct SizeScale {
        int x;
        int y;
    };
    using BytesRequired = int(*)(int stride, int height);

    enum TextureFormat {
        UnknownFormat,
        Red_8,
        RG_8,
        RGBA_8,
        BGRA_8,
        Red_16,
        RG_16,
    };

    enum class FallbackPolicy {
        Disable,
        Enable
    };

    QRhiTexture::Format rhiTextureFormat(int plane,
                                         QRhi *rhi,
                                         FallbackPolicy policy = FallbackPolicy::Enable) const;

    [[nodiscard]] int strideForWidth(int width) const { return (width*strideFactor + 15) & ~15; }
    [[nodiscard]] int bytesForSize(QSize s) const { return bytesRequired(strideForWidth(s.width()), s.height()); }
    [[nodiscard]] int widthForPlane(int width, int plane) const
    {
        if (plane > nplanes) return 0;
        return (width + sizeScale[plane].x - 1)/sizeScale[plane].x;
    }
    [[nodiscard]] int heightForPlane(int height, int plane) const
    {
        if (plane > nplanes) return 0;
        return (height + sizeScale[plane].y - 1)/sizeScale[plane].y;
    }

    SizeScale rhiSizeScale(int plane, QRhi *rhi) const
    {
        if (!rhi)
            return sizeScale[plane];

        if ((textureFormat[plane] == TextureDescription::RG_8
             || textureFormat[plane] == TextureDescription::Red_16)
            && rhiTextureFormat(plane, rhi) == QRhiTexture::RGBA8)
            return { sizeScale[plane].x * 2, sizeScale[plane].y };

        return sizeScale[plane];
    }

    QSize rhiPlaneSize(const QSize frameSize, int plane, QRhi *rhi) const
    {
        const auto [x, y] = rhiSizeScale(plane, rhi);
        return {frameSize.width() / x, frameSize.height() / y};
    }

    [[nodiscard]] bool hasTextureFormat(TextureFormat format) const
    {
        return std::any_of(textureFormat, textureFormat + nplanes, [format](TextureFormat f) {
            return f == format;
        });
    }

    int nplanes;
    int strideFactor;
    BytesRequired bytesRequired;
    TextureFormat textureFormat[maxPlanes];
    SizeScale sizeScale[maxPlanes];
};

QZ_MULTIMEDIA_EXPORT const TextureDescription *textureDescription(VideoFrameFormat::PixelFormat format);

QZ_MULTIMEDIA_EXPORT QString vertexShaderFileName(const VideoFrameFormat &format);
QZ_MULTIMEDIA_EXPORT QString
fragmentShaderFileName(const VideoFrameFormat &format, QRhi *rhi,
                       QRhiSwapChain::Format surfaceFormat = QRhiSwapChain::SDR);
QZ_MULTIMEDIA_EXPORT void updateUniformData(QByteArray *dst, QRhi *rhi,
                                           const VideoFrameFormat &format,
                                           const VideoFrame &frame, const QMatrix4x4 &transform,
                                           float opacity, float maxNits = 100,
                                           float radius = 0.f,
                                           const float rectSize[2] = nullptr,
                                           const float rectOffset[2] = nullptr);

QZ_MULTIMEDIA_EXPORT VideoFrameTexturesUPtr
createTexturesFromHandles(VideoFrameTexturesHandlesUPtr handles, QRhi &rhi,
                          VideoFrameFormat::PixelFormat pixelFormat, QSize size);

QZ_MULTIMEDIA_EXPORT VideoFrameTexturesUPtr
createTexturesFromHandlesWithReuse(VideoFrameTexturesHandlesUPtr handles, QRhi &rhi,
                                    VideoFrameFormat::PixelFormat pixelFormat, QSize size,
                                    VideoFrameTexturesUPtr &oldTextures);

QZ_MULTIMEDIA_EXPORT VideoFrameTexturesUPtr createTextures(const VideoFrame &frame, QRhi &rhi,
                                                           QRhiResourceUpdateBatch &rub,
                                                           VideoFrameTexturesUPtr &oldTextures);

QZ_MULTIMEDIA_EXPORT QRhiTexture::Format
resolvedRhiTextureFormat(QRhiTexture::Format format, QRhi *rhi);

QRhiTexture::Format
resolveRhiTextureFormatImpl(QRhiTexture::Format format, QRhi *rhi);

QZ_MULTIMEDIA_EXPORT void
setExcludedRhiTextureFormats(QList<QRhiTexture::Format> formats);

struct UniformData {
    float transformMatrix[4][4];
    float colorMatrix[4][4];
    float opacity;
    float width;
    float masteringWhite;
    float maxLum;
    int redOrAlphaIndex;
    int planeFormats[TextureDescription::maxPlanes];
    int colorRange;
    float radius;
    float rectSize[2];
    float rectOffset[2];
};

struct FormatUniformCache {
    float colorMatrix[4][4]{};
    int redOrAlphaIndex = 0;
    int planeFormats[TextureDescription::maxPlanes] = {};
    int colorRange = 0;
    float width = 0.f;
    float masteringWhite = 0.f;
};

QZ_MULTIMEDIA_EXPORT FormatUniformCache
computeFormatUniformCache(const VideoFrameFormat &format, QRhi *rhi);

QZ_MULTIMEDIA_EXPORT float computeMaxLum(float maxNits, const VideoFrameFormat &format);

struct QZ_MULTIMEDIA_EXPORT SubtitleLayout
{
    QSize videoSize;
    QRectF bounds;
    qreal cornerRadius = 0;
    QTextLayout layout;
    SubtitleStyle style;

    bool update(const QSize &frameSize, QString text, const SubtitleStyle &style = {});
    void draw(QPainter *painter, const QPointF &translate) const;
};

}

#endif
