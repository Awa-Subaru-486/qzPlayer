#include "FFmpegMediaFormatInfo_p.h"
#include "FFmpegCodecStorage_p.h"
#include "AudioFormat.h"

QT_BEGIN_NAMESPACE

namespace ffmpeg {
static constexpr struct {
    AVCodecID id;
    ::MediaFormat::VideoCodec codec;
} s_videoCodecMap [] = {
    { AV_CODEC_ID_MPEG1VIDEO, ::MediaFormat::VideoCodec::MPEG1 },
    { AV_CODEC_ID_MPEG2VIDEO, ::MediaFormat::VideoCodec::MPEG2 },
    { AV_CODEC_ID_MPEG4, ::MediaFormat::VideoCodec::MPEG4 },
    { AV_CODEC_ID_H264, ::MediaFormat::VideoCodec::H264 },
    { AV_CODEC_ID_HEVC, ::MediaFormat::VideoCodec::H265 },
    { AV_CODEC_ID_VP8, ::MediaFormat::VideoCodec::VP8 },
    { AV_CODEC_ID_VP9, ::MediaFormat::VideoCodec::VP9 },
    { AV_CODEC_ID_AV1, ::MediaFormat::VideoCodec::AV1 },
    { AV_CODEC_ID_THEORA, ::MediaFormat::VideoCodec::Theora },
    { AV_CODEC_ID_WMV3, ::MediaFormat::VideoCodec::WMV },
    { AV_CODEC_ID_MJPEG, ::MediaFormat::VideoCodec::MotionJPEG }
};

static AVCodecID codecId(::MediaFormat::VideoCodec codec)
{
    for (const auto &c : s_videoCodecMap) {
        if (c.codec == codec)
            return c.id;
    }
    return AV_CODEC_ID_NONE;
}

static constexpr struct {
    AVCodecID id;
    ::MediaFormat::AudioCodec codec;
} s_audioCodecMap [] = {
    { AV_CODEC_ID_MP3, ::MediaFormat::AudioCodec::MP3 },
    { AV_CODEC_ID_AAC, ::MediaFormat::AudioCodec::AAC },
    { AV_CODEC_ID_AC3, ::MediaFormat::AudioCodec::AC3 },
    { AV_CODEC_ID_EAC3, ::MediaFormat::AudioCodec::EAC3 },
    { AV_CODEC_ID_FLAC, ::MediaFormat::AudioCodec::FLAC },
    { AV_CODEC_ID_TRUEHD, ::MediaFormat::AudioCodec::DolbyTrueHD },
    { AV_CODEC_ID_OPUS, ::MediaFormat::AudioCodec::Opus },
    { AV_CODEC_ID_VORBIS, ::MediaFormat::AudioCodec::Vorbis },
    { AV_CODEC_ID_PCM_S16LE, ::MediaFormat::AudioCodec::Wave },
    { AV_CODEC_ID_WMAPRO, ::MediaFormat::AudioCodec::WMA },
    { AV_CODEC_ID_ALAC, ::MediaFormat::AudioCodec::ALAC }
};

static AVCodecID codecId(::MediaFormat::AudioCodec codec)
{
    for (const auto &c : s_audioCodecMap) {
        if (c.codec == codec)
            return c.id;
    }
    return AV_CODEC_ID_NONE;
}

static constexpr struct
{
    ::MediaFormat::FileFormat fileFormat;
    const char *mimeType;
    const char *name;
} s_mimeMap[] = {
    { ::MediaFormat::WMV, "video/x-ms-asf", "asf" },
    { ::MediaFormat::AVI, "video/x-msvideo", nullptr },
    { ::MediaFormat::Matroska, "video/x-matroska", nullptr },
    { ::MediaFormat::MPEG4, "video/mp4", "mp4" },
    { ::MediaFormat::Ogg, "video/ogg", nullptr },

    { ::MediaFormat::WebM, "video/webm", "webm" },

    { ::MediaFormat::AAC, "audio/aac", nullptr },

    { ::MediaFormat::FLAC, "audio/x-flac", nullptr },
    { ::MediaFormat::MP3, "audio/mpeg", "mp3" },
    { ::MediaFormat::Wave, "audio/x-wav", nullptr }
};

template <typename AVFormat>
static ::MediaFormat::FileFormat formatForAVFormat(AVFormat *format)
{
    if (!format->mime_type || !*format->mime_type)
        return ::MediaFormat::UnspecifiedFormat;

    for (const auto &m : s_mimeMap) {
        if (m.mimeType && !strcmp(m.mimeType, format->mime_type)) {

            if (!m.name || !strcmp(m.name, format->name))
                return m.fileFormat;
        }
    }
    return ::MediaFormat::UnspecifiedFormat;
}

static const AVOutputFormat *avFormatForFormat(::MediaFormat::FileFormat format)
{
    if (format == ::MediaFormat::QuickTime || format == ::MediaFormat::Mpeg4Audio)
        format = ::MediaFormat::MPEG4;
    if (format == ::MediaFormat::WMA)
        format = ::MediaFormat::WMV;

    for (const auto &m : s_mimeMap) {
        if (m.fileFormat == format)
            return av_guess_format(m.name, nullptr, m.mimeType);
    }

    return nullptr;
}

MediaFormatInfo::MediaFormatInfo()
{
    using VideoCodec = ::MediaFormat::VideoCodec;
    using AudioCodec = ::MediaFormat::AudioCodec;

    QList<AudioCodec> audioEncoders;
    QList<AudioCodec> extraAudioDecoders;
    QList<VideoCodec> videoEncoders;
    QList<VideoCodec> extraVideoDecoders;

    const AVCodecDescriptor *descriptor = nullptr;
    while ((descriptor = avcodec_descriptor_next(descriptor))) {

        const bool canEncode{ findAVEncoder(descriptor->id).has_value() };
        const bool canDecode{ findAVDecoder(descriptor->id).has_value() };

        const VideoCodec videoCodec = videoCodecForAVCodecId(descriptor->id);
        const AudioCodec audioCodec = audioCodecForAVCodecId(descriptor->id);

        if (descriptor->type == AVMEDIA_TYPE_VIDEO && videoCodec != VideoCodec::Unspecified) {
            if (canEncode) {
                if (!videoEncoders.contains(videoCodec))
                    videoEncoders.append(videoCodec);
            } else if (canDecode) {
                if (!extraVideoDecoders.contains(videoCodec))
                    extraVideoDecoders.append(videoCodec);
            }
        } else if (descriptor->type == AVMEDIA_TYPE_AUDIO && audioCodec != AudioCodec::Unspecified) {
            if (canEncode) {
                if (!audioEncoders.contains(audioCodec))
                    audioEncoders.append(audioCodec);
            } else if (canDecode) {
                if (!extraAudioDecoders.contains(audioCodec))
                    extraAudioDecoders.append(audioCodec);
            }
        }
    }

    void *opaque = nullptr;
    const AVOutputFormat *outputFormat = nullptr;
    while ((outputFormat = av_muxer_iterate(&opaque))) {
        ::MediaFormat::FileFormat mediaFormat = formatForAVFormat(outputFormat);
        if (mediaFormat == ::MediaFormat::UnspecifiedFormat)
            continue;

        CodecMap encoder;
        encoder.format = mediaFormat;

        for (AudioCodec codec : audioEncoders) {
            const AVCodecID id = codecId(codec);

            const int result = avformat_query_codec(outputFormat, id, FF_COMPLIANCE_NORMAL);
            if (result == 1 || (result < 0 && id == outputFormat->audio_codec)) {

                encoder.audio.append(codec);
            }
        }

        for (VideoCodec codec : videoEncoders) {
            const AVCodecID id = codecId(codec);

            const int result = avformat_query_codec(outputFormat, id, FF_COMPLIANCE_NORMAL);
            if (result == 1 || (result < 0 && id == outputFormat->video_codec)) {

                encoder.video.append(codec);
            }
        }

        if (encoder.audio.empty() && encoder.video.empty())
            continue;

        switch (encoder.format) {
        case ::MediaFormat::WMV:

            encoders.append({ ::MediaFormat::WMA, encoder.audio, {} });
            break;
        case ::MediaFormat::MPEG4:

            encoders.append({ ::MediaFormat::QuickTime, encoder.audio, encoder.video });
            encoders.append({ ::MediaFormat::Mpeg4Audio, encoder.audio, {} });
            break;
        case ::MediaFormat::Wave:

            if (!encoder.audio.contains(AudioCodec::Wave))
                continue;
            encoder.audio = { AudioCodec::Wave };
            break;
        default:
            break;
        }

        encoders.append(encoder);
    }

    decoders = encoders;

#ifdef Q_OS_WINDOWS

    for (auto &encoder : encoders) {
        auto h265index = encoder.video.indexOf(VideoCodec::H265);
        if (h265index >= 0)
            encoder.video.removeAt(h265index);
    }
#endif

    for (auto &encoder : encoders) {
        if (encoder.format == ::MediaFormat::Matroska) {
            encoder.video.removeAll(VideoCodec::H264);
        }
    }

    if (extraAudioDecoders.contains(AudioCodec::WMA)) {
        decoders[::MediaFormat::WMA].audio.append(AudioCodec::WMA);
        decoders[::MediaFormat::WMV].audio.append(AudioCodec::WMA);
    }

    if (extraVideoDecoders.contains(VideoCodec::WMV)) {
        decoders[::MediaFormat::WMV].video.append(VideoCodec::WMV);
    }

}

MediaFormatInfo::~MediaFormatInfo() = default;

::MediaFormat::AudioCodec MediaFormatInfo::audioCodecForAVCodecId(AVCodecID id)
{
    for (const auto &c : s_audioCodecMap) {
        if (c.id == id)
            return c.codec;
    }
    return ::MediaFormat::AudioCodec::Unspecified;
}

::MediaFormat::VideoCodec MediaFormatInfo::videoCodecForAVCodecId(AVCodecID id)
{
    for (const auto &c : s_videoCodecMap) {
        if (c.id == id)
            return c.codec;
    }
    return ::MediaFormat::VideoCodec::Unspecified;
}

::MediaFormat::FileFormat
MediaFormatInfo::fileFormatForAVInputFormat(const AVInputFormat &format)
{

    static const struct
    {
        ::MediaFormat::FileFormat fileFormat;
        const char *name;
    } map[::MediaFormat::LastFileFormat + 1] = {
        { ::MediaFormat::WMV, "asf" },
        { ::MediaFormat::AVI, "avi" },
        { ::MediaFormat::Matroska, "matroska" },
        { ::MediaFormat::MPEG4, "mov" },
        { ::MediaFormat::Ogg, "ogg" },
        { ::MediaFormat::WebM, "webm" },

        { ::MediaFormat::AAC, "aac"},

        { ::MediaFormat::FLAC, "flac" },
        { ::MediaFormat::MP3, "mp3" },
        { ::MediaFormat::Wave, "wav" },
        { ::MediaFormat::UnspecifiedFormat, nullptr }
    };

    if (!format.name)
        return ::MediaFormat::UnspecifiedFormat;

    auto *m = map;
    while (m->fileFormat != ::MediaFormat::UnspecifiedFormat) {
        if (!strncmp(m->name, format.name, strlen(m->name)))
            return m->fileFormat;
        ++m;
    }

    return ::MediaFormat::UnspecifiedFormat;
}

const AVOutputFormat *
MediaFormatInfo::outputFormatForFileFormat(::MediaFormat::FileFormat format)
{
    return avFormatForFormat(format);
}

AVCodecID MediaFormatInfo::codecIdForVideoCodec(::MediaFormat::VideoCodec codec)
{
    return codecId(codec);
}

AVCodecID MediaFormatInfo::codecIdForAudioCodec(::MediaFormat::AudioCodec codec)
{
    return codecId(codec);
}

::AudioFormat::SampleFormat MediaFormatInfo::sampleFormat(AVSampleFormat format)
{
    switch (format) {
    case AV_SAMPLE_FMT_NONE:
    default:
        return ::AudioFormat::Unknown;
    case AV_SAMPLE_FMT_U8:
    case AV_SAMPLE_FMT_U8P:
            return ::AudioFormat::UInt8;
    case AV_SAMPLE_FMT_S16:
    case AV_SAMPLE_FMT_S16P:
        return ::AudioFormat::Int16;
    case AV_SAMPLE_FMT_S32:
    case AV_SAMPLE_FMT_S32P:
        return ::AudioFormat::Int32;
    case AV_SAMPLE_FMT_FLT:
    case AV_SAMPLE_FMT_FLTP:
        return ::AudioFormat::Float;
    case AV_SAMPLE_FMT_DBL:
    case AV_SAMPLE_FMT_DBLP:
    case AV_SAMPLE_FMT_S64:
    case AV_SAMPLE_FMT_S64P:

        return ::AudioFormat::Float;
    }
}

AVSampleFormat MediaFormatInfo::avSampleFormat(::AudioFormat::SampleFormat format)
{
    switch (format) {
    case ::AudioFormat::UInt8:
        return AV_SAMPLE_FMT_U8;
    case ::AudioFormat::Int16:
        return AV_SAMPLE_FMT_S16;
    case ::AudioFormat::Int32:
        return AV_SAMPLE_FMT_S32;
    case ::AudioFormat::Float:
        return AV_SAMPLE_FMT_FLT;
    default:
        return AV_SAMPLE_FMT_NONE;
    }
}

int64_t MediaFormatInfo::avChannelLayout(::AudioFormat::ChannelConfig channelConfig)
{
    int64_t avChannelLayout = 0;
    if (channelConfig & (1 << ::AudioFormat::FrontLeft))
        avChannelLayout |= AV_CH_FRONT_LEFT;
    if (channelConfig & (1 << ::AudioFormat::FrontRight))
        avChannelLayout |= AV_CH_FRONT_RIGHT;
    if (channelConfig & (1 << ::AudioFormat::FrontCenter))
        avChannelLayout |= AV_CH_FRONT_CENTER;
    if (channelConfig & (1 << ::AudioFormat::LFE))
        avChannelLayout |= AV_CH_LOW_FREQUENCY;
    if (channelConfig & (1 << ::AudioFormat::BackLeft))
        avChannelLayout |= AV_CH_BACK_LEFT;
    if (channelConfig & (1 << ::AudioFormat::BackRight))
        avChannelLayout |= AV_CH_BACK_RIGHT;
    if (channelConfig & (1 << ::AudioFormat::FrontLeftOfCenter))
        avChannelLayout |= AV_CH_FRONT_LEFT_OF_CENTER;
    if (channelConfig & (1 << ::AudioFormat::FrontRightOfCenter))
        avChannelLayout |= AV_CH_FRONT_RIGHT_OF_CENTER;
    if (channelConfig & (1 << ::AudioFormat::BackCenter))
        avChannelLayout |= AV_CH_BACK_CENTER;
    if (channelConfig & (1 << ::AudioFormat::LFE2))
        avChannelLayout |= AV_CH_LOW_FREQUENCY_2;
    if (channelConfig & (1 << ::AudioFormat::SideLeft))
        avChannelLayout |= AV_CH_SIDE_LEFT;
    if (channelConfig & (1 << ::AudioFormat::SideRight))
        avChannelLayout |= AV_CH_SIDE_RIGHT;
    if (channelConfig & (1 << ::AudioFormat::TopFrontLeft))
        avChannelLayout |= AV_CH_TOP_FRONT_LEFT;
    if (channelConfig & (1 << ::AudioFormat::TopFrontRight))
        avChannelLayout |= AV_CH_TOP_FRONT_RIGHT;
    if (channelConfig & (1 << ::AudioFormat::TopFrontCenter))
        avChannelLayout |= AV_CH_TOP_FRONT_CENTER;
    if (channelConfig & (1 << ::AudioFormat::TopCenter))
        avChannelLayout |= AV_CH_TOP_CENTER;
    if (channelConfig & (1 << ::AudioFormat::TopBackLeft))
        avChannelLayout |= AV_CH_TOP_BACK_LEFT;
    if (channelConfig & (1 << ::AudioFormat::TopBackRight))
        avChannelLayout |= AV_CH_TOP_BACK_RIGHT;
    if (channelConfig & (1 << ::AudioFormat::TopBackCenter))
        avChannelLayout |= AV_CH_TOP_BACK_CENTER;

#ifdef AV_CH_TOP_SIDE_LEFT
    if (channelConfig & (1 << ::AudioFormat::TopSideLeft))
        avChannelLayout |= AV_CH_TOP_SIDE_LEFT;
    if (channelConfig & (1 << ::AudioFormat::TopSideRight))
        avChannelLayout |= AV_CH_TOP_SIDE_RIGHT;
    if (channelConfig & (1 << ::AudioFormat::BottomFrontCenter))
        avChannelLayout |= AV_CH_BOTTOM_FRONT_CENTER;
    if (channelConfig & (1 << ::AudioFormat::BottomFrontLeft))
        avChannelLayout |= AV_CH_BOTTOM_FRONT_LEFT;
    if (channelConfig & (1 << ::AudioFormat::BottomFrontRight))
        avChannelLayout |= AV_CH_BOTTOM_FRONT_RIGHT;
#endif
    return avChannelLayout;
}

::AudioFormat::ChannelConfig MediaFormatInfo::channelConfigForAVLayout(int64_t avChannelLayout)
{
    quint32 channelConfig = 0;
    if (avChannelLayout & AV_CH_FRONT_LEFT)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::FrontLeft);
    if (avChannelLayout & AV_CH_FRONT_RIGHT)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::FrontRight);
    if (avChannelLayout & AV_CH_FRONT_CENTER)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::FrontCenter);
    if (avChannelLayout & AV_CH_LOW_FREQUENCY)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::LFE);
    if (avChannelLayout & AV_CH_BACK_LEFT)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::BackLeft);
    if (avChannelLayout & AV_CH_BACK_RIGHT)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::BackRight);
    if (avChannelLayout & AV_CH_FRONT_LEFT_OF_CENTER)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::FrontLeftOfCenter);
    if (avChannelLayout & AV_CH_FRONT_RIGHT_OF_CENTER)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::FrontRightOfCenter);
    if (avChannelLayout & AV_CH_BACK_CENTER)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::BackCenter);
    if (avChannelLayout & AV_CH_LOW_FREQUENCY_2)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::LFE2);
    if (avChannelLayout & AV_CH_SIDE_LEFT)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::SideLeft);
    if (avChannelLayout & AV_CH_SIDE_RIGHT)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::SideRight);
    if (avChannelLayout & AV_CH_TOP_FRONT_LEFT)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::TopFrontLeft);
    if (avChannelLayout & AV_CH_TOP_FRONT_RIGHT)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::TopFrontRight);
    if (avChannelLayout & AV_CH_TOP_FRONT_CENTER)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::TopFrontCenter);
    if (avChannelLayout & AV_CH_TOP_CENTER)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::TopCenter);
    if (avChannelLayout & AV_CH_TOP_BACK_LEFT)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::TopBackLeft);
    if (avChannelLayout & AV_CH_TOP_BACK_RIGHT)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::TopBackRight);
    if (avChannelLayout & AV_CH_TOP_BACK_CENTER)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::TopBackCenter);

#ifdef AV_CH_TOP_SIDE_LEFT
    if (avChannelLayout & AV_CH_TOP_SIDE_LEFT)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::TopSideLeft);
    if (avChannelLayout & AV_CH_TOP_SIDE_RIGHT)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::TopSideRight);
    if (avChannelLayout & AV_CH_BOTTOM_FRONT_CENTER)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::BottomFrontCenter);
    if (avChannelLayout & AV_CH_BOTTOM_FRONT_LEFT)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::BottomFrontLeft);
    if (avChannelLayout & AV_CH_BOTTOM_FRONT_RIGHT)
        channelConfig |= ::AudioFormat::channelConfig(::AudioFormat::BottomFrontRight);
#endif
    return ::AudioFormat::ChannelConfig(channelConfig);
}

::AudioFormat
MediaFormatInfo::audioFormatFromCodecParameters(const AVCodecParameters &codecpar)
{
    ::AudioFormat format;
    format.setSampleFormat(sampleFormat(AVSampleFormat(codecpar.format)));
    format.setSampleRate(codecpar.sample_rate);
#if QT_FFMPEG_HAS_AV_CHANNEL_LAYOUT
    uint64_t channelLayout = 0;
    if (codecpar.ch_layout.order == AV_CHANNEL_ORDER_NATIVE)
        channelLayout = codecpar.ch_layout.u.mask;
    else
        channelLayout = avChannelLayout(::AudioFormat::defaultChannelConfigForChannelCount(codecpar.ch_layout.nb_channels));
#else
    uint64_t channelLayout = codecpar.channel_layout;
    if (!channelLayout)
        channelLayout = avChannelLayout(::AudioFormat::defaultChannelConfigForChannelCount(codecpar.channels));
#endif
    format.setChannelConfig(channelConfigForAVLayout(channelLayout));
    return format;
}
}
QT_END_NAMESPACE
