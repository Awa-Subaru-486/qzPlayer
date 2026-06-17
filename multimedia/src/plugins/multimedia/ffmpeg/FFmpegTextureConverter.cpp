#include <qzFFmpegMediaPluginImpl/private/FFmpegTextureConverter_p.h>

#include <qzFFmpegMediaPluginImpl/private/FFmpegHwAccel_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegDefs_p.h>

#include <rhi/qrhi.h>

#include <q20type_traits.h>

#ifdef Q_OS_WINDOWS
#  include <qzFFmpegMediaPluginImpl/private/FFmpegHwAccel_d3d11_p.h>
#endif

#if QT_FFMPEG_HAS_VULKAN
#  include <qzFFmpegMediaPluginImpl/private/FFmpegHwAccel_vulkan_p.h>
#endif

#ifdef Q_OS_ANDROID
#  include <qzFFmpegMediaPluginImpl/private/FFmpegHwAccel_MediaCodec_p.h>
#endif

QT_BEGIN_NAMESPACE

namespace ffmpeg {
namespace {

template <typename Converter>
using ConverterTypeIdentity = q20::type_identity<Converter>;

template <typename ConverterTypeHandler>
void applyConverterTypeByPixelFormat(AVPixelFormat fmt, const QRhi &rhi,
                                     ConverterTypeHandler &&handler)
{
    if (!TextureConverter::hwTextureConversionEnabled())
        return;

    switch (fmt) {
#ifdef Q_OS_WINDOWS
    case AV_PIX_FMT_D3D11:
        if (rhi.backend() == QRhi::Implementation::D3D11) {
            if (rhi.driverInfo().deviceType != QRhiDriverInfo::CpuDevice)
                handler(ConverterTypeIdentity<D3D11TextureConverter>{});
        }
        break;
#endif
#if QT_FFMPEG_HAS_VULKAN
    case AV_PIX_FMT_VULKAN:
        if (rhi.backend() == QRhi::Implementation::Vulkan) {
            if (rhi.driverInfo().deviceType != QRhiDriverInfo::CpuDevice)
                handler(ConverterTypeIdentity<VulkanTextureConverter>{});
        }
        break;
#endif
#ifdef Q_OS_ANDROID
    case AV_PIX_FMT_MEDIACODEC:
        if (rhi.backend() == QRhi::Implementation::Vulkan
            || rhi.backend() == QRhi::Implementation::OpenGLES2) {
            if (rhi.driverInfo().deviceType != QRhiDriverInfo::CpuDevice)
                handler(ConverterTypeIdentity<MediaCodecTextureConverter>{});
        }
        break;
#endif
    default:
        Q_UNUSED(handler)
        Q_UNUSED(rhi)
        break;
    }
}

}

TextureConverterBackend::~TextureConverterBackend() = default;

TextureConverter::TextureConverter(QRhi &rhi) : m_rhi(rhi) { }

bool TextureConverter::init(AVFrame &hwFrame)
{
    Q_ASSERT(hwFrame.hw_frames_ctx);
    if (const auto fmt = static_cast<AVPixelFormat>(hwFrame.format); fmt != m_format)
        updateBackend(fmt);
    return !isNull();
}

VideoFrameTexturesUPtr TextureConverter::createTextures(AVFrame &hwFrame,
                                                         VideoFrameTexturesUPtr &oldTextures)
{
    if (isNull())
        return nullptr;

    Q_ASSERT(hwFrame.format == m_format);
    return m_backend->createTextures(&hwFrame, oldTextures);
}

VideoFrameTexturesHandlesUPtr
TextureConverter::createTextureHandles(AVFrame &hwFrame, VideoFrameTexturesHandlesUPtr oldHandles)
{
    if (isNull())
        return nullptr;

    Q_ASSERT(hwFrame.format == m_format);
    return m_backend->createTextureHandles(&hwFrame, std::move(oldHandles));
}

void TextureConverter::updateBackend(AVPixelFormat fmt)
{
    m_backend = nullptr;
    m_format = fmt;

    applyConverterTypeByPixelFormat(m_format, m_rhi, [this]([[maybe_unused]] auto converterTypeIdentity) {
        using ConverterType = typename decltype(converterTypeIdentity)::type;
        m_backend = std::make_shared<ConverterType>(&m_rhi);
    });
}

bool TextureConverter::hwTextureConversionEnabled()
{

    static const int disableHwConversion =
            qEnvironmentVariableIntValue("QT_DISABLE_HW_TEXTURES_CONVERSION");

    return !disableHwConversion;
}

void TextureConverter::applyDecoderPreset(const AVPixelFormat format, AVCodecContext &codecContext)
{
    if (!hwTextureConversionEnabled())
        return;

    Q_ASSERT(codecContext.codec && Codec(codecContext.codec).isDecoder());

#ifdef Q_OS_WINDOWS
    if (format == AV_PIX_FMT_D3D11)
        D3D11TextureConverter::SetupDecoderTextures(&codecContext);
#endif
#if QT_FFMPEG_HAS_VULKAN
    if (format == AV_PIX_FMT_VULKAN)
        VulkanTextureConverter::SetupDecoderTextures(&codecContext);
#endif
#ifdef Q_OS_ANDROID
    if (format == AV_PIX_FMT_MEDIACODEC)
        MediaCodecTextureConverter::setupDecoderSurface(&codecContext);
#endif
    Q_UNUSED(codecContext);
    Q_UNUSED(format);
}

bool TextureConverter::isBackendAvailable(AVFrame &hwFrame, const QRhi &rhi)
{
    bool result = false;
    applyConverterTypeByPixelFormat(static_cast<AVPixelFormat>(hwFrame.format), rhi, [&result](auto) {
        result = true;
    });
    return result;
}
}
QT_END_NAMESPACE
