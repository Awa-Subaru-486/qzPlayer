#include "libavutil/version.h"

#include "FFmpegHwAccel_p.h"
#include "FFmpegDefs_p.h"

#ifdef Q_OS_WINDOWS
#  include "d3d11va/FFmpegHwAccel_d3d11_p.h"
#  include <QtCore/private/qsystemlibrary_p.h>
#endif

#if QT_FFMPEG_HAS_VULKAN
#  include "vkvideo/FFmpegHwAccel_vulkan_p.h"
#endif

#ifdef Q_OS_ANDROID
#  include "android/FFmpegHwAccel_MediaCodec_p.h"
#endif

#include "FFmpeg_p.h"
#include "FFmpegCodecStorage_p.h"
#include "FFmpegMediaIntegration_p.h"
#include "FFmpegVideoBuffer_p.h"
#include "qscopedvaluerollback.h"

extern "C" {
#include <libswscale/swscale.h>
}

#include <QtCore/QElapsedTimer>

#ifdef Q_OS_LINUX
#  include "QtCore/qfile.h"
#  include <QLibrary>
#endif

#include <rhi/qrhi.h>
#include <unordered_set>
import qzLog;

QT_BEGIN_NAMESPACE

using namespace Qt::StringLiterals;

static qz::Log::LogCategory qLHWAccel("qz.multimedia.ffmpeg.hwaccel");

namespace ffmpeg {

thread_local bool FFmpegLogsEnabledInThread = true;

static const std::initializer_list<AVHWDeviceType> preferredHardwareAccelerators = {
#if defined(Q_OS_LINUX)
    AV_HWDEVICE_TYPE_CUDA,

#elif defined (Q_OS_WIN)
    AV_HWDEVICE_TYPE_D3D11VA,

#elif defined(Q_OS_ANDROID)
    AV_HWDEVICE_TYPE_MEDIACODEC,
    AV_HWDEVICE_TYPE_VULKAN,
#endif
};

static AVBufferUPtr loadHWContext(AVHWDeviceType type)
{
    AVBufferRef *hwContext{};
    if (av_hwdevice_ctx_create(&hwContext, type, nullptr, nullptr, 0) == 0) {
        return AVBufferUPtr(hwContext);
    }
    return nullptr;
}

static bool precheckDriver(AVHWDeviceType type)
{
#if defined(Q_OS_LINUX)
    if (type == AV_HWDEVICE_TYPE_CUDA) {
        if (!QFile::exists(QLatin1String("/proc/driver/nvidia/version")))
            return false;

        QLibrary lib(u"libnvcuvid.so"_s);
        if (!lib.load())
            return false;
        lib.unload();
        return true;
    }
#elif defined(Q_OS_WINDOWS)
    if (type == AV_HWDEVICE_TYPE_D3D11VA)
        return QSystemLibrary(QLatin1String("d3d11.dll")).load();

#if QT_FFMPEG_HAS_D3D12VA
    if (type == AV_HWDEVICE_TYPE_D3D12VA)
        return QSystemLibrary(QLatin1String("d3d12.dll")).load();
#endif

    if (type == AV_HWDEVICE_TYPE_DXVA2)
        return QSystemLibrary(QLatin1String("d3d9.dll")).load();

    if (type == AV_HWDEVICE_TYPE_CUDA)
        return QSystemLibrary(QLatin1String("nvml.dll")).load();
#elif defined(Q_OS_ANDROID)
    if (type == AV_HWDEVICE_TYPE_VULKAN) {
        QLibrary lib(u"vulkan"_s);
        if (!lib.load())
            return false;
        lib.unload();
        return true;
    }
#else
     Q_UNUSED(type);
#endif

    return true;
}

static bool checkHwType(AVHWDeviceType type)
{
    const auto deviceName = av_hwdevice_get_type_name(type);
    if (!deviceName) {
        qz::Log::warn("Internal FFmpeg error, unknow hw type: {}", static_cast<int>(type));
        return false;
    }

    if (!precheckDriver(type)) {
        qz::Log::cat_debug(qLHWAccel, "Drivers for hw device {} is not installed", deviceName);
        return false;
    }

    if (type == AV_HWDEVICE_TYPE_D3D11VA ||
#if QT_FFMPEG_HAS_D3D12VA
        type == AV_HWDEVICE_TYPE_D3D12VA ||
#endif
        type == AV_HWDEVICE_TYPE_DXVA2
        || type == AV_HWDEVICE_TYPE_MEDIACODEC)
        return true;

    QScopedValueRollback rollback(FFmpegLogsEnabledInThread);
    FFmpegLogsEnabledInThread = false;

    return loadHWContext(type) != nullptr;
}

static const std::vector<AVHWDeviceType> &deviceTypes()
{
    static const auto types = []() {
        qz::Log::cat_debug(qLHWAccel, "Check device types");
        QElapsedTimer timer;
        timer.start();

        std::unordered_set<AVPixelFormat> hwPixFormats;
        for (const Codec codec : CodecEnumerator()) {
            forEachAVPixelFormat(codec, [&](AVPixelFormat format) {
                if (isHwPixelFormat(format))
                    hwPixFormats.insert(format);
            });
        }

        std::vector<AVHWDeviceType> result;
        AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
        while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
            if (hwPixFormats.contains(pixelFormatForHwDevice(type)) && checkHwType(type))
                result.push_back(type);
        result.shrink_to_fit();

        auto it = result.begin();
        for (const auto preffered : preferredHardwareAccelerators) {
            auto found = std::find(it, result.end(), preffered);
            if (found != result.end())
                std::rotate(it++, found, std::next(found));
        }

        using namespace std::chrono;
        qz::Log::cat_debug(qLHWAccel, "Device types checked. Spent time:{}", duration_cast<microseconds>(timer.durationElapsed()));

        return result;
    }();

    return types;
}

static std::vector<AVHWDeviceType> deviceTypes(const char *envVarName)
{
    const auto definedDeviceTypes = qgetenv(envVarName);

    if (definedDeviceTypes.isNull())
        return deviceTypes();

    std::vector<AVHWDeviceType> result;
    const auto definedDeviceTypesString = QString::fromUtf8(definedDeviceTypes).toLower();
    for (const auto &deviceType : definedDeviceTypesString.split(u',')) {
        if (!deviceType.isEmpty()) {
            const auto foundType = av_hwdevice_find_type_by_name(deviceType.toUtf8().data());
            if (foundType == AV_HWDEVICE_TYPE_NONE)
                qz::Log::warn("Unknown hw device type {}", deviceType);
            else
                result.emplace_back(foundType);
        }
    }

    result.shrink_to_fit();
    return result;
}

// 遍历所有支持的硬件设备类型，自动选择第一个可用的解码器
std::pair<std::optional<Codec>, HWAccelUPtr> HWAccel::findDecoderWithHwAccel(AVCodecID id)
{
    for (const auto type : decodingDeviceTypes()) {
        const std::optional<Codec> codec = findAVDecoder(id, pixelFormatForHwDevice(type));
        if (!codec)
            continue;
        HWAccelUPtr hwAccel = create(type);
        if (!hwAccel)
            continue;
        return { codec, std::move(hwAccel) };
    }

    return { std::nullopt, nullptr };
}

// 指定设备类型，由 FFmpeg 自行创建硬件设备
std::pair<std::optional<Codec>, HWAccelUPtr> HWAccel::findDecoderWithHwAccel(AVCodecID id, AVHWDeviceType hwDeviceType)
{
    if (hwDeviceType == AV_HWDEVICE_TYPE_NONE)
        return { std::nullopt, nullptr };

    const std::optional<Codec> codec = findAVDecoder(id, pixelFormatForHwDevice(hwDeviceType));

    if (!codec) {
        return { std::nullopt, nullptr };
    }

    HWAccelUPtr hwAccel = create(hwDeviceType);

    if (!hwAccel)
        return { std::nullopt, nullptr };

    return { codec, std::move(hwAccel) };
}

// 指定设备类型 + 外部 RHI 设备（zero-copy 路径），复用 RHI 设备
std::pair<std::optional<Codec>, HWAccelUPtr> HWAccel::findDecoderWithHwAccel(
    AVCodecID id, AVHWDeviceType hwDeviceType, QRhi *rhi)
{
    if (hwDeviceType == AV_HWDEVICE_TYPE_NONE)
        return { std::nullopt, nullptr };

    const std::optional<Codec> codec = findAVDecoder(id, pixelFormatForHwDevice(hwDeviceType));

    if (!codec) {
        return { std::nullopt, nullptr };
    }
    HWAccelUPtr hwAccel = createWithExternalDevice(hwDeviceType, rhi);

    if (!hwAccel)
        return { std::nullopt, nullptr };

    return { codec, std::move(hwAccel) };
}

static bool isNoConversionFormat(AVPixelFormat f)
{
    bool needsConversion = true;
    VideoBuffer::toQtPixelFormat(f, &needsConversion);
    return !needsConversion;
};

AVPixelFormat getFormat(AVCodecContext *codecContext, const AVPixelFormat *fmt)
{
    const std::span<const AVPixelFormat> suggestedFormats = makeSpan(fmt);

    if (codecContext->hw_device_ctx)
    {
        const auto *device_ctx = reinterpret_cast<AVHWDeviceContext*>(codecContext->hw_device_ctx->data);
        ValueAndScore<AVPixelFormat> formatAndScore;

        for (const Codec codec{ codecContext->codec }; const AVCodecHWConfig *config : codec.hwConfigs())
        {
            if (!(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX))
                continue;

            if (device_ctx->type != config->device_type)
                continue;

            const bool isDeprecated = (config->methods & AV_CODEC_HW_CONFIG_METHOD_AD_HOC) != 0;
            const bool shouldCheckCodecFormats = config->pix_fmt == AV_PIX_FMT_NONE;

            auto scoresGettor = [&](AVPixelFormat format) {
                const auto pixelFormats = codec.pixelFormats();
                if (shouldCheckCodecFormats && !hasValue(pixelFormats, format))
                    return NotSuitableAVScore;

                if (!shouldCheckCodecFormats && config->pix_fmt != format)
                    return NotSuitableAVScore;

                auto result = DefaultAVScore;

                if (isDeprecated)
                    result -= 10000;
                if (isHwPixelFormat(format))
                    result += 10;

                return result;
            };

            const auto found = findBestAVValueWithScore(suggestedFormats, scoresGettor);

            if (found.score > formatAndScore.score)
                formatAndScore = found;
        }

        if (const auto format = formatAndScore.value) {
            TextureConverter::applyDecoderPreset(*format, *codecContext);
            qz::Log::cat_debug(qLHWAccel, "Selected format {} for hw {}", static_cast<int>(*format), static_cast<int>(device_ctx->type));
            return *format;
        }
    }

    if (const auto noConversionFormat = findIf(suggestedFormats, &isNoConversionFormat))
        return *noConversionFormat;

    const AVPixelFormat format = !suggestedFormats.empty() ? suggestedFormats[0] : AV_PIX_FMT_NONE;

    return format;
}

HWAccel::~HWAccel() = default;

HWAccelUPtr HWAccel::create(AVHWDeviceType deviceType)
{
    if (auto ctx = loadHWContext(deviceType))
        return std::make_unique<HWAccel>(Pr{}, std::move(ctx));
    return {};
}

HWAccelUPtr HWAccel::createWithExternalDevice(AVHWDeviceType deviceType, QRhi *rhi)
{
    if (!rhi)
        return {};

#ifdef Q_OS_WINDOWS
    if (deviceType == AV_HWDEVICE_TYPE_D3D11VA && rhi->backend() == QRhi::D3D11) {
        if (auto ctx = createD3D11DeviceContextFromRhi(rhi))
            return std::make_unique<HWAccel>(Pr{}, std::move(ctx), true);
    }
#endif

#if QT_FFMPEG_HAS_VULKAN
    if (deviceType == AV_HWDEVICE_TYPE_VULKAN && rhi->backend() == QRhi::Vulkan) {
        if (auto ctx = createVulkanDeviceContextFromRhi(rhi))
            return std::make_unique<HWAccel>(Pr{}, std::move(ctx), true);
    }
#endif

    return {};
}

AVPixelFormat HWAccel::format(AVFrame *frame)
{
    if (!frame->hw_frames_ctx)
        return static_cast<AVPixelFormat>(frame->format);

    const auto *hwFramesContext = reinterpret_cast<AVHWFramesContext*>(frame->hw_frames_ctx->data);
    Q_ASSERT(hwFramesContext);
    return hwFramesContext->sw_format;
}

const std::vector<AVHWDeviceType> &HWAccel::encodingDeviceTypes()
{
    static const auto &result = deviceTypes("QT_FFMPEG_ENCODING_HW_DEVICE_TYPES");
    return result;
}

const std::vector<AVHWDeviceType> &HWAccel::decodingDeviceTypes()
{
    static const auto &result = deviceTypes("QT_FFMPEG_DECODING_HW_DEVICE_TYPES");
    return result;
}

AVHWDeviceContext *HWAccel::hwDeviceContext() const
{
    return m_hwDeviceContext ? reinterpret_cast<AVHWDeviceContext*>(m_hwDeviceContext->data) : nullptr;
}

AVPixelFormat HWAccel::hwFormat() const
{
    return pixelFormatForHwDevice(deviceType());
}

const AVHWFramesConstraints *HWAccel::constraints() const
{
    std::call_once(m_constraintsOnceFlag, [this]() {
        if (auto context = hwDeviceContextAsBuffer())
            m_constraints.reset(av_hwdevice_get_hwframe_constraints(context, nullptr));
    });

    return m_constraints.get();
}

bool HWAccel::matchesSizeContraints(QSize size) const
{
    const auto constraints = this->constraints();
    if (!constraints)
        return true;

    return size.width() >= constraints->min_width
            && size.height() >= constraints->min_height
            && size.width() <= constraints->max_width
            && size.height() <= constraints->max_height;
}

AVHWDeviceType HWAccel::deviceType() const
{
    return m_hwDeviceContext ? hwDeviceContext()->type : AV_HWDEVICE_TYPE_NONE;
}

void HWAccel::createFramesContext(AVPixelFormat swFormat, const QSize &size)
{
    if (m_hwFramesContext) {
        qz::Log::warn("Frames context has been already created!");
        return;
    }

    if (!m_hwDeviceContext)
        return;

    m_hwFramesContext.reset(av_hwframe_ctx_alloc(m_hwDeviceContext.get()));
    auto *c = reinterpret_cast<AVHWFramesContext*>(m_hwFramesContext->data);
    c->format = hwFormat();
    c->sw_format = swFormat;
    c->width = size.width();
    c->height = size.height();
    if (int err = av_hwframe_ctx_init(m_hwFramesContext.get()); err < 0)
        qz::Log::warn("failed to init HW frame context {}", err);
    else
        qz::Log::cat_debug(qLHWAccel, "Initialized frames context {} {} {}", size, static_cast<int>(c->format), static_cast<int>(c->sw_format));
}

AVHWFramesContext *HWAccel::hwFramesContext() const
{
    return m_hwFramesContext ? reinterpret_cast<AVHWFramesContext*>(m_hwFramesContext->data) : nullptr;
}

static void deleteHwFrameContextData(AVHWFramesContext *context)
{
    std::unique_ptr<HwFrameContextData> contextData(
            static_cast<HwFrameContextData *>(context->user_opaque));
    Q_ASSERT(contextData);

    if (contextData->avDeleter) {
        context->user_opaque = contextData->avUserOpaque;
        context->free = contextData->avDeleter;

        context->free(context);
    }
}

HwFrameContextData &HwFrameContextData::ensure(AVFrame &hwFrame)
{
    Q_ASSERT(hwFrame.hw_frames_ctx && hwFrame.hw_frames_ctx->data);

    const auto context = reinterpret_cast<AVHWFramesContext *>(hwFrame.hw_frames_ctx->data);

    if (context->free != deleteHwFrameContextData) {
        context->user_opaque = new HwFrameContextData{ context->free, context->user_opaque };
        context->free = deleteHwFrameContextData;
    } else {
        Q_ASSERT(context->user_opaque);
    }

    return *static_cast<HwFrameContextData *>(context->user_opaque);
}

AVFrameUPtr copyFromHwPool(AVFrameUPtr frame)
{
#if QT_FFMPEG_HAS_VULKAN
    // Check if this is a Vulkan frame first - Vulkan frames must go through
    // copyFromHwPoolVulkan regardless of platform
    if (frame && frame->format == AV_PIX_FMT_VULKAN) {
        return copyFromHwPoolVulkan(std::move(frame));
    }
#endif

#ifdef Q_OS_WINDOWS
    return copyFromHwPoolD3D11(std::move(frame));
#elif defined(Q_OS_ANDROID)
    // MediaCodec frames should be returned directly
    if (frame && frame->hw_frames_ctx) {
        auto *frameContext = reinterpret_cast<AVHWFramesContext *>(frame->hw_frames_ctx->data);
        if (frameContext && frameContext->device_ctx
            && frameContext->device_ctx->type == AV_HWDEVICE_TYPE_MEDIACODEC)
            return frame;
    }
#  if QT_FFMPEG_HAS_VULKAN
    return copyFromHwPoolVulkan(std::move(frame));
#  else
    return frame;
#  endif
#elif QT_FFMPEG_HAS_VULKAN
    return copyFromHwPoolVulkan(std::move(frame));
#else
    return frame;
#endif
}

}

QT_END_NAMESPACE
