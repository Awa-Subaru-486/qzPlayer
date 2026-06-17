#ifndef FFMPEGMEDIAFORMATINFO_P_H
#define FFMPEGMEDIAFORMATINFO_P_H

#include <qzMultimedia/private/PlatformMediaFormatInfo_p.h>
#include <qhash.h>
#include <qlist.h>
#include <AudioFormat.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpeg_p.h>

QT_BEGIN_NAMESPACE

namespace ffmpeg {
// 媒体格式信息，提供 FFmpeg 编解码 ID 与 Qt 格式的映射
class MediaFormatInfo : public PlatformMediaFormatInfo
{
public:
    MediaFormatInfo();
    ~MediaFormatInfo() override;

    static ::MediaFormat::VideoCodec videoCodecForAVCodecId(AVCodecID id);
    static ::MediaFormat::AudioCodec audioCodecForAVCodecId(AVCodecID id);
    static ::MediaFormat::FileFormat fileFormatForAVInputFormat(const AVInputFormat &format);

    static const AVOutputFormat *outputFormatForFileFormat(::MediaFormat::FileFormat format);

    static AVCodecID codecIdForVideoCodec(::MediaFormat::VideoCodec codec);
    static AVCodecID codecIdForAudioCodec(::MediaFormat::AudioCodec codec);

    static ::AudioFormat::SampleFormat sampleFormat(AVSampleFormat format);
    static AVSampleFormat avSampleFormat(::AudioFormat::SampleFormat format);

    static int64_t avChannelLayout(::AudioFormat::ChannelConfig channelConfig);
    static ::AudioFormat::ChannelConfig channelConfigForAVLayout(int64_t avChannelLayout);

    static ::AudioFormat audioFormatFromCodecParameters(const AVCodecParameters &codecPar);
};
}
QT_END_NAMESPACE

#endif
