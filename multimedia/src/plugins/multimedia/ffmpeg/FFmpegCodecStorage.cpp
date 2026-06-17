#include "FFmpegCodecStorage_p.h"

#include "FFmpeg_p.h"
#include "FFmpegHwAccel_p.h"

#include <qdebug.h>
import qzLog;

#include <algorithm>
#include <vector>
#include <array>

#include <unordered_set>

extern "C" {
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
}

QT_BEGIN_NAMESPACE

static qz::Log::LogCategory qLcCodecStorage("qz.multimedia.ffmpeg.codecstorage");

namespace ffmpeg {

namespace {

enum CodecStorageType {
    Encoders,
    Decoders,

    CodecStorageTypeCount
};

using CodecsStorage = std::vector<Codec>;

struct CodecsComparator
{
    bool operator()(const Codec &a, const Codec &b) const
    {
        return a.id() < b.id() || (a.id() == b.id() && a.isExperimental() < b.isExperimental());
    }

    bool operator()(const Codec &codec, AVCodecID id) const { return codec.id() < id; }
    bool operator()(AVCodecID id, const Codec &codec) const { return id < codec.id(); }
};

template <typename FlagNames>
QString flagsToString(int flags, const FlagNames &flagNames)
{
    QString result;
    int leftover = flags;
    for (const auto &flagAndName : flagNames)
        if ((flags & flagAndName.first) != 0) {
            leftover &= ~flagAndName.first;
            if (!result.isEmpty())
                result += u", ";
            result += QLatin1StringView(flagAndName.second);
        }

    if (leftover) {
        if (!result.isEmpty())
            result += u", ";
        result += QString::number(leftover, 16);
    }
    return result;
}

void dumpCodecInfo(const Codec &codec)
{
    using FlagNames = std::initializer_list<std::pair<int, const char *>>;
    const auto mediaType = codec.type() == AVMEDIA_TYPE_VIDEO ? "video"
            : codec.type() == AVMEDIA_TYPE_AUDIO              ? "audio"
            : codec.type() == AVMEDIA_TYPE_SUBTITLE           ? "subtitle"
                                                             : "other_type";

    const auto type = codec.isEncoder()
            ? codec.isDecoder() ? "encoder/decoder:" : "encoder:"
            : "decoder:";

    static const FlagNames capabilitiesNames = {
        { AV_CODEC_CAP_DRAW_HORIZ_BAND, "DRAW_HORIZ_BAND" },
        { AV_CODEC_CAP_DR1, "DRAW_HORIZ_DR1" },
        { AV_CODEC_CAP_DELAY, "DELAY" },
        { AV_CODEC_CAP_SMALL_LAST_FRAME, "SMALL_LAST_FRAME" },
#ifdef AV_CODEC_CAP_SUBFRAMES
        { AV_CODEC_CAP_SUBFRAMES, "SUBFRAMES" },
#endif
        { AV_CODEC_CAP_EXPERIMENTAL, "EXPERIMENTAL" },
        { AV_CODEC_CAP_CHANNEL_CONF, "CHANNEL_CONF" },
        { AV_CODEC_CAP_FRAME_THREADS, "FRAME_THREADS" },
        { AV_CODEC_CAP_SLICE_THREADS, "SLICE_THREADS" },
        { AV_CODEC_CAP_PARAM_CHANGE, "PARAM_CHANGE" },
#ifdef AV_CODEC_CAP_OTHER_THREADS
        { AV_CODEC_CAP_OTHER_THREADS, "OTHER_THREADS" },
#endif
        { AV_CODEC_CAP_VARIABLE_FRAME_SIZE, "VARIABLE_FRAME_SIZE" },
        { AV_CODEC_CAP_AVOID_PROBING, "AVOID_PROBING" },
        { AV_CODEC_CAP_HARDWARE, "HARDWARE" },
        { AV_CODEC_CAP_HYBRID, "HYBRID" },
        { AV_CODEC_CAP_ENCODER_REORDERED_OPAQUE, "ENCODER_REORDERED_OPAQUE" },
#ifdef AV_CODEC_CAP_ENCODER_FLUSH
        { AV_CODEC_CAP_ENCODER_FLUSH, "ENCODER_FLUSH" },
#endif
    };

    qz::Log::cat_debug(qLcCodecStorage, "{} {} {} id:{} capabilities:{}", mediaType, type, codec.name().data(), static_cast<int>(codec.id()), flagsToString(codec.capabilities(), capabilitiesNames));

    if (codec.type() == AVMEDIA_TYPE_VIDEO) {
        const auto pixelFormats = codec.pixelFormats();
        if (!pixelFormats.empty()) {
            static const FlagNames flagNames = {
                { AV_PIX_FMT_FLAG_BE, "BE" },
                { AV_PIX_FMT_FLAG_PAL, "PAL" },
                { AV_PIX_FMT_FLAG_BITSTREAM, "BITSTREAM" },
                { AV_PIX_FMT_FLAG_HWACCEL, "HWACCEL" },
                { AV_PIX_FMT_FLAG_PLANAR, "PLANAR" },
                { AV_PIX_FMT_FLAG_RGB, "RGB" },
                { AV_PIX_FMT_FLAG_ALPHA, "ALPHA" },
                { AV_PIX_FMT_FLAG_BAYER, "BAYER" },
                { AV_PIX_FMT_FLAG_FLOAT, "FLOAT" },
            };

            qz::Log::cat_debug(qLcCodecStorage, "  pixelFormats:");
            for (AVPixelFormat f : pixelFormats) {
                auto desc = av_pix_fmt_desc_get(f);
                qz::Log::cat_debug(qLcCodecStorage, "    id:{} {} depth:{} flags:{}", static_cast<int>(f), desc->name, desc->comp[0].depth, flagsToString(desc->flags, flagNames));
            }
        } else {
            qz::Log::cat_debug(qLcCodecStorage, "  pixelFormats: null");
        }
    } else if (codec.type() == AVMEDIA_TYPE_AUDIO) {
        const auto sampleFormats = codec.sampleFormats();
        if (!sampleFormats.empty()) {
            qz::Log::cat_debug(qLcCodecStorage, "  sampleFormats:");
            for (auto f : sampleFormats) {
                const auto name = av_get_sample_fmt_name(f);
                qz::Log::cat_debug(qLcCodecStorage, "    id:{} {} bytes_per_sample:{} is_planar:{}", static_cast<int>(f), name ? name : "unknown", av_get_bytes_per_sample(f), av_sample_fmt_is_planar(f));
            }
        } else {
            qz::Log::cat_debug(qLcCodecStorage, "  sampleFormats: null");
        }
    }

    const std::vector<const AVCodecHWConfig*> hwConfigs = codec.hwConfigs();
    if (!hwConfigs.empty()) {
        static const FlagNames hwConfigMethodNames = {
            { AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX, "HW_DEVICE_CTX" },
            { AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX, "HW_FRAMES_CTX" },
            { AV_CODEC_HW_CONFIG_METHOD_INTERNAL, "INTERNAL" },
            { AV_CODEC_HW_CONFIG_METHOD_AD_HOC, "AD_HOC" }
        };

        qz::Log::cat_debug(qLcCodecStorage, "  hw config:");
        for (const AVCodecHWConfig* config : hwConfigs) {
            const auto pixFmtForDevice = pixelFormatForHwDevice(config->device_type);
            auto pixFmtDesc = av_pix_fmt_desc_get(config->pix_fmt);
            auto pixFmtForDeviceDesc = av_pix_fmt_desc_get(pixFmtForDevice);
            qz::Log::cat_debug(qLcCodecStorage, "    device_type:{} pix_fmt:{} {} pixelFormatForHwDevice:{} {} hw_config_methods:{}", static_cast<int>(config->device_type), static_cast<int>(config->pix_fmt), pixFmtDesc ? pixFmtDesc->name : "unknown", static_cast<int>(pixelFormatForHwDevice(config->device_type)), pixFmtForDeviceDesc ? pixFmtForDeviceDesc->name : "unknown", flagsToString(config->methods, hwConfigMethodNames));
        }
    }
}

bool isCodecValid(const Codec &codec, const std::vector<AVHWDeviceType> &availableHwDeviceTypes,
                  const std::optional<std::unordered_set<AVCodecID>> &codecAvailableOnDevice)
{
    if (codec.type() != AVMEDIA_TYPE_VIDEO)
        return true;

    const auto pixelFormats = codec.pixelFormats();
    if (pixelFormats.empty()) {
#if defined(Q_OS_LINUX)
        if (codec.name().contains(QLatin1StringView{ "_v4l2m2m" }) && codec.isEncoder())
            return false;

        if (codec.name().contains(QLatin1StringView{ "_mediacodec" })
            && (codec.capabilities() & AV_CODEC_CAP_HARDWARE)
            && codecAvailableOnDevice && codecAvailableOnDevice->count(codec.id()) == 0)
            return false;
#endif

#if defined(Q_OS_ANDROID)
        // Android 上过滤掉没有可用设备的 _mediacodec 解码器
        if (codec.name().contains(QLatin1StringView{ "_mediacodec" })
            && (codec.capabilities() & AV_CODEC_CAP_HARDWARE)
            && codecAvailableOnDevice && codecAvailableOnDevice->count(codec.id()) == 0)
            return false;
#endif

        return true;
    }

    if (!findAVPixelFormat(codec, &isHwPixelFormat))
        return true;

    if ((codec.capabilities() & AV_CODEC_CAP_HARDWARE) == 0)
        return true;

    if (codecAvailableOnDevice && !codecAvailableOnDevice->contains(codec.id()))
        return false;

    auto checkDeviceType = [codec](const AVHWDeviceType type) {
        return isAVFormatSupported(codec, pixelFormatForHwDevice(type));
    };

    return std::ranges::any_of(availableHwDeviceTypes, checkDeviceType);
}

std::optional<std::unordered_set<AVCodecID>> availableHWCodecs(const CodecStorageType type)
{
    Q_UNUSED(type);
    return {};
}

const CodecsStorage &codecsStorage(CodecStorageType codecsType)
{
    static const auto &storages = []() {
        std::array<CodecsStorage, CodecStorageTypeCount> result;
        const auto platformHwEncoders = availableHWCodecs(Encoders);
        const auto platformHwDecoders = availableHWCodecs(Decoders);

        for (const Codec codec : CodecEnumerator()) {
            static const auto experimentalCodecsEnabled =
                    qEnvironmentVariableIntValue("QT_ENABLE_EXPERIMENTAL_CODECS")
#ifdef Q_OS_ANDROID
                    || true // Android 上默认启用实验性编解码器，确保 SW 解码器可用
#endif
                    ;

            if (!experimentalCodecsEnabled && codec.isExperimental()) {
                qz::Log::cat_debug(qLcCodecStorage, "Skip experimental codec {}", codec.name().data());
                continue;
            }

            if (codec.isDecoder()) {
                if (isCodecValid(codec, HWAccel::decodingDeviceTypes(), platformHwDecoders))
                    result[Decoders].emplace_back(codec);
                else
                    qz::Log::cat_debug(qLcCodecStorage, "Skip decoder {} due to disabled matching hw acceleration, or dysfunctional codec", codec.name().data());
            }

            if (codec.isEncoder()) {
                if (isCodecValid(codec, HWAccel::encodingDeviceTypes(), platformHwEncoders))
                    result[Encoders].emplace_back(codec);
                else
                    qz::Log::cat_debug(qLcCodecStorage, "Skip encoder {} due to disabled matching hw acceleration, or dysfunctional codec", codec.name().data());
            }
        }

        for (auto &storage : result) {
            storage.shrink_to_fit();
            std::ranges::stable_sort(storage, CodecsComparator{});
        }

#ifdef Q_OS_ANDROID
        // Android 上检查主流视频编解码器的 SW 解码器可用性
        {
            struct CodecCheck { AVCodecID id; const char *name; };
            const CodecCheck mainVideoCodecs[] = {
                { AV_CODEC_ID_H264,       "H264"       },
                { AV_CODEC_ID_H265,       "H265/HEVC"  },
                { AV_CODEC_ID_H263,       "H263"       },
                { AV_CODEC_ID_AV1,        "AV1"        },
                { AV_CODEC_ID_VP8,        "VP8"        },
                { AV_CODEC_ID_VP9,        "VP9"        },
                { AV_CODEC_ID_MPEG4,      "MPEG4"      },
                { AV_CODEC_ID_MPEG2VIDEO, "MPEG2"      },
            };

            qz::Log::info("Android FFmpeg SW decoder availability check:");
            for (const auto &check : mainVideoCodecs) {
                const auto &decoders = result[Decoders];
                auto begin = std::lower_bound(decoders.begin(), decoders.end(), check.id, CodecsComparator{});
                auto end = std::upper_bound(begin, decoders.end(), check.id, CodecsComparator{});

                bool hasSwDecoder = false;
                QString hwDecoderNames;
                QString swDecoderNames;

                for (auto it = begin; it != end; ++it) {
                    const auto &codec = *it;
                    const bool isHw = (codec.capabilities() & AV_CODEC_CAP_HARDWARE) != 0;
                    if (isHw) {
                        if (!hwDecoderNames.isEmpty())
                            hwDecoderNames += u", ";
                        hwDecoderNames += QString::fromUtf8(codec.name().data());
                    } else {
                        hasSwDecoder = true;
                        if (!swDecoderNames.isEmpty())
                            swDecoderNames += u", ";
                        swDecoderNames += QString::fromUtf8(codec.name().data());
                    }
                }

                if (hasSwDecoder) {
                    qz::Log::info("  {} : SW=YES ({}) HW={}", check.name, swDecoderNames,
                                   hwDecoderNames.isEmpty() ? QStringLiteral("none") : hwDecoderNames);
                } else if (!hwDecoderNames.isEmpty()) {
                    qz::Log::info("  {} : SW=NO  HW={} (no software fallback!)", check.name, hwDecoderNames);
                } else {
                    qz::Log::info("  {} : SW=NO  HW=none (not available)", check.name);
                }
            }
        }
#endif

        return result;
    }();

    return storages[codecsType];
}

template <typename CodecScoreGetter, typename CodecOpener>
bool findAndOpenCodec(CodecStorageType codecsType, AVCodecID codecId,
                      const CodecScoreGetter &scoreGetter, const CodecOpener &opener)
{
    Q_ASSERT(opener);
    const auto &storage = codecsStorage(codecsType);
    auto it = std::lower_bound(storage.begin(), storage.end(), codecId, CodecsComparator{});

    using CodecToScore = std::pair<Codec, AVScore>;
    std::vector<CodecToScore> codecsToScores;

    for (; it != storage.end() && it->id()  == codecId; ++it) {
        const AVScore score = scoreGetter ? scoreGetter(*it) : DefaultAVScore;
        if (score != NotSuitableAVScore)
            codecsToScores.emplace_back(*it, score);
    }

    if (scoreGetter) {
        std::stable_sort(
                codecsToScores.begin(), codecsToScores.end(),
                [](const CodecToScore &a, const CodecToScore &b) { return a.second > b.second; });
    }

    auto open = [&opener](const CodecToScore &codecToScore) { return opener(codecToScore.first); };

    return std::any_of(codecsToScores.begin(), codecsToScores.end(), open);
}

std::optional<Codec> findAVCodec(CodecStorageType codecsType, AVCodecID codecId,
                                 const std::optional<PixelOrSampleFormat> &format)
{
    const CodecsStorage& storage = codecsStorage(codecsType);

    auto begin = std::lower_bound(storage.begin(), storage.end(), codecId, CodecsComparator{});
    auto end = std::upper_bound(begin, storage.end(), codecId, CodecsComparator{});

    auto codecIt = std::find_if(begin, end, [&format](const Codec &codec) {
        return !format || isAVFormatSupported(codec, *format);
    });

    if (codecIt != end)
        return *codecIt;

    return {};
}

}

std::optional<Codec> findAVDecoder(AVCodecID codecId,
                                   const std::optional<PixelOrSampleFormat> &format)
{
    return findAVCodec(Decoders, codecId, format);
}

std::optional<Codec> findAVSoftwareDecoder(AVCodecID codecId,
                                           const std::optional<PixelOrSampleFormat> &format)
{
    const CodecsStorage &storage = codecsStorage(Decoders);

    const auto begin = std::lower_bound(storage.begin(), storage.end(), codecId, CodecsComparator{});
    const auto end = std::upper_bound(begin, storage.end(), codecId, CodecsComparator{});

    const auto codecIt = std::find_if(begin, end, [&format](const Codec &codec) {
        if (codec.capabilities() & AV_CODEC_CAP_HARDWARE)
            return false;
        return !format || isAVFormatSupported(codec, *format);
    });

    if (codecIt != end)
        return *codecIt;

    return {};
}

std::optional<Codec> findAVEncoder(AVCodecID codecId, const std::optional<PixelOrSampleFormat> &format)
{
    return findAVCodec(Encoders, codecId, format);
}

bool findAndOpenAVDecoder(AVCodecID codecId,
                          const std::function<AVScore(const Codec &)> &scoresGetter,
                          const std::function<bool(const Codec &)> &codecOpener)
{
    return findAndOpenCodec(Decoders, codecId, scoresGetter, codecOpener);
}

bool findAndOpenAVEncoder(AVCodecID codecId,
                          const std::function<AVScore(const Codec &)> &scoresGetter,
                          const std::function<bool(const Codec &)> &codecOpener)
{
    return findAndOpenCodec(Encoders, codecId, scoresGetter, codecOpener);
}

}

QT_END_NAMESPACE
