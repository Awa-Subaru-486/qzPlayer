// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include <AudioInput.h>
#include <AudioDevice.h>
#include <MediaDevices.h>
#include <private/PlatformAudioInput_p.h>
#include <private/PlatformMediaIntegration_p.h>

#include <utility>

AudioInput::AudioInput(QObject *parent) : AudioInput(MediaDevices::defaultAudioInput(), parent)
{
}

AudioInput::AudioInput(const AudioDevice &device, QObject *parent)
    : QObject(parent)
{
    auto maybeAudioInput = PlatformMediaIntegration::instance()->createAudioInput(this);
    if (maybeAudioInput) {
        d = maybeAudioInput.value();
        d->device = device.mode() == AudioDevice::Input ? device : MediaDevices::defaultAudioInput();
        d->setAudioDevice(d->device);
    } else {
        d = new PlatformAudioInput(nullptr);
        qWarning() << "Failed to initialize AudioInput" << maybeAudioInput.error();
    }
}

AudioInput::~AudioInput()
{
    setDisconnectFunction({});
    delete d;
}

float AudioInput::volume() const
{
    return d->volume;
}

void AudioInput::setVolume(float volume)
{
    volume = qBound(0., volume, 1.);
    if (d->volume == volume)
        return;
    d->volume = volume;
    d->setVolume(volume);
    emit volumeChanged(volume);
}

bool AudioInput::isMuted() const
{
    return d->muted;
}

void AudioInput::setMuted(bool muted)
{
    if (d->muted == muted)
        return;
    d->muted = muted;
    d->setMuted(muted);
    emit mutedChanged(muted);
}

AudioDevice AudioInput::device() const
{
    return d->device;
}

void AudioInput::setDevice(const AudioDevice &device)
{
    auto dev = device;
    if (dev.isNull())
        dev = MediaDevices::defaultAudioInput();
    if (dev.mode() != AudioDevice::Input)
        return;
    if (d->device == dev)
        return;
    d->device = dev;
    d->setAudioDevice(dev);
    emit deviceChanged();
}

void AudioInput::setDisconnectFunction(std::function<void()> disconnectFunction)
{
    if (d->disconnectFunction) {
        auto df = d->disconnectFunction;
        d->disconnectFunction = {};
        df();
    }
    d->disconnectFunction = std::move(disconnectFunction);
}

#include "moc_AudioInput.cpp"
