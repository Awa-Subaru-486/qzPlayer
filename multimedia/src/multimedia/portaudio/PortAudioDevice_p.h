// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_PORTAUDIO_PORTAUDIODEVICE_P_H
#define QT_PORTAUDIO_PORTAUDIODEVICE_P_H

#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>

#include <qzMultimedia/AudioDevice.h>
#include <qzMultimedia/private/AudioDevice_p.h>

class PortAudioDevice : public AudioDevicePrivate
{
public:
    PortAudioDevice(QByteArray deviceId, AudioDevice::Mode mode, QString description,
                     int deviceIndex, int maxChannels, double defaultSampleRate);

    ~PortAudioDevice() override = default;

    int deviceIndex() const { return m_deviceIndex; }

private:
    int m_deviceIndex;
};

#endif
