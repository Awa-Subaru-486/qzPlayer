#include <qzMultimedia/private/PlatformMediaPlugin_p.h>

#include <qzFFmpegMediaPluginImpl/private/FFmpegMediaIntegration_p.h>

QT_BEGIN_NAMESPACE

class QFFmpegMediaPlugin : public PlatformMediaPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformMediaPlugin_iid FILE "ffmpeg.json")

public:
    PlatformMediaIntegration *create(const QString &name) override
    {
        if (name == u"ffmpeg")
            return new ffmpeg::MediaIntegration;
        return nullptr;
    }
};

QT_END_NAMESPACE

#include "FFmpegPlugin.moc"
