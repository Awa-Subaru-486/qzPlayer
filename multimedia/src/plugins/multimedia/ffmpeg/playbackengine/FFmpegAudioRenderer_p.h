#ifndef PLAYBACKENGINE_FFMPEGAUDIORENDERER_P_H
#define PLAYBACKENGINE_FFMPEGAUDIORENDERER_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegRenderer_p.h>

#include "AudioBuffer.h"

QT_BEGIN_NAMESPACE

class AudioOutput;
class AudioBufferOutput;
class AudioSink;

namespace ffmpeg {
class Resampler;
}

namespace ffmpeg {

struct AbstractAudioFrameConverter;

// 音频渲染器实现类，负责将解码后的音频数据输出到音频设备。
class AudioRenderer : public Renderer
{
    Q_OBJECT
public:
    AudioRenderer(const PlaybackEngineObjectID &id, const TimeController &tc, ::AudioOutput *output,
                  ::AudioBufferOutput *bufferOutput, bool pitchCompensation);

    void setOutput(::AudioOutput *output);

    void setOutput(::AudioBufferOutput *bufferOutput);

    void setPitchCompensation(bool enabled);

    ~AudioRenderer() override;

protected:
    using Microseconds = std::chrono::microseconds;
    // 同步戳，记录音频设备状态用于音视频同步
    struct SynchronizationStamp
    {
        ::Audio::State audioSinkState = ::Audio::IdleState;
        qsizetype audioSinkBytesFree = 0;
        qsizetype bufferBytesWritten = 0;
        TimePoint timePoint = TimePoint::max();
    };

    struct BufferLoadingInfo
    {
        enum Type { Low, Moderate, High };
        Type type = Moderate;
        TimePoint timePoint = TimePoint::max();
        Microseconds delay = Microseconds(0);
    };

    struct AudioTimings
    {
        Microseconds actualBufferDuration = Microseconds(0);
        Microseconds maxSoundDelay = Microseconds(0);
        Microseconds minSoundDelay = Microseconds(0);
    };

    // 带偏移的缓冲数据，用于部分数据输出
    struct BufferedDataWithOffset
    {
        ::AudioBuffer buffer;
        qsizetype offset = 0;

        bool isValid() const { return buffer.isValid(); }
        qsizetype size() const { return buffer.byteCount() - offset; }
        const char *data() const { return buffer.constData<char>() + offset; }
    };

    RenderingResult renderInternal(Frame frame) override;

    RenderingResult pushFrameToOutput(const Frame &frame);

    void pushFrameToBufferOutput(const Frame &frame);

    void onPlaybackRateChanged() override;

    TimePoint nextTimePoint() const override;

    void onPauseChanged() override;

    void freeOutput();

    void updateOutputs(const Frame &frame);

    void initAudioFrameConverter(const Frame &frame);

    void onDeviceChanged();

    void updateVolume();

    void updateSynchronization(const SynchronizationStamp &stamp, const Frame &frame);

    Microseconds bufferLoadingTime(const SynchronizationStamp &syncStamp) const;

    void onAudioSinkStateChanged(::Audio::State state);

    Microseconds durationForBytes(qsizetype bytes) const;

private:
    QPointer<::AudioOutput> m_output;
    QPointer<::AudioBufferOutput> m_bufferOutput;
    std::unique_ptr<::AudioSink> m_sink;
    AudioTimings m_timings;
    BufferLoadingInfo m_bufferLoadingInfo;
    std::unique_ptr<Resampler> m_bufferOutputResampler;
    ::AudioFormat m_sinkFormat;

    BufferedDataWithOffset m_bufferedData;
    QPointer<QIODevice> m_ioDevice;

    bool m_lastFramePushDone = true;

    bool m_deviceChanged = false;
    bool m_bufferOutputChanged = false;
    bool m_drained = false;
    bool m_firstFrameToSink = true;

    bool m_pitchCompensation = false;
    std::unique_ptr<AbstractAudioFrameConverter> m_audioFrameConverter;
};

}

QT_END_NAMESPACE

#endif
