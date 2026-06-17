#ifndef FFMPEGCODEC_P_H
#define FFMPEGCODEC_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegDefs_p.h>

#include <QtCore/qlatin1stringview.h>
#include <span>

#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
}

QT_BEGIN_NAMESPACE

namespace ffmpeg {

// 编解码器封装类，提供编解码器信息和能力查询接口。
class Codec
{
public:
    explicit Codec(const AVCodec *codec);

    [[nodiscard]] const AVCodec *get() const noexcept;
    [[nodiscard]] AVCodecID id() const noexcept;
    [[nodiscard]] QLatin1StringView name() const noexcept;
    [[nodiscard]] AVMediaType type() const noexcept;
    [[nodiscard]] int capabilities() const noexcept;
    [[nodiscard]] bool isEncoder() const noexcept;
    [[nodiscard]] bool isDecoder() const noexcept;
    [[nodiscard]] bool isExperimental() const noexcept;
    [[nodiscard]] std::span<const AVPixelFormat> pixelFormats() const noexcept;
    [[nodiscard]] std::span<const AVSampleFormat> sampleFormats() const noexcept;
    [[nodiscard]] std::span<const int> sampleRates() const noexcept;
    [[nodiscard]] std::span<const ChannelLayoutT> channelLayouts() const noexcept;
    [[nodiscard]] std::span<const AVRational> frameRates() const noexcept;
    [[nodiscard]] std::vector<const AVCodecHWConfig *> hwConfigs() const noexcept;

private:
    const AVCodec *m_codec = nullptr;
};

// 编解码器迭代器，遍历 FFmpeg 注册的所有编解码器
class CodecIterator
{
public:
    static CodecIterator begin();
    static CodecIterator end();

    CodecIterator &operator++() noexcept;
    [[nodiscard]] Codec operator*() const noexcept;
    [[nodiscard]] bool operator!=(const CodecIterator &other) const noexcept;

private:
    void *m_state = nullptr;
    const AVCodec *m_codec = nullptr;
};

using CodecEnumerator = CodecIterator;

std::span<const AVPixelFormat> makeSpan(const AVPixelFormat *values);

}

QT_END_NAMESPACE

#endif
