// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_ANDROID_ANDROIDAUDIODEVICES_P_H
#define QT_ANDROID_ANDROIDAUDIODEVICES_P_H

#include <qzMultimedia/private/PlatformAudioDevices_p.h>

QT_BEGIN_NAMESPACE

class AndroidAudioDevices : public PlatformAudioDevices
{
public:
    AndroidAudioDevices();
    ~AndroidAudioDevices();

    PlatformAudioSource *createAudioSource(const AudioDevice &, const AudioFormat &,
                                            QObject *parent) override;
    PlatformAudioSink *createAudioSink(const AudioDevice &, const AudioFormat &,
                                        QObject *parent) override;

    using PlatformAudioDevices::onAudioInputsChanged;
    using PlatformAudioDevices::onAudioOutputsChanged;

    QLatin1String backendName() const override { return QLatin1String{ "AAudio" }; }

    [[nodiscard]] static bool registerNativeMethods();

protected:
    QList<AudioDevice> findAudioInputs() const override;
    QList<AudioDevice> findAudioOutputs() const override;
};

QT_END_NAMESPACE

#endif
