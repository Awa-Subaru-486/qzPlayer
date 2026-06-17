// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "PlatformAudioDevices_p.h"

#include <QtCore/qdebug.h>
#include <qzMultimedia/AudioDevice.h>
#include <qzMultimedia/MediaDevices.h>
#include <qzMultimedia/private/AudioSystem_p.h>

#if defined(Q_OS_WINDOWS)
#include <qzMultimedia/private/WindowsAudioDevices_p.h>
#elif defined(Q_OS_ANDROID)
#include <qzMultimedia/private/AndroidAudioDevices_p.h>
#else
#include "portaudio/PortAudioDevices_p.h"
#endif

std::unique_ptr<PlatformAudioDevices> PlatformAudioDevices::create()
{
#if defined(Q_OS_WINDOWS)
    return std::make_unique<WindowsAudioDevices>();
#elif defined(Q_OS_ANDROID)
    return std::make_unique<AndroidAudioDevices>();
#else
    return std::make_unique<PortAudioDevices>();
#endif
}

PlatformAudioDevices::PlatformAudioDevices()
{
    qRegisterMetaType<PrivateTag>();
}

PlatformAudioDevices::~PlatformAudioDevices() = default;

QList<AudioDevice> PlatformAudioDevices::audioInputs() const
{
    return m_audioInputs.ensure([this]() {
        return findAudioInputs();
    });
}

QList<AudioDevice> PlatformAudioDevices::audioOutputs() const
{
    return m_audioOutputs.ensure([this]() {
        return findAudioOutputs();
    });
}

void PlatformAudioDevices::onAudioInputsChanged()
{
    m_audioInputs.reset();
    emit audioInputsChanged(PrivateTag{});
}

void PlatformAudioDevices::onAudioOutputsChanged()
{
    m_audioOutputs.reset();
    emit audioOutputsChanged(PrivateTag{});
}

void PlatformAudioDevices::updateAudioInputsCache()
{
    if (m_audioInputs.update(findAudioInputs()))
        emit audioInputsChanged(PrivateTag{});
}

void PlatformAudioDevices::updateAudioOutputsCache()
{
    if (m_audioOutputs.update(findAudioOutputs()))
        emit audioOutputsChanged(PrivateTag{});
}

PlatformAudioSource *PlatformAudioDevices::createAudioSource(const AudioDevice &,
                                                               const AudioFormat &, QObject *)
{
    return nullptr;
}
PlatformAudioSink *PlatformAudioDevices::createAudioSink(const AudioDevice &,
                                                           const AudioFormat &, QObject *)
{
    return nullptr;
}

PlatformAudioSource *PlatformAudioDevices::audioInputDevice(AudioFormat format,
                                                              const AudioDevice &deviceInfo,
                                                              QObject *parent)
{
    AudioDevice device = deviceInfo;
    if (device.isNull())
        device = MediaDevices::defaultAudioInput();

    if (device.isNull())
        return nullptr;

    if (format == AudioFormat{})
        format = device.preferredFormat();

    return createAudioSource(device, format, parent);
}

PlatformAudioSink *PlatformAudioDevices::audioOutputDevice(AudioFormat format,
                                                             const AudioDevice &deviceInfo,
                                                             QObject *parent)
{
    AudioDevice device = deviceInfo;
    if (device.isNull())
        device = MediaDevices::defaultAudioOutput();

    if (device.isNull())
        return nullptr;

    if (format == AudioFormat{})
        format = device.preferredFormat();

    return createAudioSink(device, format, parent);
}

#include "moc_PlatformAudioDevices_p.cpp"
