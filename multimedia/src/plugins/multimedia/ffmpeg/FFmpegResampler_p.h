#ifndef FFMPEGRESAMPLER_P_H
#define FFMPEGRESAMPLER_P_H

#include "AudioBuffer.h"
#include <qzFFmpegMediaPluginImpl/private/FFmpeg_p.h>
#include <qzMultimedia/private/PlatformAudioResampler_p.h>

QT_BEGIN_NAMESPACE

namespace ffmpeg
{

class CodecContext;

// 音频重采样器，基于 FFmpeg SwrContext 实现格式转换
class Resampler : public PlatformAudioResampler
{
    Resampler(const ::AudioFormat &inputFormat, const ::AudioFormat &outputFormat,
                     qint64 startTime);
    Resampler(const CodecContext *codecContext, const ::AudioFormat &outputFormat,
                     qint64 startTime);

    template <typename... Args>
    static std::unique_ptr<Resampler> createImpl(Args...);

public:
    static std::unique_ptr<Resampler> createFromInputFormat(const ::AudioFormat &input,
                                                                   const ::AudioFormat &output,
                                                                   qint64 startTime = 0);
    static std::unique_ptr<Resampler> createFromCodecContext(const CodecContext *,
                                                                    const ::AudioFormat &output,
                                                                    qint64 startTime = 0);

    ~Resampler() override;

    bool isInitialized() const;

    ::AudioBuffer resample(const char *data, size_t size) override;

    ::AudioBuffer resample(const AVFrame *frame);

    qint64 samplesProcessed() const { return m_samplesProcessed; }
    void setSampleCompensation(qint32 delta, quint32 distance);
    qint32 activeSampleCompensationDelta() const;

private:
    int adjustMaxOutSamples(int inputSamplesCount);

    ::AudioBuffer resample(const uint8_t **inputData, int inputSamplesCount);

private:
    ::AudioFormat m_inputFormat;
    ::AudioFormat m_outputFormat;
    qint64 m_startTime = 0;
    SwrContextUPtr m_resampler;
    qint64 m_samplesProcessed = 0;
    qint64 m_endCompensationSample = std::numeric_limits<qint64>::min();
    qint32 m_sampleCompensationDelta = 0;
};

}

QT_END_NAMESPACE

#endif
