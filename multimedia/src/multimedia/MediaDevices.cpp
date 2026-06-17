// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "MediaDevices.h"
#include "private/PlatformMediaIntegration_p.h"
#include "private/PlatformAudioDevices_p.h"

#include <QtCore/qmetaobject.h>

#include <AudioDevice.h>

QList<AudioDevice> MediaDevices::audioInputs()
{
    return PlatformMediaIntegration::instance()->audioDevices()->audioInputs();
}

QList<AudioDevice> MediaDevices::audioOutputs()
{
    return PlatformMediaIntegration::instance()->audioDevices()->audioOutputs();
}

AudioDevice MediaDevices::defaultAudioInput()
{
    const auto inputs = audioInputs();
    if (inputs.empty())
        return {};
    for (const auto &info : inputs)
        if (info.isDefault())
            return info;
    return inputs.value(0);
}

AudioDevice MediaDevices::defaultAudioOutput()
{
    const auto outputs = audioOutputs();
    if (outputs.empty())
        return {};
    for (const auto &info : outputs)
        if (info.isDefault())
            return info;
    return outputs.value(0);
}

MediaDevices::MediaDevices(QObject *parent) : QObject(parent) { }

MediaDevices::~MediaDevices() = default;

void MediaDevices::connectNotify(const QMetaMethod &signal)
{

    auto ensureConnection = [&](auto devicesMethod, auto platformDevicesSignal, auto targetSignal) {
        if (signal == QMetaMethod::fromSignal(targetSignal))
            if (auto devices = (PlatformMediaIntegration::instance()->*devicesMethod)()) {
                connect(devices, platformDevicesSignal, this, targetSignal, Qt::UniqueConnection);
                return true;
            }

        return false;
    };

    ensureConnection(&PlatformMediaIntegration::audioDevices,
                     &PlatformAudioDevices::audioInputsChanged,
                     &MediaDevices::audioInputsChanged) ||
    ensureConnection(&PlatformMediaIntegration::audioDevices,
                     &PlatformAudioDevices::audioOutputsChanged,
                     &MediaDevices::audioOutputsChanged);

    QObject::connectNotify(signal);
}

#include "moc_MediaDevices.cpp"
