// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "PortAudioDevice_p.h"

PortAudioDevice::PortAudioDevice(QByteArray deviceId, AudioDevice::Mode mode,
                                   QString description, int deviceIndex, int maxChannels,
                                   double defaultSampleRate)
    : AudioDevicePrivate(std::move(deviceId), mode, std::move(description))
    , m_deviceIndex(deviceIndex)
{
    minimumChannelCount = 1;
    maximumChannelCount = maxChannels;

    minimumSampleRate = 8000;
    maximumSampleRate = 192000;

    preferredFormat.setSampleRate(static_cast<int>(defaultSampleRate));
    preferredFormat.setChannelCount(qMin(2, maxChannels));
    preferredFormat.setSampleFormat(AudioFormat::Float);

    supportedSampleFormats = qAllSupportedSampleFormats();

    channelConfiguration = AudioFormat::defaultChannelConfigForChannelCount(maxChannels);
}

