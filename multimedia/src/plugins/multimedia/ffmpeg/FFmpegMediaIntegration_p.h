#ifndef FFMPEGMEDIAINTEGRATION_P_H
#define FFMPEGMEDIAINTEGRATION_P_H

#include <private/PlatformMediaIntegration_p.h>

QT_BEGIN_NAMESPACE

namespace ffmpeg {
// FFmpeg 媒体集成类，创建平台实现对象
class MediaIntegration : public PlatformMediaIntegration
{
public:
    MediaIntegration();

    std::expected<std::unique_ptr<PlatformAudioResampler>, QString>
    createAudioResampler(const ::AudioFormat &inputFormat,
                         const ::AudioFormat &outputFormat) override;
    std::expected<PlatformMediaPlayer *, QString> createPlayer(::MediaPlayer *player) override;

    std::expected<PlatformVideoSink *, QString> createVideoSink(::VideoSink *sink) override;

    PlatformPreviewFrameProvider *createPreviewFrameProvider() override;

    std::expected<PlatformAudioInput *, QString> createAudioInput(::AudioInput *input) override;

    ::VideoFrame convertVideoFrame(::VideoFrame &srcFrame,
                                  const VideoFrameFormat &destFormat) override;

protected:
    PlatformMediaFormatInfo *createFormatInfo() override;
};
}
QT_END_NAMESPACE

#endif
