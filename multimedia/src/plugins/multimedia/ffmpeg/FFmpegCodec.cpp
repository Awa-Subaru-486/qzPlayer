#include "FFmpegCodec_p.h"
#include "FFmpeg_p.h"

import qzLog;

QT_BEGIN_NAMESPACE

namespace ffmpeg {
namespace {

template <typename T>
inline constexpr auto InvalidAvValue = T{};

template <>
inline constexpr auto InvalidAvValue<AVSampleFormat> = AV_SAMPLE_FMT_NONE;

template <>
inline constexpr auto InvalidAvValue<AVPixelFormat> = AV_PIX_FMT_NONE;

template <typename T>
std::span<const T> makeSpan(const T *values)
{
    if (!values)
        return {};

    qsizetype size = 0;
    while (values[size] != InvalidAvValue<T>)
        ++size;

    return std::span<const T>{ values, static_cast<size_t>(size) };
}

#if QT_FFMPEG_HAS_AVCODEC_GET_SUPPORTED_CONFIG

static qz::Log::LogCategory qLcFFmpegUtils("qz.multimedia.ffmpeg.utils");

void logGetCodecConfigError(const AVCodec *codec, AVCodecConfig config, int error)
{
    qz::Log::cat_warn(qLcFFmpegUtils, "Failed to retrieve config {} for codec {} with error {}", static_cast<int>(config), codec->name, static_cast<int>(AVError(error)));
}

template <typename T>
std::span<const T> getCodecConfig(const AVCodec *codec, AVCodecConfig config)
{
    const T *result = nullptr;
    int size = 0;
    const auto error = avcodec_get_supported_config(
            nullptr, codec, config, 0u, reinterpret_cast<const void **>(&result), &size);
    if (error != 0) {
        logGetCodecConfigError(codec, config, error);
        return {};
    }

    Q_ASSERT(!result || (size > 0 && result[size] == InvalidAvValue<T>));

    return std::span<const T>{ result, static_cast<size_t>(size) };
}
#endif

std::span<const AVPixelFormat> getCodecPixelFormats(const AVCodec *codec)
{
#if QT_FFMPEG_HAS_AVCODEC_GET_SUPPORTED_CONFIG
    return getCodecConfig<AVPixelFormat>(codec, AV_CODEC_CONFIG_PIX_FORMAT);
#else
    return makeSpan(codec->pix_fmts);
#endif
}

std::span<const AVSampleFormat> getCodecSampleFormats(const AVCodec *codec)
{
#if QT_FFMPEG_HAS_AVCODEC_GET_SUPPORTED_CONFIG
    return getCodecConfig<AVSampleFormat>(codec, AV_CODEC_CONFIG_SAMPLE_FORMAT);
#else
    return makeSpan(codec->sample_fmts);
#endif
}

std::span<const int> getCodecSampleRates(const AVCodec *codec)
{
#if QT_FFMPEG_HAS_AVCODEC_GET_SUPPORTED_CONFIG
    return getCodecConfig<int>(codec, AV_CODEC_CONFIG_SAMPLE_RATE);
#else
    return makeSpan(codec->supported_samplerates);
#endif
}

#ifdef Q_OS_WINDOWS

auto stereoLayout()
{
    constexpr uint64_t mask = AV_CH_FRONT_LEFT | AV_CH_FRONT_RIGHT;
#if QT_FFMPEG_HAS_AV_CHANNEL_LAYOUT
    AVChannelLayout channelLayout{};
    av_channel_layout_from_mask(&channelLayout, mask);
    return channelLayout;
#else
    return mask;
#endif
};

#endif

std::span<const ChannelLayoutT> getCodecChannelLayouts(const AVCodec *codec)
{
    std::span<const ChannelLayoutT> layout;
#if QT_FFMPEG_HAS_AVCODEC_GET_SUPPORTED_CONFIG
    layout = getCodecConfig<AVChannelLayout>(codec, AV_CODEC_CONFIG_CHANNEL_LAYOUT);
#elif QT_FFMPEG_HAS_AV_CHANNEL_LAYOUT
    layout = makeSpan(codec->ch_layouts);
#else
    layout = makeSpan(codec->channel_layouts);
#endif

#ifdef Q_OS_WINDOWS
    if (layout.empty() && QLatin1StringView(codec->name) == QLatin1StringView("mp3_mf")) {
        static const ChannelLayoutT defaultLayout[] = { stereoLayout() };
        layout = defaultLayout;
    }
#endif
    return layout;
}

std::span<const AVRational> getCodecFrameRates(const AVCodec *codec)
{
#if QT_FFMPEG_HAS_AVCODEC_GET_SUPPORTED_CONFIG
    return getCodecConfig<AVRational>(codec, AV_CODEC_CONFIG_FRAME_RATE);
#else
    return makeSpan(codec->supported_framerates);
#endif
}
}

Codec::Codec(const AVCodec *codec) : m_codec{ codec }
{
    Q_ASSERT(m_codec);
}

const AVCodec* Codec::get() const noexcept
{
    Q_ASSERT(m_codec);
    return m_codec;
}

AVCodecID Codec::id() const noexcept
{
    Q_ASSERT(m_codec);

    return m_codec->id;
}

QLatin1StringView Codec::name() const noexcept
{
    Q_ASSERT(m_codec);

    return QLatin1StringView{ m_codec->name };
}

AVMediaType Codec::type() const noexcept
{
    Q_ASSERT(m_codec);

    return m_codec->type;
}

int Codec::capabilities() const noexcept
{
    Q_ASSERT(m_codec);

    return m_codec->capabilities;
}

bool Codec::isEncoder() const noexcept
{
    Q_ASSERT(m_codec);

    return av_codec_is_encoder(m_codec) != 0;
}

bool Codec::isDecoder() const noexcept
{
    Q_ASSERT(m_codec);

    return av_codec_is_decoder(m_codec) != 0;
}

bool Codec::isExperimental() const noexcept
{
    Q_ASSERT(m_codec);

    return (m_codec->capabilities & AV_CODEC_CAP_EXPERIMENTAL) != 0;
}

std::span<const AVPixelFormat> Codec::pixelFormats() const noexcept
{
    Q_ASSERT(m_codec);

    if (m_codec->type != AVMEDIA_TYPE_VIDEO)
        return {};

    return getCodecPixelFormats(m_codec);
}

std::span<const AVSampleFormat> Codec::sampleFormats() const noexcept
{
    Q_ASSERT(m_codec);

    if (m_codec->type != AVMEDIA_TYPE_AUDIO)
        return {};

    return getCodecSampleFormats(m_codec);
}

std::span<const int> Codec::sampleRates() const noexcept
{
    Q_ASSERT(m_codec);

    if (m_codec->type != AVMEDIA_TYPE_AUDIO)
        return {};

    return getCodecSampleRates(m_codec);
}

std::span<const ChannelLayoutT> Codec::channelLayouts() const noexcept
{
    Q_ASSERT(m_codec);

    if (m_codec->type != AVMEDIA_TYPE_AUDIO)
        return {};

    return getCodecChannelLayouts(m_codec);
}

std::span<const AVRational> Codec::frameRates() const noexcept
{
    Q_ASSERT(m_codec);

    if (m_codec->type != AVMEDIA_TYPE_VIDEO)
        return {};

    return getCodecFrameRates(m_codec);
}

std::vector<const AVCodecHWConfig *> Codec::hwConfigs() const noexcept
{
    Q_ASSERT(m_codec);

    std::vector<const AVCodecHWConfig *> configs;

    for (int index = 0; auto config = avcodec_get_hw_config(m_codec, index); ++index)
        configs.push_back(config);

    return configs;
}

CodecIterator CodecIterator::begin()
{
    CodecIterator iterator;
    iterator.m_codec = av_codec_iterate(&iterator.m_state);
    return iterator;
}

CodecIterator CodecIterator::end()
{
    return { };
}

CodecIterator &CodecIterator::operator++() noexcept
{
    Q_ASSERT(m_codec);
    m_codec = av_codec_iterate(&m_state);
    return *this;
}

Codec CodecIterator::operator*() const noexcept
{
    Q_ASSERT(m_codec);
    return Codec{ m_codec };
}

bool CodecIterator::operator!=(const CodecIterator &other) const noexcept
{
    return m_codec != other.m_codec;
}

std::span<const AVPixelFormat> makeSpan(const AVPixelFormat *values)
{
    return makeSpan<AVPixelFormat>(values);
}

}

QT_END_NAMESPACE
