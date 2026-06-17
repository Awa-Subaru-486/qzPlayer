#ifndef WINDOWSINTEGRATION_P_H
#define WINDOWSINTEGRATION_P_H

#include <qzMultimedia/private/ComInitializer_p.h>
#include <qzMultimedia/private/PlatformMediaIntegration_p.h>

namespace windows {

class AudioDevices;
class FormatInfo;

// Windows 平台媒体集成类，创建 Media Foundation 平台实现
class MediaIntegration : public PlatformMediaIntegration
{
    Q_OBJECT
public:
    MediaIntegration();
    ~MediaIntegration();

    // 创建播放器
    std::expected<PlatformMediaPlayer *, QString> createPlayer(MediaPlayer *parent) override;

    // 创建视频输出
    std::expected<PlatformVideoSink *, QString> createVideoSink(VideoSink *sink) override;

protected:
    // 创建格式信息
    PlatformMediaFormatInfo *createFormatInfo() override;

private:
    ComInitializer m_comInitializer;
};

}

#endif
