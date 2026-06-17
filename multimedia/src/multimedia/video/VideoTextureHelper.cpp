// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "AbstractVideoBuffer.h"

#include "VideoTextureHelper_p.h"
#include "VideoFrameConverter_p.h"
#include "VideoFrame_p.h"
#include "VideoFrameTextureFromSource_p.h"
#include "private/MultimediaUtils_p.h"
#include "private/AudioAlignmentSupport_p.h"

#include <QtCore/qfile.h>
#include <qpainter.h>
import qzLog;

static qz::Log::LogCategory qLcVideoTextureHelper("qz.multimedia.video.texturehelper");

namespace VideoTextureHelper
{

static const TextureDescription descriptions[VideoFrameFormat::NPixelFormats] = {

    { 0, 0,
      [](int, int) { return 0; },
     { TextureDescription::UnknownFormat, TextureDescription::UnknownFormat, TextureDescription::UnknownFormat},
     { { 1, 1 }, { 1, 1 }, { 1, 1 } }
    },

    { 1, 4,
        [](int stride, int height) { return stride*height; },
        { TextureDescription::RGBA_8, TextureDescription::UnknownFormat, TextureDescription::UnknownFormat },
        { { 1, 1 }, { 1, 1 }, { 1, 1 } }
    },

    { 1, 4,
        [](int stride, int height) { return stride*height; },
        { TextureDescription::RGBA_8, TextureDescription::UnknownFormat, TextureDescription::UnknownFormat },
        { { 1, 1 }, { 1, 1 }, { 1, 1 } }
    },

    { 1, 4,
        [](int stride, int height) { return stride*height; },
        { TextureDescription::RGBA_8, TextureDescription::UnknownFormat, TextureDescription::UnknownFormat },
        { { 1, 1 }, { 1, 1 }, { 1, 1 } }
    },

    { 1, 4,
        [](int stride, int height) { return stride*height; },
        { TextureDescription::BGRA_8, TextureDescription::UnknownFormat, TextureDescription::UnknownFormat },
        { { 1, 1 }, { 1, 1 }, { 1, 1 } }
    },

    { 1, 4,
        [](int stride, int height) { return stride*height; },
        { TextureDescription::BGRA_8, TextureDescription::UnknownFormat, TextureDescription::UnknownFormat },
        { { 1, 1 }, { 1, 1 }, { 1, 1 } }
    },

    { 1, 4,
        [](int stride, int height) { return stride*height; },
        { TextureDescription::BGRA_8, TextureDescription::UnknownFormat, TextureDescription::UnknownFormat },
        { { 1, 1 }, { 1, 1 }, { 1, 1 } }
    },

    { 1, 4,
        [](int stride, int height) { return stride*height; },
        { TextureDescription::RGBA_8, TextureDescription::UnknownFormat, TextureDescription::UnknownFormat },
        { { 1, 1 }, { 1, 1 }, { 1, 1 } }
    },

    { 1, 4,
        [](int stride, int height) { return stride*height; },
        { TextureDescription::RGBA_8, TextureDescription::UnknownFormat, TextureDescription::UnknownFormat },
        { { 1, 1 }, { 1, 1 }, { 1, 1 } }
    },

    { 1, 4,
        [](int stride, int height) { return stride*height; },
        { TextureDescription::RGBA_8, TextureDescription::UnknownFormat, TextureDescription::UnknownFormat },
        { { 1, 1 }, { 1, 1 }, { 1, 1 } }
    },

    { 1, 4,
        [](int stride, int height) { return stride*height; },
        { TextureDescription::RGBA_8, TextureDescription::UnknownFormat, TextureDescription::UnknownFormat },
        { { 1, 1 }, { 1, 1 }, { 1, 1 } }
    },

    { 1, 4,
      [](int stride, int height) { return stride*height; },
     { TextureDescription::RGBA_8, TextureDescription::UnknownFormat, TextureDescription::UnknownFormat },
     { { 1, 1 }, { 1, 1 }, { 1, 1 } }
    },

    { 1, 4,
        [](int stride, int height) { return stride*height; },
        { TextureDescription::RGBA_8, TextureDescription::UnknownFormat, TextureDescription::UnknownFormat },
        { { 1, 1 }, { 1, 1 }, { 1, 1 } }
    },

    { 3, 1,
      [](int stride, int height) { return stride * (height + QtMultimediaPrivate::alignUp(height, 2) / 2); },
     { TextureDescription::Red_8, TextureDescription::Red_8, TextureDescription::Red_8 },
     { { 1, 1 }, { 2, 2 }, { 2, 2 } }
    },

    { 3, 1,
      [](int stride, int height) { return stride * height * 2; },
     {TextureDescription::Red_8, TextureDescription::Red_8, TextureDescription::Red_8 },
     { { 1, 1 }, { 2, 1 }, { 2, 1 } }
    },

    { 3, 1,
      [](int stride, int height) { return stride * (height + QtMultimediaPrivate::alignUp(height, 2) / 2); },
     {TextureDescription::Red_8, TextureDescription::Red_8, TextureDescription::Red_8 },
     { { 1, 1 }, { 2, 2 }, { 2, 2 } }
    },

    { 1, 2,
      [](int stride, int height) { return stride*height; },
     { TextureDescription::RGBA_8, TextureDescription::UnknownFormat, TextureDescription::UnknownFormat },
     { { 2, 1 }, { 1, 1 }, { 1, 1 } }
    },

    { 1, 2,
      [](int stride, int height) { return stride*height; },
     { TextureDescription::RGBA_8, TextureDescription::UnknownFormat, TextureDescription::UnknownFormat },
     { { 2, 1 }, { 1, 1 }, { 1, 1 } }
    },

    { 2, 1,
      [](int stride, int height) { return stride * ((height * 3 / 2 + 1) & ~1); },
     { TextureDescription::Red_8, TextureDescription::RG_8, TextureDescription::UnknownFormat },
     { { 1, 1 }, { 2, 2 }, { 1, 1 } }
    },

    { 2, 1,
      [](int stride, int height) { return stride * ((height * 3 / 2 + 1) & ~1); },
     { TextureDescription::Red_8, TextureDescription::RG_8, TextureDescription::UnknownFormat },
     { { 1, 1 }, { 2, 2 }, { 1, 1 } }
    },

    { 3, 1,
      [](int stride, int height) {

          int h = (height + 15) & ~15;
          h += 2*(((h/2) + 15) & ~15);
          return stride * h;
      },
     {TextureDescription::Red_8,TextureDescription::Red_8,TextureDescription::Red_8 },
     { { 1, 1 }, { 2, 2 }, { 2, 2 } }
    },

    { 2, 1,
      [](int stride, int height) { return 2*stride*height; },
     {TextureDescription::Red_8,TextureDescription::Red_8, TextureDescription::UnknownFormat },
     { { 1, 1 }, { 1, 2 }, { 1, 1 } }
    },

    { 3, 1,
      [](int stride, int height) {

          int h = (height + 15) & ~15;
          h += 2*(((h/2) + 15) & ~15);
          return stride * h;
      },
     {TextureDescription::Red_8,TextureDescription::Red_8,TextureDescription::Red_8 },
     { { 1, 1 }, { 2, 2 }, { 2, 2 } }
    },

    { 2, 1,
      [](int stride, int height) { return 2*stride*height; },
     {TextureDescription::Red_8,TextureDescription::Red_8, TextureDescription::UnknownFormat },
     { { 1, 1 }, { 1, 2 }, { 1, 1 } }
    },

    { 1, 1,
      [](int stride, int height) { return stride*height; },
     {TextureDescription::Red_8, TextureDescription::UnknownFormat, TextureDescription::UnknownFormat },
     { { 1, 1 }, { 1, 1 }, { 1, 1 } }
    },

    { 1, 2,
      [](int stride, int height) { return stride*height; },
     { TextureDescription::Red_16, TextureDescription::UnknownFormat, TextureDescription::UnknownFormat },
     { { 1, 1 }, { 1, 1 }, { 1, 1 } }
    },

    { 2, 2,
      [](int stride, int height) { return stride * ((height * 3 / 2 + 1) & ~1); },
     { TextureDescription::Red_16, TextureDescription::RG_16, TextureDescription::UnknownFormat },
     { { 1, 1 }, { 2, 2 }, { 1, 1 } }
    },

    { 2, 2,
      [](int stride, int height) { return stride * ((height * 3 / 2 + 1) & ~1); },
     { TextureDescription::Red_16, TextureDescription::RG_16, TextureDescription::UnknownFormat },
     { { 1, 1 }, { 2, 2 }, { 1, 1 } }
    },

    {
        1, 0,
        [](int, int) { return 0; },
        { TextureDescription::RGBA_8, TextureDescription::UnknownFormat, TextureDescription::UnknownFormat },
        { { 1, 1 }, { 1, 1 }, { 1, 1 } }
    },

    { 1, 4,
      [](int stride, int height) { return stride*height; },
     { TextureDescription::RGBA_8, TextureDescription::UnknownFormat, TextureDescription::UnknownFormat },
     { { 1, 1 }, { 1, 1 }, { 1, 1 } }
    },

    {
        1, 0,
        [](int, int) { return 0; },
        { TextureDescription::BGRA_8, TextureDescription::UnknownFormat, TextureDescription::UnknownFormat },
        { { 1, 1 }, { 1, 1 }, { 1, 1 } }
    },

    { 3, 2,
        [](int stride, int height) { return stride * (height + QtMultimediaPrivate::alignUp(height, 2) / 2); },
        { TextureDescription::Red_16, TextureDescription::Red_16, TextureDescription::Red_16 },
        { { 1, 1 }, { 2, 2 }, { 2, 2 } }
    },
};

Q_GLOBAL_STATIC(QList<QRhiTexture::Format>, g_excludedRhiTextureFormats)

static bool isRhiTextureFormatSupported(const QRhi *rhi, QRhiTexture::Format format)
{
    if (g_excludedRhiTextureFormats->contains(format))
        return false;
    if (!rhi)
        return true;
    return rhi->isTextureFormatSupported(format);
}

QRhiTexture::Format TextureDescription::rhiTextureFormat(int plane, QRhi *rhi, FallbackPolicy policy) const
{
    QRhiTexture::Format preferredFormat = QRhiTexture::UnknownFormat;

    switch (textureFormat[plane]) {
        case TextureDescription::Red_8:
            preferredFormat = QRhiTexture::R8;
            break;
        case TextureDescription::Red_16:
            preferredFormat = QRhiTexture::R16;
            break;
        case TextureDescription::RG_8:
            preferredFormat = QRhiTexture::RG8;
            break;
        case TextureDescription::RG_16:
            preferredFormat = QRhiTexture::RG16;
            break;
        case TextureDescription::RGBA_8:
            preferredFormat = QRhiTexture::RGBA8;
            break;
        case TextureDescription::BGRA_8:
            preferredFormat = QRhiTexture::BGRA8;
            break;
        case TextureDescription::UnknownFormat:
            break;
        default:
            Q_UNREACHABLE();
    }
    if (policy == FallbackPolicy::Disable)
        return preferredFormat;

    return resolvedRhiTextureFormat(preferredFormat, rhi);
}

QRhiTexture::Format resolvedRhiTextureFormat(QRhiTexture::Format format, QRhi *rhi)
{
    static QHash<QRhiTexture::Format, QRhiTexture::Format> cache;

    auto it = cache.find(format);
    if (it != cache.end())
        return it.value();

    QRhiTexture::Format result = resolveRhiTextureFormatImpl(format, rhi);
    cache[format] = result;
    return result;
}

QRhiTexture::Format resolveRhiTextureFormatImpl(QRhiTexture::Format format, QRhi *rhi)
{
    if (isRhiTextureFormatSupported(rhi, format))
        return format;

    QRhiTexture::Format fallbackFormat;
    switch (format) {
        case QRhiTexture::R8:
            fallbackFormat = resolvedRhiTextureFormat(QRhiTexture::RED_OR_ALPHA8, rhi);
            break;
        case QRhiTexture::RG8:
        case QRhiTexture::RG16:
            fallbackFormat = resolvedRhiTextureFormat(QRhiTexture::RGBA8, rhi);
            break;
        case QRhiTexture::R16:
            fallbackFormat = resolvedRhiTextureFormat(QRhiTexture::RG8, rhi);
            break;
        default:

            return QRhiTexture::UnknownFormat;
    }

    if (fallbackFormat == QRhiTexture::UnknownFormat) {

        qz::Log::cat_debug(qLcVideoTextureHelper, "Cannot determine any usable texture format, using preferred format {}", static_cast<int>(format));
        return format;
    }

    qz::Log::cat_debug(qLcVideoTextureHelper, "Using fallback texture format {}", static_cast<int>(fallbackFormat));
    return fallbackFormat;
}

void setExcludedRhiTextureFormats(QList<QRhiTexture::Format> formats)
{
    g_excludedRhiTextureFormats->swap(formats);
}

const TextureDescription *textureDescription(VideoFrameFormat::PixelFormat format)
{
    return descriptions + format;
}

QString vertexShaderFileName(const VideoFrameFormat &format)
{
    auto fmt = format.pixelFormat();
    Q_UNUSED(fmt);

#if 1
    if (fmt == VideoFrameFormat::Format_SamplerExternalOES)
        return QStringLiteral(":/qz/multimedia/shaders/externalsampler.vert.qsb");
#endif
#if 1
    if (fmt == VideoFrameFormat::Format_SamplerRect)
        return QStringLiteral(":/qz/multimedia/shaders/rectsampler.vert.qsb");
#endif

    return QStringLiteral(":/qz/multimedia/shaders/vertex.vert.qsb");
}

QString fragmentShaderFileName(const VideoFrameFormat &format, QRhi *,
                               QRhiSwapChain::Format surfaceFormat)
{
    QString shaderFile;
    switch (format.pixelFormat()) {
    case VideoFrameFormat::Format_Y8:
        shaderFile = QStringLiteral("y");
        break;
    case VideoFrameFormat::Format_Y16:
        shaderFile = QStringLiteral("y16");
        break;
    case VideoFrameFormat::Format_AYUV:
    case VideoFrameFormat::Format_AYUV_Premultiplied:
        shaderFile = QStringLiteral("ayuv");
        break;
    case VideoFrameFormat::Format_ARGB8888:
    case VideoFrameFormat::Format_ARGB8888_Premultiplied:
    case VideoFrameFormat::Format_XRGB8888:
        shaderFile = QStringLiteral("argb");
        break;
    case VideoFrameFormat::Format_ABGR8888:
    case VideoFrameFormat::Format_XBGR8888:
        shaderFile = QStringLiteral("abgr");
        break;
    case VideoFrameFormat::Format_Jpeg:
        shaderFile = QStringLiteral("bgra");
        break;
    case VideoFrameFormat::Format_RGBA8888:
    case VideoFrameFormat::Format_RGBX8888:
    case VideoFrameFormat::Format_BGRA8888:
    case VideoFrameFormat::Format_BGRA8888_Premultiplied:
    case VideoFrameFormat::Format_BGRX8888:
        shaderFile = QStringLiteral("rgba");
        break;
    case VideoFrameFormat::Format_YUV420P:
    case VideoFrameFormat::Format_YUV422P:
    case VideoFrameFormat::Format_IMC3:
        shaderFile = QStringLiteral("yuv_triplanar");
        break;
    case VideoFrameFormat::Format_YUV420P10:
        shaderFile = QStringLiteral("yuv_triplanar_p10");
        break;
    case VideoFrameFormat::Format_YV12:
    case VideoFrameFormat::Format_IMC1:
        shaderFile = QStringLiteral("yvu_triplanar");
        break;
    case VideoFrameFormat::Format_IMC2:
        shaderFile = QStringLiteral("imc2");
        break;
    case VideoFrameFormat::Format_IMC4:
        shaderFile = QStringLiteral("imc4");
        break;
    case VideoFrameFormat::Format_UYVY:
        shaderFile = QStringLiteral("uyvy");
        break;
    case VideoFrameFormat::Format_YUYV:
        shaderFile = QStringLiteral("yuyv");
        break;
    case VideoFrameFormat::Format_P010:
    case VideoFrameFormat::Format_P016:

        if (format.colorTransfer() == VideoFrameFormat::ColorTransfer_ST2084) {
            shaderFile = QStringLiteral("nv12_bt2020_pq");
            break;
        }
        if (format.colorTransfer() == VideoFrameFormat::ColorTransfer_STD_B67) {
            shaderFile = QStringLiteral("nv12_bt2020_hlg");
            break;
        }
        shaderFile = QStringLiteral("p016");
        break;
    case VideoFrameFormat::Format_NV12:
        shaderFile = QStringLiteral("nv12");
        break;
    case VideoFrameFormat::Format_NV21:
        shaderFile = QStringLiteral("nv21");
        break;
    case VideoFrameFormat::Format_SamplerExternalOES:
#if 1
        shaderFile = QStringLiteral("externalsampler");
        break;
#endif
    case VideoFrameFormat::Format_SamplerRect:
#if 1
        shaderFile = QStringLiteral("rectsampler_bgra");
        break;
#endif

    case VideoFrameFormat::Format_Invalid:
    default:
        break;
    }

    if (shaderFile.isEmpty())
        return {};

    shaderFile.prepend(u":/qz/multimedia/shaders/");

    if (surfaceFormat == QRhiSwapChain::HDRExtendedSrgbLinear)
        shaderFile.append(u"_linear");

    shaderFile.append(u".frag.qsb");

    Q_ASSERT_X(QFile::exists(shaderFile), Q_FUNC_INFO,
               QStringLiteral("Shader file %1 does not exist").arg(shaderFile).toLatin1());

    return shaderFile;
}

static QMatrix4x4 colorMatrix(const VideoFrameFormat &format)
{
    auto colorSpace = format.colorSpace();
    if (colorSpace == VideoFrameFormat::ColorSpace_Undefined) {
        if (format.frameHeight() > 576)

            colorSpace = VideoFrameFormat::ColorSpace_BT709;
        else

            colorSpace = VideoFrameFormat::ColorSpace_BT601;
    }
    switch (colorSpace) {
    case VideoFrameFormat::ColorSpace_AdobeRgb:
        return {
            1.0f,  0.000f,  1.402f, -0.701f,
            1.0f, -0.344f, -0.714f,  0.529f,
            1.0f,  1.772f,  0.000f, -0.886f,
            0.0f,  0.000f,  0.000f,  1.000f
        };
    default:
    case VideoFrameFormat::ColorSpace_BT709:
        if (format.colorRange() == VideoFrameFormat::ColorRange_Full)
            return {
                1.0f,  0.0f,       1.5748f,   -0.790488f,
                1.0f, -0.187324f, -0.468124f,  0.329010f,
                1.0f,  1.855600f,  0.0f,      -0.931439f,
                0.0f,  0.0f,       0.0f,       1.0f
            };
        return {
            1.1644f,  0.0000f,  1.7927f, -0.9729f,
            1.1644f, -0.2132f, -0.5329f,  0.3015f,
            1.1644f,  2.1124f,  0.0000f, -1.1334f,
            0.0000f,  0.0000f,  0.0000f,  1.0000f
        };
    case VideoFrameFormat::ColorSpace_BT2020:
        if (format.colorRange() == VideoFrameFormat::ColorRange_Full)
            return {
                1.f,  0.0000f,  1.4746f, -0.7402f,
                1.f, -0.1646f, -0.5714f,  0.3694f,
                1.f,  1.8814f,  0.000f,  -0.9445f,
                0.0f, 0.0000f,  0.000f,   1.0000f
            };
        return {
            1.1644f,  0.000f,   1.6787f, -0.9157f,
            1.1644f, -0.1874f, -0.6504f,  0.3475f,
            1.1644f,  2.1418f,  0.0000f, -1.1483f,
            0.0000f,  0.0000f,  0.0000f,  1.0000f
        };
    case VideoFrameFormat::ColorSpace_BT601:

        if (format.colorRange() == VideoFrameFormat::ColorRange_Full)
            return {
                1.f,  0.000f,   1.772f,   -0.886f,
                1.f, -0.1646f, -0.57135f,  0.36795f,
                1.f,  1.42f,    0.000f,   -0.71f,
                0.0f, 0.000f,   0.000f,    1.0000f
            };
        return {
            1.164f,  0.000f,  1.596f, -0.8708f,
            1.164f, -0.392f, -0.813f,  0.5296f,
            1.164f,  2.017f,  0.000f, -1.0810f,
            0.000f,  0.000f,  0.000f,  1.0000f
        };
    }
}

static float convertPQFromLinear(float sig)
{
    const float m1 = 1305.f/8192.f;
    const float m2 = 2523.f/32.f;
    const float c1 = 107.f/128.f;
    const float c2 = 2413.f/128.f;
    const float c3 = 2392.f/128.f;

    const float SDR_LEVEL = 100.f;
    sig *= SDR_LEVEL/10000.f;
    float psig = powf(sig, m1);
    float num = c1 + c2*psig;
    float den = 1 + c3*psig;
    return powf(num/den, m2);
}

float convertHLGFromLinear(float sig)
{
    const float a = 0.17883277f;
    const float b = 0.28466892f;
    const float c = 0.55991073f;

    if (sig < 1.f/12.f)
        return sqrtf(3.f*sig);
    return a*logf(12.f*sig - b) + c;
}

static float convertSDRFromLinear(float sig)
{
    return sig;
}

FormatUniformCache computeFormatUniformCache(const VideoFrameFormat &format, QRhi *rhi)
{
    FormatUniformCache cache;

    QMatrix4x4 cmat;
    switch (format.pixelFormat()) {
    case VideoFrameFormat::Format_Invalid:
        break;
    case VideoFrameFormat::Format_ARGB8888:
    case VideoFrameFormat::Format_ARGB8888_Premultiplied:
    case VideoFrameFormat::Format_XRGB8888:
    case VideoFrameFormat::Format_BGRA8888:
    case VideoFrameFormat::Format_BGRA8888_Premultiplied:
    case VideoFrameFormat::Format_BGRX8888:
    case VideoFrameFormat::Format_ABGR8888:
    case VideoFrameFormat::Format_XBGR8888:
    case VideoFrameFormat::Format_RGBA8888:
    case VideoFrameFormat::Format_RGBX8888: {
        if (format.colorRange() == VideoFrameFormat::ColorRange_Video) {
            constexpr float scale = 255.0f / 219.0f;
            constexpr float offset = -16.0f / 219.0f;
            cmat = QMatrix4x4 {
                scale, 0.f,   0.f, offset,
                0.f, scale,   0.f, offset,
                0.f,   0.f, scale, offset,
                0.f,   0.f,   0.f, 1.f,
            };
        }
        break;
    }
    case VideoFrameFormat::Format_Jpeg:
    case VideoFrameFormat::Format_Y8:
    case VideoFrameFormat::Format_Y16:
        break;
    case VideoFrameFormat::Format_IMC1:
    case VideoFrameFormat::Format_IMC2:
    case VideoFrameFormat::Format_IMC3:
    case VideoFrameFormat::Format_IMC4:
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
    case VideoFrameFormat::Format_P010:
    case VideoFrameFormat::Format_P016:
        cmat = colorMatrix(format);
        break;
    default:
        break;
    }

    memcpy(cache.colorMatrix, cmat.constData(), sizeof(cache.colorMatrix));

    auto fromLinear = convertSDRFromLinear;
    switch (format.colorTransfer()) {
    case VideoFrameFormat::ColorTransfer_ST2084:
        fromLinear = convertPQFromLinear;
        break;
    case VideoFrameFormat::ColorTransfer_STD_B67:
        fromLinear = convertHLGFromLinear;
        break;
    default:
        break;
    }

    cache.width = static_cast<float>(format.frameWidth());
    cache.masteringWhite = fromLinear(format.maxLuminance() / 100.f);
    cache.colorRange = static_cast<int>(format.colorRange());

    const TextureDescription *desc = textureDescription(format.pixelFormat());

    constexpr bool isDmaBuf = false;
    using FallbackPolicy = TextureDescription::FallbackPolicy;
    auto fallbackPolicy = isDmaBuf ? FallbackPolicy::Disable : FallbackPolicy::Enable;

    const bool useRedComponent =
        !desc->hasTextureFormat(TextureDescription::Red_8)
        || isRhiTextureFormatSupported(rhi, QRhiTexture::R8)
        || rhi->isFeatureSupported(QRhi::RedOrAlpha8IsRed);
    cache.redOrAlphaIndex = useRedComponent ? 0 : 3;

    for (int plane = 0; plane < desc->nplanes; ++plane)
        cache.planeFormats[plane] = desc->rhiTextureFormat(plane, rhi, fallbackPolicy);
    for (int plane = desc->nplanes; plane < TextureDescription::maxPlanes; ++plane)
        cache.planeFormats[plane] = 0;

    return cache;
}

float computeMaxLum(float maxNits, const VideoFrameFormat &format)
{
    auto fromLinear = convertSDRFromLinear;
    switch (format.colorTransfer()) {
    case VideoFrameFormat::ColorTransfer_ST2084:
        fromLinear = convertPQFromLinear;
        break;
    case VideoFrameFormat::ColorTransfer_STD_B67:
        fromLinear = convertHLGFromLinear;
        break;
    default:
        break;
    }
    return fromLinear(maxNits / 100.f);
}

void updateUniformData(QByteArray *dst, QRhi *rhi, const VideoFrameFormat &format,
                       const VideoFrame &frame, const QMatrix4x4 &transform, float opacity,
                       float maxNits, float radius, const float rectSize[2],
                       const float rectOffset[2])
{
    Q_UNUSED(frame);

    QMatrix4x4 cmat;
    switch (format.pixelFormat()) {
    case VideoFrameFormat::Format_Invalid:
        return;

    case VideoFrameFormat::Format_ARGB8888:
    case VideoFrameFormat::Format_ARGB8888_Premultiplied:
    case VideoFrameFormat::Format_XRGB8888:
    case VideoFrameFormat::Format_BGRA8888:
    case VideoFrameFormat::Format_BGRA8888_Premultiplied:
    case VideoFrameFormat::Format_BGRX8888:
    case VideoFrameFormat::Format_ABGR8888:
    case VideoFrameFormat::Format_XBGR8888:
    case VideoFrameFormat::Format_RGBA8888:
    case VideoFrameFormat::Format_RGBX8888: {
        if (format.colorRange() == VideoFrameFormat::ColorRange_Video) {
            constexpr float scale = 255.0f / 219.0f;
            constexpr float offset = -16.0f / 219.0f;

            cmat = QMatrix4x4 {
                scale, 0.f,   0.f, offset,
                0.f, scale,   0.f, offset,
                0.f,   0.f, scale, offset,
                0.f,   0.f,   0.f, 1.f,
            };

        }

        break;
    }

    case VideoFrameFormat::Format_Jpeg:
    case VideoFrameFormat::Format_Y8:
    case VideoFrameFormat::Format_Y16:
        break;
    case VideoFrameFormat::Format_IMC1:
    case VideoFrameFormat::Format_IMC2:
    case VideoFrameFormat::Format_IMC3:
    case VideoFrameFormat::Format_IMC4:
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
    case VideoFrameFormat::Format_P010:
    case VideoFrameFormat::Format_P016:
        cmat = colorMatrix(format);
        break;
    case VideoFrameFormat::Format_SamplerExternalOES:

        if (auto hwBuffer = VideoFramePrivate::hwBuffer(frame))
            cmat = hwBuffer->externalTextureMatrix();
        break;
    case VideoFrameFormat::Format_SamplerRect:
    {

        const QSize videoSize = frame.size();
        cmat.scale(videoSize.width(), videoSize.height());
    }
        break;
    }

    auto fromLinear = convertSDRFromLinear;
    switch (format.colorTransfer()) {
    case VideoFrameFormat::ColorTransfer_ST2084:
        fromLinear = convertPQFromLinear;
        break;
    case VideoFrameFormat::ColorTransfer_STD_B67:
        fromLinear = convertHLGFromLinear;
        break;
    default:
        break;
    }

    if (dst->size() < sizeof(UniformData))
        dst->resize(sizeof(UniformData));

    auto ud = reinterpret_cast<UniformData*>(dst->data());
    memcpy(ud->transformMatrix, transform.constData(), sizeof(ud->transformMatrix));
    memcpy(ud->colorMatrix, cmat.constData(), sizeof(ud->transformMatrix));
    ud->opacity = opacity;
    ud->width = float(format.frameWidth());
    ud->masteringWhite = fromLinear(float(format.maxLuminance())/100.f);
    ud->maxLum = fromLinear(float(maxNits)/100.f);
    const TextureDescription* desc = textureDescription(format.pixelFormat());

    const bool isDmaBuf = VideoFramePrivate::hasDmaBuf(frame);
    using FallbackPolicy = VideoTextureHelper::TextureDescription::FallbackPolicy;
    auto fallbackPolicy = isDmaBuf
            ? FallbackPolicy::Disable
            : FallbackPolicy::Enable;

    const bool useRedComponent =
            !desc->hasTextureFormat(TextureDescription::Red_8)
            || isRhiTextureFormatSupported(rhi, QRhiTexture::R8)
            || rhi->isFeatureSupported(QRhi::RedOrAlpha8IsRed)
            || isDmaBuf;
    ud->redOrAlphaIndex = useRedComponent ? 0 : 3;

    for (int plane = 0; plane < desc->nplanes; ++plane)
        ud->planeFormats[plane] = desc->rhiTextureFormat(plane, rhi, fallbackPolicy);

    ud->colorRange = static_cast<int>(format.colorRange());

    ud->radius = radius;
    if (rectSize) {
        ud->rectSize[0] = rectSize[0];
        ud->rectSize[1] = rectSize[1];
    } else {
        ud->rectSize[0] = 0.f;
        ud->rectSize[1] = 0.f;
    }
    if (rectOffset) {
        ud->rectOffset[0] = rectOffset[0];
        ud->rectOffset[1] = rectOffset[1];
    } else {
        ud->rectOffset[0] = 0.f;
        ud->rectOffset[1] = 0.f;
    }
}

enum class UpdateTextureWithMapResult : uint8_t {
    Failed,
    UpdatedWithDataCopy,
    UpdatedWithDataReference
};

static UpdateTextureWithMapResult updateTextureWithMap(const VideoFrame &frame, QRhi &rhi,
                                                       QRhiResourceUpdateBatch &rub, const int plane,
                                                       std::unique_ptr<QRhiTexture> &tex)
{
    Q_ASSERT(frame.isMapped());

    VideoFrameFormat fmt = frame.surfaceFormat();
    VideoFrameFormat::PixelFormat pixelFormat = fmt.pixelFormat();
    QSize size = fmt.frameSize();

    const TextureDescription &texDesc = descriptions[pixelFormat];
    QSize planeSize = texDesc.rhiPlaneSize(size, plane, &rhi);

    bool needsRebuild = !tex || tex->pixelSize() != planeSize || tex->format() != texDesc.rhiTextureFormat(plane, &rhi);
    if (!tex) {
        tex.reset(rhi.newTexture(texDesc.rhiTextureFormat(plane, &rhi), planeSize, 1, {}));
        if (!tex) {
            qWarning("Failed to create new texture (size %dx%d)", planeSize.width(), planeSize.height());
            return UpdateTextureWithMapResult::Failed;
        }
    }

    if (needsRebuild) {
        tex->setFormat(texDesc.rhiTextureFormat(plane, &rhi));
        tex->setPixelSize(planeSize);
        if (!tex->create()) {
            qWarning("Failed to create texture (size %dx%d)", planeSize.width(), planeSize.height());
            return UpdateTextureWithMapResult::Failed;
        }
    }

    auto result = UpdateTextureWithMapResult::UpdatedWithDataCopy;

    QRhiTextureSubresourceUploadDescription subresDesc;

    if (pixelFormat == VideoFrameFormat::Format_Jpeg) {
        Q_ASSERT(plane == 0);

        QImage image;

        const VideoFrameFormat surfaceFormat = frame.surfaceFormat();

        const bool hasSurfaceTransform = surfaceFormat.isMirrored()
                || surfaceFormat.scanLineDirection() == VideoFrameFormat::BottomToTop
                || surfaceFormat.rotation() != QtVideo::Rotation::None;

        if (hasSurfaceTransform)
            image = qImageFromVideoFrame(frame, VideoTransformation{});
        else
            image = frame.toImage();

        image.convertTo(QImage::Format_ARGB32);
        subresDesc.setImage(image);

    } else {

        subresDesc.setData(QByteArray::fromRawData(
                reinterpret_cast<const char *>(frame.bits(plane)), frame.mappedBytes(plane)));
        subresDesc.setDataStride(frame.bytesPerLine(plane));
        result = UpdateTextureWithMapResult::UpdatedWithDataReference;
    }

    QRhiTextureUploadEntry entry(0, 0, subresDesc);
    QRhiTextureUploadDescription desc({ entry });
    rub.uploadTexture(tex.get(), desc);

    return result;
}

static std::unique_ptr<QRhiTexture>
createTextureFromHandle(VideoFrameTexturesHandles &texturesSet, QRhi &rhi,
                        VideoFrameFormat::PixelFormat pixelFormat, QSize size, int plane)
{
    const TextureDescription &texDesc = descriptions[pixelFormat];
    QSize planeSize = texDesc.rhiPlaneSize(size, plane, &rhi);

    QRhiTexture::Flags textureFlags = {};

    if (quint64 handle = texturesSet.textureHandle(rhi, plane); handle) {
        std::unique_ptr<QRhiTexture> tex(rhi.newTexture(texDesc.rhiTextureFormat(plane, &rhi), planeSize, 1, textureFlags));
        if (tex->createFrom({handle, 0}))
            return tex;

        qWarning("Failed to initialize QRhiTexture wrapper for native texture object %llu",handle);
    }
    return {};
}

template <typename TexturesType, typename... Args>
static VideoFrameTexturesUPtr
createTexturesArray(QRhi &rhi, VideoFrameTexturesHandles &texturesSet,
                    VideoFrameFormat::PixelFormat pixelFormat, QSize size, Args &&...args)
{
    const TextureDescription &texDesc = descriptions[pixelFormat];
    bool ok = true;
    RhiTextureArray textures;
    for (quint8 plane = 0; plane < texDesc.nplanes; ++plane) {
        textures[plane] = VideoTextureHelper::createTextureFromHandle(texturesSet, rhi,
                                                                       pixelFormat, size, plane);
        ok &= bool(textures[plane]);
    }
    if (ok)
        return std::make_unique<TexturesType>(std::move(textures), std::forward<Args>(args)...);
    else
        return {};
}

VideoFrameTexturesUPtr createTexturesFromHandles(VideoFrameTexturesHandlesUPtr texturesSet,
                                                  QRhi &rhi,
                                                  VideoFrameFormat::PixelFormat pixelFormat,
                                                  QSize size)
{
    if (!texturesSet)
        return nullptr;

    if (pixelFormat == VideoFrameFormat::Format_Invalid)
        return nullptr;

    if (size.isEmpty())
        return nullptr;

    auto &texturesSetRef = *texturesSet;
    return createTexturesArray<VideoFrameTexturesFromHandlesSet>(rhi, texturesSetRef, pixelFormat,
                                                                  size, std::move(texturesSet));
}

static std::unique_ptr<QRhiTexture>
reuseOrCreateTextureFromHandle(VideoFrameTexturesHandles &texturesSet, QRhi &rhi,
                                VideoFrameFormat::PixelFormat pixelFormat, QSize size, int plane,
                                QRhiTexture *oldTexture)
{
    const TextureDescription &texDesc = descriptions[pixelFormat];
    QSize planeSize = texDesc.rhiPlaneSize(size, plane, &rhi);

    if (quint64 handle = texturesSet.textureHandle(rhi, plane); handle) {
        if (oldTexture && oldTexture->pixelSize() == planeSize
            && oldTexture->format() == texDesc.rhiTextureFormat(plane, &rhi)) {
            quint64 oldHandle = oldTexture->nativeTexture().object;
            if (oldHandle == handle)
                return std::unique_ptr<QRhiTexture>(oldTexture);

            if (oldTexture->createFrom({handle, 0}))
                return std::unique_ptr<QRhiTexture>(oldTexture);
        }

        std::unique_ptr<QRhiTexture> tex(rhi.newTexture(texDesc.rhiTextureFormat(plane, &rhi), planeSize, 1, {}));
        if (tex->createFrom({handle, 0}))
            return tex;

        qWarning("Failed to initialize QRhiTexture wrapper for native texture object %llu", handle);
    }
    return {};
}

VideoFrameTexturesUPtr createTexturesFromHandlesWithReuse(VideoFrameTexturesHandlesUPtr texturesSet,
                                                           QRhi &rhi,
                                                           VideoFrameFormat::PixelFormat pixelFormat,
                                                           QSize size,
                                                           VideoFrameTexturesUPtr &oldTextures)
{
    if (!texturesSet)
        return nullptr;

    if (pixelFormat == VideoFrameFormat::Format_Invalid)
        return nullptr;

    if (size.isEmpty())
        return nullptr;

    auto *oldHandlesSet = dynamic_cast<VideoFrameTexturesFromHandlesSet *>(oldTextures.get());

    const TextureDescription &texDesc = descriptions[pixelFormat];
    bool ok = true;
    RhiTextureArray textures;

    auto &texturesSetRef = *texturesSet;

    for (quint8 plane = 0; plane < texDesc.nplanes; ++plane) {
        QRhiTexture *oldTex = oldHandlesSet ? oldHandlesSet->texture(plane) : nullptr;
        textures[plane] = reuseOrCreateTextureFromHandle(texturesSetRef, rhi, pixelFormat, size, plane, oldTex);
        ok &= bool(textures[plane]);
    }

    if (oldHandlesSet) {
        for (quint8 plane = 0; plane < texDesc.nplanes; ++plane) {
            if (textures[plane] && textures[plane].get() == oldHandlesSet->texture(plane)) {
                oldHandlesSet->textureArray()[plane].release();
            }
        }
    }

    if (ok)
        return std::make_unique<VideoFrameTexturesFromHandlesSet>(std::move(textures), std::move(texturesSet));
    else
        return {};
}

static VideoFrameTexturesUPtr createTexturesFromMemory(VideoFrame frame, QRhi &rhi,
                                                        QRhiResourceUpdateBatch &rub,
                                                        VideoFrameTexturesUPtr &oldTextures)
{
    qz::Log::cat_debug(qLcVideoTextureHelper, "createTexturesFromMemory, pixelFormat: {}", static_cast<int>(frame.pixelFormat()));
    if (!frame.map(VideoFrame::ReadOnly)) {
        qWarning() << "Cannot map a video frame in ReadOnly mode!";
        return {};
    }

    auto unmapFrameGuard = qScopeGuard([&frame] { frame.unmap(); });

    const TextureDescription &texDesc = descriptions[frame.surfaceFormat().pixelFormat()];

    const bool canReuseTextures(dynamic_cast<VideoFrameTexturesFromMemory*>(oldTextures.get()));

    std::unique_ptr<VideoFrameTexturesFromMemory> textures(canReuseTextures ?
                static_cast<VideoFrameTexturesFromMemory *>(oldTextures.release()) :
                new VideoFrameTexturesFromMemory);

    RhiTextureArray& textureArray = textures->textureArray();
    bool shouldKeepMapping = false;
    for (quint8 plane = 0; plane < texDesc.nplanes; ++plane) {
        const auto result = updateTextureWithMap(frame, rhi, rub, plane, textureArray[plane]);
        if (result == UpdateTextureWithMapResult::Failed)
            return {};

        if (result == UpdateTextureWithMapResult::UpdatedWithDataReference)
            shouldKeepMapping = true;
    }

    textures->setMappedFrame(shouldKeepMapping ? std::move(frame) : VideoFrame());

    return textures;
}

VideoFrameTexturesUPtr createTextures(const VideoFrame &frame, QRhi &rhi,
                                       QRhiResourceUpdateBatch &rub,
                                       VideoFrameTexturesUPtr &oldTextures)
{
    if (!frame.isValid())
        return {};

    auto setSourceFrame = [&frame](VideoFrameTexturesUPtr result) {
        result->setSourceFrame(frame);
        return result;
    };

    if (HwVideoBuffer *hwBuffer = VideoFramePrivate::hwBuffer(frame)) {
        if (auto textures = hwBuffer->mapTextures(rhi, oldTextures))
            return setSourceFrame(std::move(textures));

        VideoFrameFormat format = frame.surfaceFormat();
        if (auto textures = createTexturesArray<VideoFrameTexturesFromRhiTextureArray>(
                    rhi, *hwBuffer, format.pixelFormat(), format.frameSize()))
            return setSourceFrame(std::move(textures));
    }

    if (auto textures = createTexturesFromMemory(frame, rhi, rub, oldTextures))
        return setSourceFrame(std::move(textures));

    return {};
}

bool SubtitleLayout::update(const QSize &frameSize, QString text, const SubtitleStyle &subStyle)
{
    text.replace(QLatin1Char('\n'), QChar::LineSeparator);
    if (layout.text() == text && videoSize == frameSize && style == subStyle)
        return false;

    style = subStyle;
    videoSize = frameSize;
    QFont font;

    if (!style.fontFamily().isEmpty())
        font.setFamily(style.fontFamily());
    font.setPointSize(static_cast<int>(style.fontSize()));
    font.setBold(style.bold());
    font.setItalic(style.italic());

    layout.setText(text);
    if (text.isEmpty()) {
        bounds = {};
        return true;
    }
    layout.setFont(font);
    QTextOption option;
    option.setUseDesignMetrics(true);
    option.setAlignment(Qt::AlignCenter);
    layout.setTextOption(option);

    QFontMetrics metrics(font);
    int leading = metrics.leading();

    qreal leftMarginPx = videoSize.width() * style.leftMargin();
    qreal rightMarginPx = videoSize.width() * style.rightMargin();
    qreal lineWidth = videoSize.width() - leftMarginPx - rightMarginPx;
    qreal height = 0;
    qreal textWidth = 0;
    layout.beginLayout();
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;

        line.setLineWidth(lineWidth);
        height += leading;
        line.setPosition(QPointF(leftMarginPx, height));
        height += line.height();
        textWidth = qMax(textWidth, line.naturalTextWidth());
    }
    layout.endLayout();

    qreal topMarginPx = videoSize.height() * style.topMargin();
    qreal bottomMarginPx = videoSize.height() * style.bottomMargin();
    qreal y = videoSize.height() - bottomMarginPx - height;
    if (y < topMarginPx)
        y = topMarginPx;

    layout.setPosition(QPointF(0, y));
    textWidth += metrics.height() / 4.;

    const qreal availableCenterX = (leftMarginPx + videoSize.width() - rightMarginPx) / 2.;
    constexpr qreal paddingH = 8;
    constexpr qreal paddingV = 8;
    bounds = QRectF(availableCenterX - textWidth / 2. - paddingH,
                     y - paddingV,
                     textWidth + 2 * paddingH,
                     height + 2 * paddingV);
    cornerRadius = bounds.height() * style.cornerRadius();
    return true;
}

void SubtitleLayout::draw(QPainter *painter, const QPointF &translate) const
{
    painter->save();
    painter->translate(translate);
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter->setRenderHint(QPainter::Antialiasing);

    QColor bgColor = style.backgroundColor();
    bgColor.setAlphaF(style.backgroundOpacity());
    painter->setBrush(bgColor);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(bounds, cornerRadius, cornerRadius);

    QTextLayout::FormatRange range;
    range.start = 0;
    range.length = layout.text().size();
    range.format.setForeground(style.fontColor());
    layout.draw(painter, {}, { range });
    painter->restore();
}

}

