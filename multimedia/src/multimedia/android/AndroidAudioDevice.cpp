// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "AndroidAudioDevice_p.h"

#include <qzMultimedia/private/AudioFormat_p.h>
#include <QtCore/qjniobject.h>

QT_BEGIN_NAMESPACE

AndroidAudioDevice::AndroidAudioDevice(QByteArray device, QString desc,
                                       AudioDevice::Mode mode,
                                       AudioFormat preferredFormat,
                                       bool isBluetoothDevice,
                                       bool isDefaultDevice)
    : AudioDevicePrivate(std::move(device), mode, std::move(desc)),
      m_isBluetoothDevice(isBluetoothDevice)
{
    isDefault = isDefaultDevice;
    this->preferredFormat = preferredFormat;
    minimumChannelCount = 1;
    maximumChannelCount = 32;
    minimumSampleRate = QtMultimediaPrivate::allSupportedSampleRates.front();
    maximumSampleRate = QtMultimediaPrivate::allSupportedSampleRates.back();
    supportedSampleFormats = qAllSupportedSampleFormats();
    channelConfiguration = preferredFormat.channelConfig();
}

bool AndroidAudioDevice::isBluetoothDevice() const
{
    return m_isBluetoothDevice;
}

QT_END_NAMESPACE
