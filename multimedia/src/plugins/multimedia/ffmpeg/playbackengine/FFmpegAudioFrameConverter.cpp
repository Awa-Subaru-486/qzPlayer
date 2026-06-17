#include "FFmpegAudioFrameConverter_p.h"

#include <qzMultimedia/private/AudioBufferSupport_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegFrame_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegResampler_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegMediaFormatInfo_p.h>

#if defined(Q_CC_MSVC) && defined(QT_MM_OPTIMIZE_DEBUG)
#  pragma optimize("s", on)
#endif

#include <signalsmith-stretch.h>

QT_BEGIN_NAMESPACE

namespace ffmpeg {

namespace {

qreal sampleRateFactor()
{

    static const qreal result = []() {
        const auto sampleRateFactorStr =
                qEnvironmentVariable("QT_MEDIA_PLAYER_AUDIO_SAMPLE_RATE_FACTOR");
        bool ok = false;
        const auto result = sampleRateFactorStr.toDouble(&ok);
        return ok ? result : 1.;
    }();

    return result;
}

struct TrivialAudioFrameConverter : AbstractAudioFrameConverter
{
    explicit TrivialAudioFrameConverter(const Frame &frame, ::AudioFormat outputFormat,
                                        float playbackRate)
    {
        int sampleRate = qRound(outputFormat.sampleRate() / playbackRate * sampleRateFactor());
        outputFormat.setSampleRate(sampleRate);
        m_converter = createResampler(frame, outputFormat);
    }

    ::AudioBuffer convert(AVFrame *frame) override { return m_converter->resample(frame); }

private:
    std::unique_ptr<Resampler> m_converter;
};

struct PitchShiftingAudioFrameConverter : AbstractAudioFrameConverter
{
    explicit PitchShiftingAudioFrameConverter(const Frame &frame, ::AudioFormat outputFormat,
                                              float playbackRate)
        : m_playbackRate{ playbackRate }
    {
        const ::AudioFormat mediaFormat = MediaFormatInfo::audioFormatFromCodecParameters(
                *frame.codecContext()->stream()->codecpar);

        const ::AudioFormat floatFormat = [&] {
            ::AudioFormat ret = mediaFormat;
            ret.setSampleFormat(::AudioFormat::SampleFormat::Float);
            return ret;
        }();

        m_toPCMDecoder = createResampler(frame, floatFormat);
        m_stretcher.reset();
        m_pendingFractionalFrames = 0.f;
        m_stretcher.presetDefault(mediaFormat.channelCount(), outputFormat.sampleRate());

        const ::AudioFormat pitchCompensatedFormat = [&] {
            ::AudioFormat ret = floatFormat;
            ret.setSampleRate(outputFormat.sampleRate());
            return ret;
        }();
        m_toOutputFormatConverter = Resampler::createFromInputFormat(pitchCompensatedFormat, outputFormat,
                                                             frame.startTime().get());
    }

    ::AudioBuffer convert(AVFrame *frame) override
    {
        using namespace QtPrivate;

        ::AudioBuffer wordConverted = m_toPCMDecoder->resample(frame);

        int mediaFrameCount = wordConverted.frameCount();
        float expectedNumberOfFrames = mediaFrameCount / m_playbackRate + m_pendingFractionalFrames;
        int numberOfFullExpectedFrames = qFloor(expectedNumberOfFrames);
        m_pendingFractionalFrames = expectedNumberOfFrames - numberOfFullExpectedFrames;

        auto timeStretcherOutput = ::AudioBuffer{
            numberOfFullExpectedFrames,
            wordConverted.format(),
        };

        m_stretcher.process(
                AudioBufferDeinterleaveAdaptor<const float>{
                        wordConverted,
                },
                mediaFrameCount,
                AudioBufferDeinterleaveAdaptor<float>{
                        timeStretcherOutput,
                },
                numberOfFullExpectedFrames);

        ::AudioBuffer outputBuffer = m_toOutputFormatConverter->resample(
                timeStretcherOutput.constData<const char>(), timeStretcherOutput.byteCount());

        return outputBuffer;
    }

private:
    std::unique_ptr<Resampler> m_toPCMDecoder;
    signalsmith::stretch::SignalsmithStretch<float> m_stretcher;
    std::unique_ptr<Resampler> m_toOutputFormatConverter;
    float m_playbackRate;
    float m_pendingFractionalFrames = 0.f;
};

}

AbstractAudioFrameConverter::~AbstractAudioFrameConverter() = default;

std::unique_ptr<Resampler> createResampler(const Frame &frame,
                                                  const ::AudioFormat &outputFormat)
{
    return Resampler::createFromCodecContext(frame.codecContext(), outputFormat, frame.startTime().get());
}

std::unique_ptr<AbstractAudioFrameConverter>
makeTrivialAudioFrameConverter(const Frame &frame, ::AudioFormat outputFormat, float playbackRate)
{
    return std::make_unique<TrivialAudioFrameConverter>(frame, outputFormat, playbackRate);
}

std::unique_ptr<AbstractAudioFrameConverter>
makePitchShiftingAudioFrameConverter(const Frame &frame, ::AudioFormat outputFormat,
                                     float playbackRate)
{
    return std::make_unique<PitchShiftingAudioFrameConverter>(frame, outputFormat, playbackRate);
}

}

QT_END_NAMESPACE
