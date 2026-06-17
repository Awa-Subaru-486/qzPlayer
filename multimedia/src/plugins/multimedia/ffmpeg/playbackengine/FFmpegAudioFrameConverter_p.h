#ifndef PLAYBACKENGINE_FFMPEGAUDIOFRAMECONVERTER_P_H
#define PLAYBACKENGINE_FFMPEGAUDIOFRAMECONVERTER_P_H

#include <qzMultimedia/AudioBuffer.h>

extern "C" {
#include <libavutil/frame.h>
}

#include <memory>

QT_BEGIN_NAMESPACE

namespace ffmpeg {
class Resampler;
}

namespace ffmpeg {

// 音频帧转换器抽象接口，将 AVFrame 转换为 AudioBuffer
struct AbstractAudioFrameConverter
{
    virtual ~AbstractAudioFrameConverter();
    // 转换音频帧
    virtual ::AudioBuffer convert(AVFrame *) = 0;
};

struct Frame;

// 创建重采样器
std::unique_ptr<Resampler> createResampler(const Frame &frame,
                                                  const ::AudioFormat &outputFormat);

// 创建普通音频帧转换器
std::unique_ptr<AbstractAudioFrameConverter>
makeTrivialAudioFrameConverter(const Frame &frame, ::AudioFormat outputFormat, float playbackRate);

// 创建变调补偿音频帧转换器
std::unique_ptr<AbstractAudioFrameConverter>
makePitchShiftingAudioFrameConverter(const Frame &frame, ::AudioFormat outputFormat,
                                     float playbackRate);

}

QT_END_NAMESPACE

#endif
