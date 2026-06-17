// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_ANDROID_ANDROIDAUDIODEVICE_P_H
#define QT_ANDROID_ANDROIDAUDIODEVICE_P_H

#include <qzMultimedia/private/AudioDevice_p.h>

QT_BEGIN_NAMESPACE

class AndroidAudioDevice : public AudioDevicePrivate
{
public:
    AndroidAudioDevice(QByteArray device, QString desc, AudioDevice::Mode mode,
                       AudioFormat preferredFormat, bool isBluetoothDevice,
                       bool isDefaultDevice = false);

    bool isBluetoothDevice() const;

private:
    bool m_isBluetoothDevice;
};

QT_END_NAMESPACE

#endif
