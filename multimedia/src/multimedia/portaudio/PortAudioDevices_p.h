// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_PORTAUDIO_PORTAUDIODEVICES_P_H
#define QT_PORTAUDIO_PORTAUDIODEVICES_P_H

#include <qzMultimedia/private/PlatformAudioDevices_p.h>

class PortAudioDevices : public PlatformAudioDevices
{
public:
    PortAudioDevices();
    ~PortAudioDevices() override;

protected:
    QList<AudioDevice> findAudioInputs() const override;
    QList<AudioDevice> findAudioOutputs() const override;

    PlatformAudioSource *createAudioSource(const AudioDevice &, const AudioFormat &,
                                            QObject *parent) override;
    PlatformAudioSink *createAudioSink(const AudioDevice &, const AudioFormat &,
                                        QObject *parent) override;
};

#endif
