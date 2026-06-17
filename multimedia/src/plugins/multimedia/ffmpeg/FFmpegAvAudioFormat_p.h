#ifndef FFMPEGAVAUDIOFORMAT_P_H
#define FFMPEGAVAUDIOFORMAT_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegDefs_p.h>
#include <qzMultimedia/private/MultimediaGlobal_p.h>

#if QT_FFMPEG_HAS_AV_CHANNEL_LAYOUT
[[maybe_unused]] static inline bool operator==(const AVChannelLayout &lhs,
                                               const AVChannelLayout &rhs)
{
    return lhs.order == rhs.order && lhs.nb_channels == rhs.nb_channels && lhs.u.mask == rhs.u.mask;
}

[[maybe_unused]] static inline bool operator!=(const AVChannelLayout &lhs,
                                               const AVChannelLayout &rhs)
{
    return !(lhs == rhs);
}

#endif

QT_BEGIN_NAMESPACE

class AudioFormat;

namespace ffmpeg {

// FFmpeg 音频格式封装，包含声道布局、采样格式和采样率
struct AVAudioFormat
{
    explicit AVAudioFormat(const AVCodecContext *context);
    explicit AVAudioFormat(const AVCodecParameters *codecPar);
    explicit AVAudioFormat(const ::AudioFormat &audioFormat);

#if QT_FFMPEG_HAS_AV_CHANNEL_LAYOUT
    AVChannelLayout channelLayout;
#else
    uint64_t channelLayoutMask;
#endif
    AVSampleFormat sampleFormat;
    int sampleRate;
};

bool operator==(const AVAudioFormat &lhs, const AVAudioFormat &rhs);

inline bool operator!=(const AVAudioFormat &lhs, const AVAudioFormat &rhs)
{
    return !(lhs == rhs);
}

QDebug operator<<(QDebug, const AVAudioFormat &);

}

QT_END_NAMESPACE

#endif
