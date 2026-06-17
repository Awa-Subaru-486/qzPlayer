// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "PlatformMediaIntegration_p.h"

#include <QtCore/qapplicationstatic.h>
#include <QtCore/qatomic.h>
#include <QtCore/qcoreapplication.h>
import qzLog;
#include <QtCore/qmutex.h>
#include <QtCore/private/qcoreapplication_p.h>
#include <QtCore/private/qfactoryloader_p.h>

#include <qzMultimedia/MediaDevices.h>
#include <qzMultimedia/VideoFrame.h>
#include <qzMultimedia/private/PlatformAudioDevices_p.h>
#include <qzMultimedia/private/PlatformAudioInput_p.h>
#include <qzMultimedia/private/PlatformAudioOutput_p.h>
#include <qzMultimedia/private/PlatformAudioResampler_p.h>
#include <qzMultimedia/private/PlatformMediaFormatInfo_p.h>
#include <qzMultimedia/private/PlatformMediaPlugin_p.h>
#include <qzMultimedia/private/MultimediaGlobal_p.h>

#ifdef Q_OS_WINDOWS
#include <qzMultimedia/private/WindowsPlatformSpecificInterface_p.h>
#endif

namespace {

class FallbackIntegration : public PlatformMediaIntegration
{
public:
    FallbackIntegration() : PlatformMediaIntegration(QLatin1String("fallback"))
    {
        qWarning("No QtMultimedia backends found. Only MediaDevices, AudioDevice, SoundEffect, AudioSink, and AudioSource are available.");
    }
};

static qz::Log::LogCategory qLcMediaPlugin("qz.multimedia.plugin");

Q_GLOBAL_STATIC_WITH_ARGS(QFactoryLoader, loader,
                          (QPlatformMediaPlugin_iid,
                           QLatin1String("/multimedia")))

constexpr auto FFmpegBackend = "ffmpeg";

struct InstanceHolder
{
    InstanceHolder()
    {
        init();
    }

    void init()
    {
        if (!QCoreApplication::instance())
            qz::Log::cat_critical(qLcMediaPlugin, "Qt Multimedia requires a QCoreApplication instance");

        const QStringList backends = PlatformMediaIntegration::availableBackends();
        const QString backend = QString::fromUtf8(FFmpegBackend);
        instance.reset(qLoadPlugin<PlatformMediaIntegration, PlatformMediaPlugin>(loader(), backend));

        if (!instance) {
            instance = std::make_unique<FallbackIntegration>();
        }
    }

    ~InstanceHolder()
    {
        instance.reset();
        qz::Log::cat_debug(qLcMediaPlugin, "Released media backend");
    }

    std::unique_ptr<PlatformMediaIntegration> instance;
};

Q_APPLICATION_STATIC(InstanceHolder, s_instanceHolder);

}

PlatformMediaIntegration *PlatformMediaIntegration::instance()
{
    return s_instanceHolder->instance.get();
}

void PlatformMediaIntegration::resetInstance()
{
    s_instanceHolder->init();
}

std::expected<std::unique_ptr<PlatformAudioResampler>, QString>
PlatformMediaIntegration::createAudioResampler(const AudioFormat &, const AudioFormat &)
{
    return std::unexpected(notAvailable);
}

std::expected<PlatformAudioInput *, QString> PlatformMediaIntegration::createAudioInput(AudioInput *q)
{
    return new PlatformAudioInput(q);
}

std::expected<PlatformAudioOutput *, QString> PlatformMediaIntegration::createAudioOutput(AudioOutput *q)
{
    return new PlatformAudioOutput(q);
}

const PlatformMediaFormatInfo *PlatformMediaIntegration::formatInfo()
{
    std::call_once(m_formatInfoOnceFlg, [this]() {
        m_formatInfo.reset(createFormatInfo());
        Q_ASSERT(m_formatInfo);
    });
    return m_formatInfo.get();
}

PlatformMediaFormatInfo *PlatformMediaIntegration::createFormatInfo()
{
    return new PlatformMediaFormatInfo;
}

std::unique_ptr<PlatformAudioDevices> PlatformMediaIntegration::createAudioDevices()
{
    return PlatformAudioDevices::create();
}

PlatformAudioDevices *PlatformMediaIntegration::audioDevices()
{
    std::call_once(m_audioDevicesOnceFlag, [this] {
        m_audioDevices = createAudioDevices();
    });
    return m_audioDevices.get();
}

QStringList PlatformMediaIntegration::availableBackends()
{
    QStringList list;

    if (QFactoryLoader *fl = loader()) {
        const auto keyMap = fl->keyMap();
        for (auto it = keyMap.constBegin(); it != keyMap.constEnd(); ++it)
            if (!list.contains(it.value()))
                list << it.value();
    }

    qz::Log::cat_debug(qLcMediaPlugin, "Available backends {}", list.join(QLatin1String(", ")));
    return list;
}

QLatin1String PlatformMediaIntegration::name()
{
    return m_backendName;
}

VideoFrame PlatformMediaIntegration::convertVideoFrame(VideoFrame &,
                                                         const VideoFrameFormat &)
{
    return {};
}

QLatin1String PlatformMediaIntegration::audioBackendName()
{
    return PlatformMediaIntegration::instance()->audioDevices()->backendName();
}

PlatformMediaIntegration::PlatformMediaIntegration(QLatin1String name) : m_backendName(name) { }

PlatformMediaIntegration::~PlatformMediaIntegration() = default;

AbstractPlatformSpecificInterface *PlatformMediaIntegration::platformSpecificInterface()
{
#ifdef Q_OS_WINDOWS
    if (!m_platformSpecificInterface)
        m_platformSpecificInterface = std::make_unique<WindowsPlatformSpecificInterface>();
    return m_platformSpecificInterface.get();
#else
    return nullptr;
#endif
}

#include "moc_PlatformMediaIntegration_p.cpp"
