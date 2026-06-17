#include "FFmpegVideoBuffer_p.h"
#include "private/VideoTextureHelper_p.h"
#include "private/MultimediaUtils_p.h"
#include "FFmpegHwAccel_p.h"
#include "qloggingcategory.h"
#include <QtCore/qthread.h>

#ifdef Q_OS_WINDOWS
#include "d3d11va/FFmpegHwAccel_d3d11sw_p.h"
#endif

extern "C" {
#include <libavutil/pixdesc.h>
#include <libavutil/hdr_dynamic_metadata.h>
#include <libavutil/mastering_display_metadata.h>
}

import qzLog;

QT_BEGIN_NAMESPACE

namespace ffmpeg {
static bool isFrameFlipped(const AVFrame& frame) {
    for (int i = 0; i < AV_NUM_DATA_POINTERS && frame.data[i]; ++i) {
        if (frame.linesize[i] < 0)
            return true;
    }

    return false;
}

VideoBuffer::VideoBuffer(AVFrameUPtr frame, AVRational pixelAspectRatio)
    : HwVideoBuffer(::VideoFrame::NoHandle),
      m_frame(frame.get()),
      m_size(qCalculateFrameSize({ frame->width, frame->height },
                                 { pixelAspectRatio.num, pixelAspectRatio.den }))
{
    if (frame->hw_frames_ctx) {
        m_hwFrame = std::move(frame);
        m_pixelFormat = toQtPixelFormat(HWAccel::format(m_hwFrame.get()));
        return;
    }

#ifdef Q_OS_WINDOWS
    if (isD3D11SWZeroCopyFrame(frame.get())) {
        m_swFrame = std::move(frame);
        m_pixelFormat = toQtPixelFormat(AVPixelFormat(m_swFrame->format));
        m_isSWZeroCopy = true;
        return;
    }
#endif

    m_swFrame = std::move(frame);
    m_pixelFormat = toQtPixelFormat(AVPixelFormat(m_swFrame->format));

    convertSWFrame();
}

VideoBuffer::~VideoBuffer() = default;

void VideoBuffer::convertSWFrame()
{
    Q_ASSERT(m_swFrame);

    const auto actualAVPixelFormat = AVPixelFormat(m_swFrame->format);
    const auto targetAVPixelFormat = toAVPixelFormat(m_pixelFormat);

    const QSize actualSize(m_swFrame->width, m_swFrame->height);
    if (actualAVPixelFormat != targetAVPixelFormat || isFrameFlipped(*m_swFrame)
        || m_size != actualSize) {
        Q_ASSERT(toQtPixelFormat(targetAVPixelFormat) == m_pixelFormat);

        if (!m_cachedSwsContext
            || m_cachedSwsSrcW != actualSize.width() || m_cachedSwsSrcH != actualSize.height()
            || m_cachedSwsSrcFmt != actualAVPixelFormat
            || m_cachedSwsDstW != m_size.width() || m_cachedSwsDstH != m_size.height()
            || m_cachedSwsDstFmt != targetAVPixelFormat) {
            m_cachedSwsContext = createSwsContext(actualSize, actualAVPixelFormat, m_size,
                                                   targetAVPixelFormat, SWS_BICUBIC);
            m_cachedSwsSrcW = actualSize.width();
            m_cachedSwsSrcH = actualSize.height();
            m_cachedSwsSrcFmt = actualAVPixelFormat;
            m_cachedSwsDstW = m_size.width();
            m_cachedSwsDstH = m_size.height();
            m_cachedSwsDstFmt = targetAVPixelFormat;
        }

        auto newFrame = makeAVFrame();
        newFrame->width = m_size.width();
        newFrame->height = m_size.height();
        newFrame->format = targetAVPixelFormat;
        av_frame_get_buffer(newFrame.get(), 0);

        sws_scale(m_cachedSwsContext.get(), m_swFrame->data, m_swFrame->linesize, 0, m_swFrame->height,
                  newFrame->data, newFrame->linesize);
        if (m_frame == m_swFrame.get())
            m_frame = newFrame.get();
        m_swFrame = std::move(newFrame);
    }
}

void VideoBuffer::initTextureConverter(QRhi &rhi)
{
    if (!m_hwFrame)
        return;

    ensureTextureConverter(rhi);

    m_type = m_hwFrame && TextureConverter::isBackendAvailable(*m_hwFrame, rhi)
            ? ::VideoFrame::RhiTextureHandle
            : ::VideoFrame::NoHandle;
}

TextureConverter &VideoBuffer::ensureTextureConverter(QRhi &rhi)
{
    Q_ASSERT(m_hwFrame);

    HwFrameContextData &frameContextData = HwFrameContextData::ensure(*m_hwFrame);
    TextureConverter *converter = frameContextData.textureConverterMapper.get(&rhi);

    if (!converter) {
        bool added = false;
        std::tie(converter, added) =
                frameContextData.textureConverterMapper.tryMap(rhi, TextureConverter(rhi));

        Q_ASSERT(converter && added);
    }

    return *converter;
}

QRhi *VideoBuffer::rhi() const
{
    if (!m_hwFrame)
        return nullptr;

    HwFrameContextData &frameContextData = HwFrameContextData::ensure(*m_hwFrame);
    return frameContextData.textureConverterMapper.findRhi(
            [](QRhi &rhi) { return rhi.thread()->isCurrentThread(); });
}

VideoFrameFormat::ColorSpace VideoBuffer::colorSpace() const
{
    return fromAvColorSpace(m_frame->colorspace);
}

VideoFrameFormat::ColorTransfer VideoBuffer::colorTransfer() const
{
    return fromAvColorTransfer(m_frame->color_trc);
}

VideoFrameFormat::ColorRange VideoBuffer::colorRange() const
{
    return fromAvColorRange(m_frame->color_range);
}

float VideoBuffer::maxNits()
{
    float maxNits = -1;
    for (int i = 0; i < m_frame->nb_side_data; ++i) {
        AVFrameSideData *sd = m_frame->side_data[i];

        if (sd->type == AV_FRAME_DATA_MASTERING_DISPLAY_METADATA) {
            auto *data = reinterpret_cast<AVMasteringDisplayMetadata *>(sd->data);
            auto maybeLum = mul(qreal(10'000.), data->max_luminance);
            if (maybeLum)
                maxNits = float(maybeLum.value());
        }
    }
    return maxNits;
}

AbstractVideoBuffer::MapData VideoBuffer::map(::VideoFrame::MapMode mode)
{
    if (!m_swFrame) {
        Q_ASSERT(m_hwFrame && m_hwFrame->hw_frames_ctx);
        m_swFrame = makeAVFrame();

        int ret = av_hwframe_transfer_data(m_swFrame.get(), m_hwFrame.get(), 0);
        if (ret < 0) {
            qz::Log::warn("Error transferring the data to system memory: {}", ret);
            return {};
        }
        convertSWFrame();
    }

    m_mode = mode;

    MapData mapData;
    auto *desc = VideoTextureHelper::textureDescription(pixelFormat());
    mapData.planeCount = desc->nplanes;
    for (int i = 0; i < mapData.planeCount; ++i) {
        Q_ASSERT(m_swFrame->linesize[i] >= 0);

        mapData.data[i] = m_swFrame->data[i];
        mapData.bytesPerLine[i] = m_swFrame->linesize[i];
        mapData.dataSize[i] = mapData.bytesPerLine[i]*desc->heightForPlane(m_swFrame->height, i);
    }

    if ((mode & ::VideoFrame::WriteOnly) != 0 && m_hwFrame) {
        m_type = ::VideoFrame::NoHandle;
        m_hwFrame.reset();
    }

    return mapData;
}

void VideoBuffer::unmap()
{

    m_mode = ::VideoFrame::NotMapped;
}

VideoFrameTexturesUPtr VideoBuffer::mapTextures(QRhi &rhi, VideoFrameTexturesUPtr& oldTextures)
{
    Q_ASSERT(rhi.thread()->isCurrentThread());

#ifdef Q_OS_WINDOWS
    if (m_isSWZeroCopy && m_swFrame) {
        VideoFrameTexturesUPtr result = createTexturesFromSWZeroCopyFrame(rhi, oldTextures);
        m_type = result ? ::VideoFrame::RhiTextureHandle : ::VideoFrame::NoHandle;
        return result;
    }
#endif

    VideoFrameTexturesUPtr result = createTexturesFromHwFrame(rhi, oldTextures);

    m_type = result ? ::VideoFrame::RhiTextureHandle : ::VideoFrame::NoHandle;
    return result;
}

VideoFrameTexturesUPtr VideoBuffer::createTexturesFromHwFrame(QRhi &rhi, VideoFrameTexturesUPtr& oldTextures) {

    if (!m_hwFrame)
        return {};

    constexpr bool initTextureConverterForAnyRhi = false;

    TextureConverter *converter = initTextureConverterForAnyRhi
            ? &ensureTextureConverter(rhi)
            : HwFrameContextData::ensure(*m_hwFrame).textureConverterMapper.get(&rhi);

    if (!converter)
        return {};

    if (!converter->init(*m_hwFrame))
        return {};

    const VideoFrameTextures *oldTexturesRaw = oldTextures.get();
    if (VideoFrameTexturesUPtr newTextures = converter->createTextures(*m_hwFrame, oldTextures))
        return newTextures;

    Q_ASSERT(oldTextures.get() == oldTexturesRaw);

    VideoFrameTexturesHandlesUPtr oldTextureHandles =
            oldTextures ? oldTextures->takeHandles() : nullptr;
    VideoFrameTexturesHandlesUPtr newTextureHandles =
            converter->createTextureHandles(*m_hwFrame, std::move(oldTextureHandles));

    if (newTextureHandles) {
        VideoFrameTexturesUPtr newTextures = VideoTextureHelper::createTexturesFromHandlesWithReuse(
                std::move(newTextureHandles), rhi, m_pixelFormat,
                { m_hwFrame->width, m_hwFrame->height }, oldTextures);

        return newTextures;
    }

    static thread_local int lastFormat = 0;
    if (std::exchange(lastFormat, m_hwFrame->format) != m_hwFrame->format)
        qz::Log::warn("    failed to get textures for frame; format: {}", m_hwFrame->format);

    return {};
}

#ifdef Q_OS_WINDOWS
VideoFrameTexturesUPtr VideoBuffer::createTexturesFromSWZeroCopyFrame(QRhi &rhi, VideoFrameTexturesUPtr& oldTextures)
{
    if (!m_swFrame || rhi.backend() != QRhi::D3D11)
        return {};

    D3D11SWTextureConverter converter(&rhi);
    if (!converter.rhi)
        return {};

    VideoFrameTexturesHandlesUPtr oldTextureHandles =
            oldTextures ? oldTextures->takeHandles() : nullptr;
    VideoFrameTexturesHandlesUPtr newTextureHandles =
            converter.createTextureHandles(m_swFrame.get(), std::move(oldTextureHandles));

    if (newTextureHandles) {
        return VideoTextureHelper::createTexturesFromHandles(
                std::move(newTextureHandles), rhi, m_pixelFormat,
                { m_swFrame->width, m_swFrame->height });
    }

    return {};
}
#endif

VideoFrameFormat::PixelFormat VideoBuffer::pixelFormat() const
{
    return m_pixelFormat;
}

QSize VideoBuffer::size() const
{
    return m_size;
}

VideoFrameFormat::PixelFormat VideoBuffer::toQtPixelFormat(AVPixelFormat avPixelFormat, bool *needsConversion)
{
    if (needsConversion)
        *needsConversion = false;

    switch (avPixelFormat) {
    default:
        break;
    case AV_PIX_FMT_NONE:
        Q_ASSERT(!"Invalid avPixelFormat!");
        return VideoFrameFormat::Format_Invalid;
    case AV_PIX_FMT_ARGB:
        return VideoFrameFormat::Format_ARGB8888;
    case AV_PIX_FMT_0RGB:
        return VideoFrameFormat::Format_XRGB8888;
    case AV_PIX_FMT_BGRA:
        return VideoFrameFormat::Format_BGRA8888;
    case AV_PIX_FMT_BGR0:
        return VideoFrameFormat::Format_BGRX8888;
    case AV_PIX_FMT_ABGR:
        return VideoFrameFormat::Format_ABGR8888;
    case AV_PIX_FMT_0BGR:
        return VideoFrameFormat::Format_XBGR8888;
    case AV_PIX_FMT_RGBA:
        return VideoFrameFormat::Format_RGBA8888;
    case AV_PIX_FMT_RGB0:
        return VideoFrameFormat::Format_RGBX8888;

    case AV_PIX_FMT_YUV422P:
        return VideoFrameFormat::Format_YUV422P;
    case AV_PIX_FMT_YUV420P:
        return VideoFrameFormat::Format_YUV420P;
    case AV_PIX_FMT_YUV420P10:
        return VideoFrameFormat::Format_YUV420P10;
    case AV_PIX_FMT_UYVY422:
        return VideoFrameFormat::Format_UYVY;
    case AV_PIX_FMT_YUYV422:
        return VideoFrameFormat::Format_YUYV;
    case AV_PIX_FMT_NV12:
        return VideoFrameFormat::Format_NV12;
    case AV_PIX_FMT_NV21:
        return VideoFrameFormat::Format_NV21;
    case AV_PIX_FMT_GRAY8:
        return VideoFrameFormat::Format_Y8;
    case AV_PIX_FMT_GRAY16:
        return VideoFrameFormat::Format_Y16;

    case AV_PIX_FMT_P010:
        return VideoFrameFormat::Format_P010;
    case AV_PIX_FMT_P016:
        return VideoFrameFormat::Format_P016;
    case AV_PIX_FMT_MEDIACODEC:
        return VideoFrameFormat::Format_SamplerExternalOES;
    }

    if (needsConversion)
        *needsConversion = true;

    const AVPixFmtDescriptor *descriptor = av_pix_fmt_desc_get(avPixelFormat);

    if (descriptor->flags & AV_PIX_FMT_FLAG_RGB)
        return VideoFrameFormat::Format_RGBA8888;

    if (descriptor->comp[0].depth > 8)
        return VideoFrameFormat::Format_P016;
    return VideoFrameFormat::Format_YUV420P;
}

AVPixelFormat VideoBuffer::toAVPixelFormat(VideoFrameFormat::PixelFormat pixelFormat)
{
    switch (pixelFormat) {
    default:
    case VideoFrameFormat::Format_Invalid:
    case VideoFrameFormat::Format_AYUV:
    case VideoFrameFormat::Format_AYUV_Premultiplied:
    case VideoFrameFormat::Format_YV12:
    case VideoFrameFormat::Format_IMC1:
    case VideoFrameFormat::Format_IMC2:
    case VideoFrameFormat::Format_IMC3:
    case VideoFrameFormat::Format_IMC4:
        return AV_PIX_FMT_NONE;
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
}
QT_END_NAMESPACE
