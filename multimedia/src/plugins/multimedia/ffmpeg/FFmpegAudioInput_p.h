#ifndef FFMPEGAUDIOINPUT_P_H
#define FFMPEGAUDIOINPUT_P_H

#include <qzMultimedia/AudioInput.h>
#include <qzMultimedia/private/PlatformAudioInput_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegThread_p.h>

QT_BEGIN_NAMESPACE

namespace ffmpeg {
class AudioSourceIO;
}

constexpr int DefaultAudioInputBufferSize = 4096;

namespace ffmpeg {

// 音频输入实现，捕获音频输入设备数据
class AudioInput : public QObject, public PlatformAudioInput
{
    Q_OBJECT
public:
    explicit AudioInput(::AudioInput *qq);
    ~AudioInput() override;

    void setAudioDevice(const ::AudioDevice &) override;
    void setMuted(bool ) override;
    void setVolume(float ) override;

    void setBufferSize(int bufferSize);

    int bufferSize() const;

private:
    AudioSourceIO *m_audioIO = nullptr;
    std::unique_ptr<QThread> m_inputThread;
};

}

QT_END_NAMESPACE

#endif
