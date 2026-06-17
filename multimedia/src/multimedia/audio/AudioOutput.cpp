// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include <AudioOutput.h>
#include <AudioDevice.h>
#include <MediaDevices.h>
#include <private/PlatformAudioOutput_p.h>
#include <private/PlatformMediaIntegration_p.h>

AudioOutput::AudioOutput(QObject *parent)
    : AudioOutput(MediaDevices::defaultAudioOutput(), parent)
{
}

AudioOutput::AudioOutput(const AudioDevice &device, QObject *parent)
    : QObject(parent)
{
    auto maybeAudioOutput = PlatformMediaIntegration::instance()->createAudioOutput(this);
    if (maybeAudioOutput) {
        d = maybeAudioOutput.value();
        d->device = device.mode() == AudioDevice::Output ? device : MediaDevices::defaultAudioOutput();
        d->setAudioDevice(d->device);
    } else {
        d = new PlatformAudioOutput(nullptr);
        qWarning() << "Failed to initialize AudioOutput" << maybeAudioOutput.error();
    }
}

AudioOutput::~AudioOutput()
{
    setDisconnectFunction({});
    delete d;
}

float AudioOutput::volume() const
{
    return d->volume;
}

void AudioOutput::setVolume(float volume)
{
    volume = qBound(0., volume, 1.);
    if (d->volume == volume)
        return;
    d->volume = volume;
    d->setVolume(volume);
    emit volumeChanged(volume);
}

bool AudioOutput::isMuted() const
{
    return d->muted;
}

void AudioOutput::setMuted(bool muted)
{
    if (d->muted == muted)
        return;
    d->muted = muted;
    d->setMuted(muted);
    emit mutedChanged(muted);
}

AudioDevice AudioOutput::device() const
{
    return d->device;
}

void AudioOutput::setDevice(const AudioDevice &device)
{
    auto dev = device;
    if (dev.isNull())
        dev = MediaDevices::defaultAudioOutput();
    if (dev.mode() != AudioDevice::Output)
        return;
    if (d->device == dev)
        return;
    d->device = dev;
    d->setAudioDevice(dev);
    emit deviceChanged();
}

void AudioOutput::setDisconnectFunction(std::function<void()> disconnectFunction)
{
    if (d->disconnectFunction) {
        auto df = d->disconnectFunction;
        d->disconnectFunction = {};
        df();
    }
    d->disconnectFunction = std::move(disconnectFunction);
}

#include "moc_AudioOutput.cpp"
