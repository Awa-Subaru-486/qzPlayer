#include "WindowsIntegration_p.h"
#include <private/WindowsAudioDevices_p.h>
#include "WindowsFormatInfo_p.h"
#include <private/PlatformMediaPlugin_p.h>

namespace windows {

class MediaPlugin : public PlatformMediaPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformMediaPlugin_iid FILE "windows.json")

public:
    MediaPlugin()
      : PlatformMediaPlugin()
    {}

    PlatformMediaIntegration* create(const QString &name) override
    {
        if (name == u"windows")
            return new MediaIntegration;
        return nullptr;
    }
};

MediaIntegration::MediaIntegration()
    : PlatformMediaIntegration(QLatin1String("windows"))
{
    MFStartup(MF_VERSION);
}

MediaIntegration::~MediaIntegration()
{
    MFShutdown();
}

PlatformMediaFormatInfo *MediaIntegration::createFormatInfo()
{
    return new FormatInfo();
}

std::expected<PlatformMediaPlayer *, QString> MediaIntegration::createPlayer(MediaPlayer *)
{
    return nullptr;
}

std::expected<PlatformVideoSink *, QString> MediaIntegration::createVideoSink(VideoSink *)
{
    return nullptr;
}

}

#include "WindowsIntegration.moc"
