#include "playbackengine/FFmpegCodecContext_p.h"
#include "FFmpegCodecStorage_p.h"
#include "FFmpegDefs_p.h"

#include <qzMultimedia/PlaybackOptions.h>
#include <rhi/qrhi.h>

#ifdef Q_OS_WINDOWS
#include "d3d11va/FFmpegHwAccel_d3d11sw_p.h"
#include "d3d11va/FFmpegHwAccel_d3d11_p.h"
#endif

#if QT_FFMPEG_HAS_VULKAN
#include "vkvideo/FFmpegHwAccel_vulkan_p.h"
#include "vkvideo/FFmpegHwAccel_vulkansw_p.h"
#endif

#ifdef Q_OS_ANDROID
#include "android/FFmpegHwAccel_MediaCodec_p.h"
#endif

import qzLog;

QT_BEGIN_NAMESPACE

using namespace Qt::StringLiterals;

static qz::Log::LogCategory qLcPlaybackEngineCodec("qz.multimedia.playbackengine.codec");

namespace ffmpeg {

CodecContext::Data::Data(AVCodecContextUPtr context, AVStream *avStream,
                         AVFormatContext *avFormatContext,
                         std::unique_ptr<HWAccel> hwAccel)
    : context(std::move(context)),
      stream(avStream),
      formatContext(avFormatContext),
      hwAccel(std::move(hwAccel))
{
    if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        pixelAspectRatio = av_guess_sample_aspect_ratio(formatContext, stream, nullptr);
}

std::expected<CodecContext, QString> CodecContext::create(AVStream *stream,
                                                          AVFormatContext *formatContext,
                                                          const ::PlaybackOptions &options,
                                                          QRhi *rhi)
{
    if (!stream)
        return std::unexpected{ u"Invalid stream"_s };

    if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
        const auto priority = options.videoDecoderPriority();

        QString lastError;

        for (const auto &policy : priority)
        {
            auto result = tryCreateDecoder(stream, formatContext, options, policy, rhi);
            if (result)
            {
                return result;
            }

            lastError = result.error();
            qz::Log::cat_warn(qLcPlaybackEngineCodec, "Decoder policy {} failed:{}", static_cast<int>(policy), lastError);
        }

        return std::unexpected{ QStringLiteral("All decoders failed. Last error: %1").arg(lastError) };
    }

    return createSoftwareDecoder(stream, formatContext, options);
}

std::expected<CodecContext, QString> CodecContext::tryCreateDecoder(
    AVStream *stream,
    AVFormatContext *formatContext,
    const ::PlaybackOptions &options,
    ::PlaybackOptions::VideoDecoderPolicy policy,
    QRhi *rhi)
{
    switch (policy) {
    case ::PlaybackOptions::VideoDecoderPolicy::Software:
        return createSoftwareDecoder(stream, formatContext, options, rhi);
    case ::PlaybackOptions::VideoDecoderPolicy::HardwareD3D11VA:
    case ::PlaybackOptions::VideoDecoderPolicy::HardwareVkVideo:
    case ::PlaybackOptions::VideoDecoderPolicy::HardwareMediaVideo:
        return createHardwareDecoder(stream, formatContext, options, policy, rhi);
    }

    return std::unexpected{ QStringLiteral("Unknown decoder policy: %1").arg(static_cast<int>(policy)) };
}

std::expected<CodecContext, QString> CodecContext::createSoftwareDecoder(
    AVStream *stream,
    AVFormatContext *formatContext,
    const ::PlaybackOptions &options,
    QRhi *rhi)
{
    return createInternal(stream, formatContext, options, false, AV_HWDEVICE_TYPE_NONE, rhi);
}

// 将 VideoDecoderPolicy 映射到 AVHWDeviceType
static AVHWDeviceType policyToHwDeviceType(::PlaybackOptions::VideoDecoderPolicy policy)
{
    switch (policy) {
    case ::PlaybackOptions::VideoDecoderPolicy::HardwareD3D11VA:
        return AV_HWDEVICE_TYPE_D3D11VA;
    case ::PlaybackOptions::VideoDecoderPolicy::HardwareVkVideo:
        return AV_HWDEVICE_TYPE_VULKAN;
    case ::PlaybackOptions::VideoDecoderPolicy::HardwareMediaVideo:
        return AV_HWDEVICE_TYPE_MEDIACODEC;
    default:
        return AV_HWDEVICE_TYPE_NONE;
    }
}

// 根据 RHI 后端确定首选的硬件解码策略
static ::PlaybackOptions::VideoDecoderPolicy rhiBackendToPolicy(QRhi *rhi)
{
    if (!rhi)
        return ::PlaybackOptions::VideoDecoderPolicy::Software;

    switch (rhi->backend()) {
#ifdef Q_OS_WINDOWS
    case QRhi::D3D11:
        return ::PlaybackOptions::VideoDecoderPolicy::HardwareD3D11VA;
#endif
#if QT_FFMPEG_HAS_VULKAN
    case QRhi::Vulkan:
        return ::PlaybackOptions::VideoDecoderPolicy::HardwareVkVideo;
#endif
#ifdef Q_OS_ANDROID
    case QRhi::OpenGLES2:
        return ::PlaybackOptions::VideoDecoderPolicy::HardwareMediaVideo;
#endif
    default:
        return ::PlaybackOptions::VideoDecoderPolicy::Software;
    }
}

std::expected<CodecContext, QString> CodecContext::createHardwareDecoder(
    AVStream *stream,
    AVFormatContext *formatContext,
    const ::PlaybackOptions &options,
    ::PlaybackOptions::VideoDecoderPolicy policy,
    QRhi *rhi)
{
    if (!rhi)
        return std::unexpected{ QStringLiteral("No RHI available for hardware decoding") };

    // 确定 policy 与 RHI 后端是否匹配，不匹配时优先使用 RHI 后端对应的 API
    const auto rhiPolicy = rhiBackendToPolicy(rhi);
    auto effectivePolicy = policy;

    if (policy != rhiPolicy && rhiPolicy != ::PlaybackOptions::VideoDecoderPolicy::Software) {
        qz::Log::cat_warn(qLcPlaybackEngineCodec,
            "Decoder policy {} doesn't match RHI backend, using RHI-compatible policy {}",
            static_cast<int>(policy), static_cast<int>(rhiPolicy));
        effectivePolicy = rhiPolicy;
    }

    // 构建尝试顺序：首选 effectivePolicy，然后回退到其他硬件策略
    QVector<::PlaybackOptions::VideoDecoderPolicy> tryOrder;
    tryOrder.reserve(3);
    tryOrder.append(effectivePolicy);

#ifdef Q_OS_WINDOWS
    if (effectivePolicy != ::PlaybackOptions::VideoDecoderPolicy::HardwareD3D11VA)
        tryOrder.append(::PlaybackOptions::VideoDecoderPolicy::HardwareD3D11VA);
#elif defined(Q_OS_ANDROID)
    if (effectivePolicy != ::PlaybackOptions::VideoDecoderPolicy::HardwareMediaVideo)
        tryOrder.append(::PlaybackOptions::VideoDecoderPolicy::HardwareMediaVideo);
    if (effectivePolicy != ::PlaybackOptions::VideoDecoderPolicy::HardwareVkVideo)
        tryOrder.append(::PlaybackOptions::VideoDecoderPolicy::HardwareVkVideo);
#else
    if (effectivePolicy != ::PlaybackOptions::VideoDecoderPolicy::HardwareVkVideo)
        tryOrder.append(::PlaybackOptions::VideoDecoderPolicy::HardwareVkVideo);
#endif

    QString lastError;
    for (const auto &tryPolicy : tryOrder) {
        const auto hwDeviceType = policyToHwDeviceType(tryPolicy);
        auto result = createInternal(stream, formatContext, options, true, hwDeviceType, rhi);
        if (result) {
            qz::Log::cat_info(qLcPlaybackEngineCodec, "Hardware decoder created successfully (policy:{})",
                static_cast<int>(tryPolicy));
            return result;
        }
        lastError = result.error();
        qz::Log::cat_warn(qLcPlaybackEngineCodec, "Hardware decoder failed (policy:{}): {}",
            static_cast<int>(tryPolicy), lastError);
    }

    return std::unexpected{ QStringLiteral("All hardware decoders failed: %1").arg(lastError) };
}

AVRational CodecContext::pixelAspectRatio(AVFrame *frame) const
{
    return d->pixelAspectRatio.num && d->pixelAspectRatio.den ? d->pixelAspectRatio
                                                              : frame->sample_aspect_ratio;
}

std::expected<CodecContext, QString> CodecContext::createInternal(
    AVStream *stream,
    AVFormatContext *formatContext,
    const ::PlaybackOptions &options,
    bool useHardware,
    AVHWDeviceType hwDeviceType,
    QRhi *rhi)
{
    Q_ASSERT(stream);

    std::optional<Codec> decoder;
    std::unique_ptr<HWAccel> hwAccel;

#ifdef Q_OS_WINDOWS
    std::shared_ptr<D3D11SWTexturePool> swTexturePool;
#endif
#if QT_FFMPEG_HAS_VULKAN
    std::shared_ptr<VulkanSWImagePool> vkImagePool;
#endif

    const bool enableZeroCopy = options.zeroCopy() == ::PlaybackOptions::ZeroCopy::Enabled;

    if (useHardware)
    {
        if (enableZeroCopy && rhi) {
            std::tie(decoder, hwAccel) = HWAccel::findDecoderWithHwAccel(stream->codecpar->codec_id, hwDeviceType, rhi);
            if (decoder)
                qz::Log::cat_info(qLcPlaybackEngineCodec, "Zero-copy HWAccel: {}",
                    hwAccel->usesExternalDevice() ? "shared device" : "separate device");
        }

        if (!decoder) {
            std::tie(decoder, hwAccel) = HWAccel::findDecoderWithHwAccel(stream->codecpar->codec_id, hwDeviceType);
        }

        if (!decoder) {
            qz::Log::warn("Video decoder: no hardware decoder found for device type: {}", av_hwdevice_get_type_name(hwDeviceType));
            return std::unexpected{QStringLiteral("No hardware decoder found for device type: %1").arg(av_hwdevice_get_type_name(hwDeviceType))};
        }
    }
    else
    {
        decoder = findAVSoftwareDecoder(stream->codecpar->codec_id);
        if (!decoder) {
            return std::unexpected{ u"No software decoder found"_s };
        }

        if (enableZeroCopy && rhi) {
            const auto rhi_backend = rhi->backend();
#ifdef Q_OS_WINDOWS
            if (rhi_backend == QRhi::D3D11)
            {
                if ((decoder->capabilities() & AV_CODEC_CAP_DR1) != 0)
                {
                    if (const auto native = static_cast<const QRhiD3D11NativeHandles *>(rhi->nativeHandles());
                        native && native->dev && native->context)
                    {
                        auto d3d11Device = static_cast<ID3D11Device *>(native->dev);
                        auto d3d11Context = static_cast<ID3D11DeviceContext *>(native->context);

                        swTexturePool = std::make_shared<D3D11SWTexturePool>(d3d11Device, d3d11Context);
                        if (swTexturePool->isValid())
                        {
                            qz::Log::cat_info(qLcPlaybackEngineCodec, "D3D11 software zero-copy enabled");
                        }
                        else
                        {
                            qz::Log::cat_warn(qLcPlaybackEngineCodec, "Failed to create D3D11 software zero-copy texture pool");
                            swTexturePool.reset();
                        }
                    }
                }
            }
#endif
#if QT_FFMPEG_HAS_VULKAN
            if (rhi_backend == QRhi::Vulkan)
            {
                if ((decoder->capabilities() & AV_CODEC_CAP_DR1) != 0)
                {
                    const auto native = static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles());
                    if (native && native->inst && native->dev != VK_NULL_HANDLE && native->physDev != VK_NULL_HANDLE)
                    {
                        vkImagePool = std::make_shared<VulkanSWImagePool>(
                            native->inst, native->dev, native->physDev, native->gfxQueueFamilyIdx);
                        if (vkImagePool->isValid())
                        {
                            qz::Log::cat_info(qLcPlaybackEngineCodec, "Vulkan software zero-copy enabled");
                        }
                        else
                        {
                            qz::Log::cat_warn(qLcPlaybackEngineCodec, "Failed to create Vulkan software zero-copy image pool");
                            vkImagePool.reset();
                        }
                    }
                }
            }
#endif
        }
    }

    AVCodecContextUPtr context(avcodec_alloc_context3(decoder->get()));
    if (!context)
        return std::unexpected{ u"Failed to allocate a FFmpeg codec context"_s };

    context->hwaccel_flags |= AV_HWACCEL_FLAG_IGNORE_LEVEL;

    static const bool allowProfileMismatch = static_cast<bool>(
            qEnvironmentVariableIntValue("QT_FFMPEG_HW_ALLOW_PROFILE_MISMATCH"));
    if (allowProfileMismatch) {
        context->hwaccel_flags |= AV_HWACCEL_FLAG_ALLOW_PROFILE_MISMATCH;
    }

    if (hwAccel)
        context->hw_device_ctx = av_buffer_ref(hwAccel->hwDeviceContextAsBuffer());

    if (context->codec_type != AVMEDIA_TYPE_AUDIO && context->codec_type != AVMEDIA_TYPE_VIDEO
        && context->codec_type != AVMEDIA_TYPE_SUBTITLE) {
        return std::unexpected{ u"Unknown codec type"_s };
    }

    int ret = avcodec_parameters_to_context(context.get(), stream->codecpar);
    if (ret < 0)
        return std::unexpected{
            QStringLiteral("Failed to set FFmpeg codec parameters: %1").arg(err2str(ret))
        };

    context->get_format = getFormat;

#ifdef Q_OS_WINDOWS
    if (rhi && swTexturePool) {
        const auto native = static_cast<const QRhiD3D11NativeHandles *>(rhi->nativeHandles());
        if (native && native->dev && native->context) {
            auto *d3d11Device = static_cast<ID3D11Device *>(native->dev);
            auto *d3d11Context = static_cast<ID3D11DeviceContext *>(native->context);

            setD3D11SWTexturePoolForContext(context.get(), swTexturePool);
            initD3D11SWContext(context.get(), d3d11Device, d3d11Context);
            context->get_buffer2 = d3d11SWGetBuffer2;
            qz::Log::cat_debug(qLcPlaybackEngineCodec, "D3D11 software zero-copy buffer callback installed");
        }
    }
#endif

#if QT_FFMPEG_HAS_VULKAN
    if (rhi && vkImagePool) {
        const auto native = static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles());
        if (native && native->inst) {
            initVulkanSWContext(context.get(), native->inst, native->dev, native->physDev, native->gfxQueueFamilyIdx);
            setVulkanSWImagePoolForContext(context.get(), vkImagePool);
            context->get_buffer2 = vulkanSWGetBuffer2;
            qz::Log::cat_debug(qLcPlaybackEngineCodec, "Vulkan software zero-copy buffer callback installed");
        }
    }
#endif

    AVDictionaryHolder opts;

    if (options.playbackIntent() == ::PlaybackOptions::PlaybackIntent::LowLatencyStreaming)
        av_dict_set(opts, "flags", "low_delay", 0);

    av_dict_set(opts, "threads", "auto", 0);

    if (context->codec_type == AVMEDIA_TYPE_VIDEO) {
        if (decoder->capabilities() & AV_CODEC_CAP_FRAME_THREADS)
            context->thread_type = FF_THREAD_FRAME;
        else if (decoder->capabilities() & AV_CODEC_CAP_SLICE_THREADS)
            context->thread_type = FF_THREAD_SLICE;
    }

    applyExperimentalCodecOptions(*decoder, opts);

    ret = avcodec_open2(context.get(), decoder->get(), opts);

    if (ret < 0) {
        qz::Log::debug("Video decoder failed to open: {} ({}), error:{}", decoder->name().data(),
                        useHardware ? "hardware" : "software", err2str(ret));
        return std::unexpected{
            QStringLiteral("Failed to open FFmpeg codec context: %1").arg(err2str(ret))
        };
    }

    if (context->hw_device_ctx) {
        const auto *device_ctx = reinterpret_cast<AVHWDeviceContext*>(context->hw_device_ctx->data);
        const char* deviceType{};
        switch (device_ctx->type) {
        case AV_HWDEVICE_TYPE_D3D11VA:
            deviceType = "D3D11";
            break;
        case AV_HWDEVICE_TYPE_VULKAN:
            deviceType = "Vulkan";
            break;
        case AV_HWDEVICE_TYPE_MEDIACODEC:
            deviceType = "MediaCodec";
            break;
        default:
            deviceType = av_hwdevice_get_type_name(device_ctx->type);
            break;
        }
        qz::Log::cat_info(qLcPlaybackEngineCodec, "decoder selected: {} (hardware:{})", decoder->name().data(), deviceType);
    } else {
        qz::Log::cat_info(qLcPlaybackEngineCodec, "decoder selected: {} (software)", decoder->name().data());
    }

    return make(std::move(context), stream, formatContext, std::move(hwAccel));
}

}

QT_END_NAMESPACE
