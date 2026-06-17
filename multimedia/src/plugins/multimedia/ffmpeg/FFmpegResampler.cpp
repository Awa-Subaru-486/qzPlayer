#include "FFmpegResampler_p.h"
#include "playbackengine/FFmpegCodecContext_p.h"
#include "FFmpegMediaFormatInfo_p.h"
import qzLog;

static qz::Log::LogCategory qLcResampler("qz.multimedia.ffmpeg.resampler");
static qz::Log::LogCategory qLcResamplerTrace("qz.multimedia.ffmpeg.resampler.trace");

QT_BEGIN_NAMESPACE

namespace ffmpeg {
Resampler::Resampler(const ::AudioFormat &inputFormat,
                                   const ::AudioFormat &outputFormat, qint64 startTime)
    : m_inputFormat(inputFormat), m_outputFormat(outputFormat), m_startTime(startTime)
{
    Q_ASSERT(inputFormat.isValid());
    Q_ASSERT(outputFormat.isValid());

    const auto inputAvFormat = AVAudioFormat(m_inputFormat);
    const auto outputAvFormat = AVAudioFormat(m_outputFormat);

    m_resampler = createResampleContext(inputAvFormat, outputAvFormat);

    qz::Log::cat_debug(qLcResampler, "Created Resampler with offset {} us. Converting from {} to {}", m_startTime, static_cast<int>(inputAvFormat.sampleFormat), static_cast<int>(outputAvFormat.sampleFormat));
}

Resampler::Resampler(const CodecContext *codecContext,
                                   const ::AudioFormat &outputFormat,
                                   qint64 startTime)
    : m_outputFormat(outputFormat), m_startTime(startTime)
{
    Q_ASSERT(codecContext);

    const AVStream *audioStream = codecContext->stream();

    if (!m_outputFormat.isValid())

        m_outputFormat = MediaFormatInfo::audioFormatFromCodecParameters(*audioStream->codecpar);

    const auto inputAvFormat = AVAudioFormat(codecContext->context());
    const auto outputAvFormat = AVAudioFormat(m_outputFormat);

    m_resampler = createResampleContext(inputAvFormat, outputAvFormat);

    qz::Log::cat_debug(qLcResampler, "Created Resampler. Offset: {} us. From: {} to: {}", m_startTime, static_cast<int>(inputAvFormat.sampleFormat), static_cast<int>(outputAvFormat.sampleFormat));
}

template <typename... Args>
std::unique_ptr<Resampler> Resampler::createImpl(Args... args)
{
    std::unique_ptr<Resampler> resampler{
        new Resampler(std::forward<Args>(args)...),
    };
    if (resampler->isInitialized())
        return resampler;
    return nullptr;
}

std::unique_ptr<Resampler>
Resampler::createFromInputFormat(const ::AudioFormat &inputFormat,
                                        const ::AudioFormat &outputFormat, qint64 startTime)
{
    return createImpl(inputFormat, outputFormat, startTime);
}

std::unique_ptr<Resampler>
Resampler::createFromCodecContext(const CodecContext *codecContext,
                                         const ::AudioFormat &outputFormat, qint64 startTime)
{
    return createImpl(codecContext, outputFormat, startTime);
}

bool Resampler::isInitialized() const
{
    return m_resampler != nullptr;
}

Resampler::~Resampler() = default;

::AudioBuffer Resampler::resample(const char* data, size_t size)
{
    if (!m_inputFormat.isValid())
        return {};

    return resample(reinterpret_cast<const uint8_t **>(&data),
                    m_inputFormat.framesForBytes(static_cast<qint32>(size)));
}

::AudioBuffer Resampler::resample(const AVFrame *frame)
{
    return resample(const_cast<const uint8_t **>(frame->extended_data), frame->nb_samples);
}

::AudioBuffer Resampler::resample(const uint8_t **inputData, int inputSamplesCount)
{
    const int maxOutSamples = adjustMaxOutSamples(inputSamplesCount);

    QByteArray samples(m_outputFormat.bytesForFrames(maxOutSamples), Qt::Uninitialized);
    auto *out = reinterpret_cast<uint8_t *>(samples.data());
    const int outSamples =
            swr_convert(m_resampler.get(), &out, maxOutSamples, inputData, inputSamplesCount);

    samples.resize(m_outputFormat.bytesForFrames(outSamples));

    const qint64 startTime = m_outputFormat.durationForFrames(m_samplesProcessed) + m_startTime;
    m_samplesProcessed += outSamples;

    qz::Log::cat_debug(qLcResamplerTrace, "Created output buffer. Time stamp: {} us. Samples in: {}, Samples out: {}, Max samples: {}", startTime, inputSamplesCount, outSamples, maxOutSamples);
    return ::AudioBuffer(samples, m_outputFormat, startTime);
}

int Resampler::adjustMaxOutSamples(int inputSamplesCount)
{
    int maxOutSamples = swr_get_out_samples(m_resampler.get(), inputSamplesCount);

    const auto remainingCompensationDistance = m_endCompensationSample - m_samplesProcessed;

    if (remainingCompensationDistance > 0 && maxOutSamples > remainingCompensationDistance) {

        setSampleCompensation(0, 0);
        maxOutSamples = swr_get_out_samples(m_resampler.get(), inputSamplesCount);
    }

    return maxOutSamples;
}

void Resampler::setSampleCompensation(qint32 delta, quint32 distance)
{
    const int res = swr_set_compensation(m_resampler.get(), delta, static_cast<int>(distance));
    if (res < 0)
        qz::Log::cat_warn(qLcResampler, "swr_set_compensation fail:{}", res);
    else {
        m_sampleCompensationDelta = delta;
        m_endCompensationSample = m_samplesProcessed + distance;
    }
}

qint32 Resampler::activeSampleCompensationDelta() const
{
    return m_samplesProcessed < m_endCompensationSample ? m_sampleCompensationDelta : 0;
}
}
QT_END_NAMESPACE
