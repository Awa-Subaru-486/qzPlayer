#include "FFmpegConverter_p.h"
#include "FFmpeg_p.h"
#include <qzMultimedia/VideoFrameFormat.h>
#include <qzMultimedia/VideoFrame.h>
import qzLog;
#include <private/VideoTextureHelper_p.h>

extern "C" {
#include <libswscale/swscale.h>
}

QT_BEGIN_NAMESPACE

namespace {

qz::Log::LogCategory qLcConverter("qz.multimedia.ffmpeg.converter");

AVPixelFormat toAVPixelFormat(VideoFrameFormat::PixelFormat pixelFormat)
{
    switch (pixelFormat) {
    default:
    case VideoFrameFormat::Format_Invalid:
        return AV_PIX_FMT_NONE;
    case VideoFrameFormat::Format_AYUV:
    case VideoFrameFormat::Format_AYUV_Premultiplied:
        return AV_PIX_FMT_NONE;
    case VideoFrameFormat::Format_YV12:
    case VideoFrameFormat::Format_IMC1:
    case VideoFrameFormat::Format_IMC3:
    case VideoFrameFormat::Format_IMC2:
    case VideoFrameFormat::Format_IMC4:
        return AV_PIX_FMT_YUV420P;
    case VideoFrameFormat::Format_Jpeg:
        return AV_PIX_FMT_BGRA;
    case VideoFrameFormat::Format_ARGB8888:
        return AV_PIX_FMT_ARGB;
    case VideoFrameFormat::Format_ARGB8888_Premultiplied:
    case VideoFrameFormat::Format_XRGB8888:
        return AV_PIX_FMT_0RGB;
    case VideoFrameFormat::Format_BGRA8888:
        return AV_PIX_FMT_BGRA;
    case VideoFrameFormat::Format_BGRA8888_Premultiplied:
    case VideoFrameFormat::Format_BGRX8888:
        return AV_PIX_FMT_BGR0;
    case VideoFrameFormat::Format_ABGR8888:
        return AV_PIX_FMT_ABGR;
    case VideoFrameFormat::Format_XBGR8888:
        return AV_PIX_FMT_0BGR;
    case VideoFrameFormat::Format_RGBA8888:
        return AV_PIX_FMT_RGBA;
    case VideoFrameFormat::Format_RGBX8888:
        return AV_PIX_FMT_RGB0;
    case VideoFrameFormat::Format_YUV422P:
        return AV_PIX_FMT_YUV422P;
    case VideoFrameFormat::Format_YUV420P:
        return AV_PIX_FMT_YUV420P;
    case VideoFrameFormat::Format_YUV420P10:
        return AV_PIX_FMT_YUV420P10;
    case VideoFrameFormat::Format_UYVY:
        return AV_PIX_FMT_UYVY422;
    case VideoFrameFormat::Format_YUYV:
        return AV_PIX_FMT_YUYV422;
    case VideoFrameFormat::Format_NV12:
        return AV_PIX_FMT_NV12;
    case VideoFrameFormat::Format_NV21:
        return AV_PIX_FMT_NV21;
    case VideoFrameFormat::Format_Y8:
        return AV_PIX_FMT_GRAY8;
    case VideoFrameFormat::Format_Y16:
        return AV_PIX_FMT_GRAY16;
    case VideoFrameFormat::Format_P010:
        return AV_PIX_FMT_P010;
    case VideoFrameFormat::Format_P016:
        return AV_PIX_FMT_P016;
    case VideoFrameFormat::Format_SamplerExternalOES:
        return AV_PIX_FMT_MEDIACODEC;
    }
}

struct SwsFrameData
{
    static constexpr int arraySize = 4;
    std::array<uchar *, arraySize> bits;
    std::array<int, arraySize> stride;
};

SwsFrameData getSwsData(::VideoFrame &dst)
{
    switch (dst.pixelFormat()) {
    case VideoFrameFormat::Format_YV12:
    case VideoFrameFormat::Format_IMC1:
        return { { dst.bits(0), dst.bits(2), dst.bits(1), nullptr },
                 { dst.bytesPerLine(0), dst.bytesPerLine(2), dst.bytesPerLine(1), 0 } };

    case VideoFrameFormat::Format_IMC2:
        return { { dst.bits(0), dst.bits(1) + dst.bytesPerLine(1) / 2, dst.bits(1), nullptr },
                 { dst.bytesPerLine(0), dst.bytesPerLine(1), dst.bytesPerLine(1), 0 } };

    case VideoFrameFormat::Format_IMC4:
        return { { dst.bits(0), dst.bits(1), dst.bits(1) + dst.bytesPerLine(1) / 2, nullptr },
                 { dst.bytesPerLine(0), dst.bytesPerLine(1), dst.bytesPerLine(1), 0 } };
    default:
        return { { dst.bits(0), dst.bits(1), dst.bits(2), nullptr },
                 { dst.bytesPerLine(0), dst.bytesPerLine(1), dst.bytesPerLine(2), 0 } };
    }
}

struct SwsColorSpace
{
    int colorSpace;
    int colorRange;
};

SwsColorSpace toSwsColorSpace(VideoFrameFormat::ColorRange colorRange,
                              VideoFrameFormat::ColorSpace colorSpace)
{
    const int avRange = colorRange == VideoFrameFormat::ColorRange_Video ? 0 : 1;

    switch (colorSpace) {
    case VideoFrameFormat::ColorSpace_BT601:
        if (colorRange == VideoFrameFormat::ColorRange_Full)
            return { SWS_CS_ITU709, 1 };
        return { SWS_CS_ITU601, 0 };
    case VideoFrameFormat::ColorSpace_BT709:
        return { SWS_CS_ITU709, avRange };
    case VideoFrameFormat::ColorSpace_AdobeRgb:
        return { SWS_CS_ITU601, 1 };
    case VideoFrameFormat::ColorSpace_BT2020:
        return { SWS_CS_BT2020, avRange };
    case VideoFrameFormat::ColorSpace_Undefined:
    default:
        return { SWS_CS_DEFAULT, avRange };
    }
}

using PixelFormat = VideoFrameFormat::PixelFormat;

ffmpeg::SwsContextUPtr createConverter(const QSize &srcSize, PixelFormat srcPixFmt,
                               const QSize &dstSize, PixelFormat dstPixFmt)
{
    return ffmpeg::createSwsContext(srcSize, toAVPixelFormat(srcPixFmt), dstSize, toAVPixelFormat(dstPixFmt), SWS_BILINEAR);
}

bool setColorSpaceDetails(SwsContext *context,
                          const VideoFrameFormat &srcFormat,
                          const VideoFrameFormat &dstFormat)
{
    const SwsColorSpace src = toSwsColorSpace(srcFormat.colorRange(), srcFormat.colorSpace());
    const SwsColorSpace dst = toSwsColorSpace(dstFormat.colorRange(), dstFormat.colorSpace());

    constexpr int brightness = 0;
    constexpr int contrast = 0;
    constexpr int saturation = 0;
    const int status = sws_setColorspaceDetails(context,
        sws_getCoefficients(src.colorSpace), src.colorRange,
        sws_getCoefficients(dst.colorSpace), dst.colorRange,
        brightness, contrast, saturation);

    return status == 0;
}

bool convert(SwsContext *context, ::VideoFrame &src, int srcHeight, ::VideoFrame &dst)
{
    if (!src.map(::VideoFrame::ReadOnly))
        return false;

    QScopeGuard unmapSrc{[&] {
        src.unmap();
    }};

    if (!dst.map(::VideoFrame::WriteOnly))
        return false;

    QScopeGuard unmapDst{[&] {
        dst.unmap();
    }};

    const SwsFrameData srcData = getSwsData(src);
    const SwsFrameData dstData = getSwsData(dst);

    constexpr int firstSrcSliceRow = 0;
    const int scaledHeight = sws_scale(context,
        srcData.bits.data(), srcData.stride.data(),
        firstSrcSliceRow, srcHeight,
        dstData.bits.data(), dstData.stride.data());

    if (scaledHeight != srcHeight)
        return false;

    return true;
}

QSize adjustSize(const QSize& size, PixelFormat srcFmt, PixelFormat dstFmt)
{
    const auto* srcDesc = VideoTextureHelper::textureDescription(srcFmt);
    const auto* dstDesc = VideoTextureHelper::textureDescription(dstFmt);

    QSize output = size;
    for (const auto desc : { srcDesc, dstDesc }) {
        for (int i = 0; i < desc->nplanes; ++i) {
            if (desc->sizeScale[i].x != 1)
                output.setWidth(output.width() & ~1);

            if (desc->sizeScale[i].y != 1)
                output.setHeight(output.height() & ~1);
        }
    }

    return output;
}

}

namespace ffmpeg {

::VideoFrame convertFrame(::VideoFrame &src, const VideoFrameFormat &dstFormat)
{
    if (src.size() != dstFormat.frameSize()) {
        qz::Log::cat_critical(qLcConverter, "Resizing is not supported");
        return {};
    }

    const QSize size = adjustSize(src.size(), src.pixelFormat(), dstFormat.pixelFormat());
    if (size != src.size())
        qz::Log::cat_warn(qLcConverter, "Input truncated to even width/height");

    const SwsContextUPtr conv = createConverter(
        size, src.pixelFormat(), size, dstFormat.pixelFormat());

    if (!conv) {
        qz::Log::cat_critical(qLcConverter, "Failed to create SW converter");
        return {};
    }

    if (!setColorSpaceDetails(conv.get(), src.surfaceFormat(), dstFormat)) {
        qz::Log::cat_critical(qLcConverter, "Failed to set color space details");
        return {};
    }

    ::VideoFrame dst{ dstFormat };

    if (!convert(conv.get(), src, size.height(), dst)) {
        qz::Log::cat_critical(qLcConverter, "Frame conversion failed");
        return {};
    }

    return dst;
}

}

QT_END_NAMESPACE
