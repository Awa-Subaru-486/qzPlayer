// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "PortAudioDevices_p.h"
#include "PortAudioDevice_p.h"
#include "PortAudioSink_p.h"

#include <portaudio.h>

PortAudioDevices::PortAudioDevices()
{
    Pa_Initialize();
}

PortAudioDevices::~PortAudioDevices()
{
    Pa_Terminate();
}

QList<AudioDevice> PortAudioDevices::findAudioInputs() const
{
    QList<AudioDevice> devices;

    int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0)
        return devices;

    PaDeviceIndex defaultInput = Pa_GetDefaultInputDevice();

    for (int i = 0; i < numDevices; ++i) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        if (!info || info->maxInputChannels <= 0)
            continue;

        auto device = std::make_unique<PortAudioDevice>(
            QByteArray::number(i),
            AudioDevice::Input,
            QString::fromUtf8(info->name),
            i,
            info->maxInputChannels,
            info->defaultSampleRate
        );

        device->isDefault = (i == defaultInput);
        devices.append(AudioDevicePrivate::createQAudioDevice(std::move(device)));
    }

    return devices;
}

QList<AudioDevice> PortAudioDevices::findAudioOutputs() const
{
    QList<AudioDevice> devices;

    int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0)
        return devices;

    PaDeviceIndex defaultOutput = Pa_GetDefaultOutputDevice();

    for (int i = 0; i < numDevices; ++i) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        if (!info || info->maxOutputChannels <= 0)
            continue;

        auto device = std::make_unique<PortAudioDevice>(
            QByteArray::number(i),
            AudioDevice::Output,
            QString::fromUtf8(info->name),
            i,
            info->maxOutputChannels,
            info->defaultSampleRate
        );

        device->isDefault = (i == defaultOutput);
        devices.append(AudioDevicePrivate::createQAudioDevice(std::move(device)));
    }

    return devices;
}

PlatformAudioSource *PortAudioDevices::createAudioSource(const AudioDevice &device,
                                                           const AudioFormat &format,
                                                           QObject *parent)
{
    Q_UNUSED(device);
    Q_UNUSED(format);
    Q_UNUSED(parent);
    return nullptr;
}

PlatformAudioSink *PortAudioDevices::createAudioSink(const AudioDevice &device,
                                                       const AudioFormat &format,
                                                       QObject *parent)
{
    return new PortAudioSink(device, format, parent);
}

