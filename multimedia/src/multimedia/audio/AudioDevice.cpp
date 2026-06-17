// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "AudioSystem_p.h"
#include "AudioDevice_p.h"
#include <private/PlatformAudioDevices_p.h>
#include <private/PlatformMediaIntegration_p.h>

#include <QtCore/qmap.h>

AudioDevicePrivate::~AudioDevicePrivate() = default;

QT_DEFINE_QESDP_SPECIALIZATION_DTOR(AudioDevicePrivate);

AudioDevice::AudioDevice() = default;

AudioDevice::AudioDevice(const AudioDevice &other) = default;

AudioDevice::~AudioDevice() = default;

AudioDevice &AudioDevice::operator=(const AudioDevice &other) = default;

bool AudioDevice::operator==(const AudioDevice &other) const
{
    return mode() == other.mode() && id() == other.id();
}

bool AudioDevice::operator!=(const AudioDevice &other) const
{
    return !operator==(other);
}

bool AudioDevice::isNull() const
{
    return d == nullptr;
}

QByteArray AudioDevice::id() const
{
    return isNull() ? QByteArray() : d->id;
}

QString AudioDevice::description() const
{
    return isNull() ? QString() : d->description;
}

bool AudioDevice::isDefault() const
{
    return d ? d->isDefault : false;
}

bool AudioDevice::isFormatSupported(const AudioFormat &settings) const
{
    if (isNull())
        return false;
    if (settings.sampleRate() < d->minimumSampleRate
        || settings.sampleRate() > d->maximumSampleRate)
        return false;
    if (settings.channelCount() < d->minimumChannelCount
        || settings.channelCount() > d->maximumChannelCount)
        return false;
    if (!d->supportedSampleFormats.contains(settings.sampleFormat()))
        return false;
    return true;
}

AudioFormat AudioDevice::preferredFormat() const
{
    return isNull() ? AudioFormat() : d->preferredFormat;
}

int AudioDevice::minimumSampleRate() const
{
    return isNull() ? 0 : d->minimumSampleRate;
}

int AudioDevice::maximumSampleRate() const
{
    return isNull() ? 0 : d->maximumSampleRate;
}

int AudioDevice::minimumChannelCount() const
{
    return isNull() ? 0 : d->minimumChannelCount;
}

int AudioDevice::maximumChannelCount() const
{
    return isNull() ? 0 : d->maximumChannelCount;
}

QList<AudioFormat::SampleFormat> AudioDevice::supportedSampleFormats() const
{
    return isNull() ? QList<AudioFormat::SampleFormat>() : d->supportedSampleFormats;
}

AudioFormat::ChannelConfig AudioDevice::channelConfiguration() const
{
    return isNull() ? AudioFormat::ChannelConfigUnknown : d->channelConfiguration;
}

AudioDevice::AudioDevice(AudioDevicePrivate *p) : d(p) { }

AudioDevice::Mode AudioDevice::mode() const
{
    return d ? d->mode : Null;
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, AudioDevice::Mode mode)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    switch (mode) {
    case AudioDevice::Input:
        dbg << "AudioDevice::Input";
        break;
    case AudioDevice::Output:
        dbg << "AudioDevice::Output";
        break;
    case AudioDevice::Null:
        dbg << "AudioDevice::Null";
        break;
    }
    return dbg;
}
#endif

AudioDevice
AudioDevicePrivate::createQAudioDevice(std::unique_ptr<AudioDevicePrivate> devicePrivate)
{
    return AudioDevice(devicePrivate.release());
}

#include "moc_AudioDevice.cpp"
