// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_AUDIODEVICE_P_H
#define QT_AUDIO_AUDIODEVICE_P_H

#include <qzMultimedia/AudioDevice.h>
#include <QtCore/private/qglobal_p.h>

class QZ_MULTIMEDIA_EXPORT AudioDevicePrivate : public QSharedData
{
public:
    AudioDevicePrivate(QByteArray id, AudioDevice::Mode m, QString description)
        : id(std::move(id)), mode(m), description(std::move(description))
    {}
    virtual ~AudioDevicePrivate();
    const QByteArray id;
    const AudioDevice::Mode mode = AudioDevice::Output;
    const QString description;
    bool isDefault = false;

    AudioFormat preferredFormat;
    int minimumSampleRate = 0;
    int maximumSampleRate = 0;
    int minimumChannelCount = 0;
    int maximumChannelCount = 0;
    QList<AudioFormat::SampleFormat> supportedSampleFormats;
    AudioFormat::ChannelConfig channelConfiguration = AudioFormat::ChannelConfigUnknown;

    static AudioDevice createQAudioDevice(std::unique_ptr<AudioDevicePrivate> devicePrivate);

    static const AudioDevicePrivate *handle(const AudioDevice &device) { return device.d.get(); }

    template <typename Derived>
    static const Derived *handle(const AudioDevice &device)
    {

        return dynamic_cast<const Derived *>(handle(device));
    }
};

inline const QList<AudioFormat::SampleFormat> &qAllSupportedSampleFormats()
{
    static const auto singleton = QList<AudioFormat::SampleFormat>{
        AudioFormat::UInt8,
        AudioFormat::Int16,
        AudioFormat::Int32,
        AudioFormat::Float,
    };
    return singleton;
}

struct AudioDevicePrivateAllMembersEqual
{
    bool operator()(const AudioDevicePrivate &lhs, const AudioDevicePrivate &rhs)
    {
        auto asTuple = [](const AudioDevicePrivate &x) {
            return std::tie(x.id, x.mode, x.isDefault, x.preferredFormat, x.description,
                            x.minimumSampleRate, x.maximumSampleRate, x.minimumChannelCount,
                            x.maximumChannelCount, x.supportedSampleFormats,
                            x.channelConfiguration);
        };

        return asTuple(lhs) == asTuple(rhs);
    }
};

#endif
