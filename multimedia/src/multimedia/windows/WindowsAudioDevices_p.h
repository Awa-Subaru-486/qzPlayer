// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_WINDOWS_WINDOWSAUDIODEVICES_P_H
#define QT_WINDOWS_WINDOWSAUDIODEVICES_P_H

#include <private/PlatformAudioDevices_p.h>
#include <QtCore/private/qcomptr_p.h>
#include <private/ComInitializer_p.h>
#include <private/WindowsMediaFoundation_p.h>

#include <AudioDevice.h>

struct IAudioClient3;
struct IMMDevice;
struct IMMDeviceEnumerator;

class WindowsEngine;
class CMMNotificationClient;

// Windows 音频设备管理器：基于 WASAPI 枚举和管理音频设备
class WindowsAudioDevices : public PlatformAudioDevices
{
public:
    WindowsAudioDevices();
    virtual ~WindowsAudioDevices();

    PlatformAudioSource *createAudioSource(const AudioDevice &, const AudioFormat &,
                                            QObject *parent) override;
    PlatformAudioSink *createAudioSink(const AudioDevice &, const AudioFormat &,
                                        QObject *parent) override;

    using PlatformAudioDevices::onAudioInputsChanged;
    using PlatformAudioDevices::onAudioOutputsChanged;

    QLatin1String backendName() const override { return QLatin1String{ "WASAPI" }; }

    static QByteArray currentDefaultOutputDeviceId() { return s_currentDefaultOutputDeviceId; }
    static QByteArray currentDefaultInputDeviceId() { return s_currentDefaultInputDeviceId; }
    static void setCurrentDefaultOutputDeviceId(const QByteArray &id) { s_currentDefaultOutputDeviceId = id; }
    static void setCurrentDefaultInputDeviceId(const QByteArray &id) { s_currentDefaultInputDeviceId = id; }

protected:
    QList<AudioDevice> findAudioInputs() const override;
    QList<AudioDevice> findAudioOutputs() const override;

private:
    ComInitializer m_comInitializer;
    MFRuntimeInit m_wmfRuntime{ WindowsMediaFoundation::instance() };
    QList<AudioDevice> availableDevices(AudioDevice::Mode mode) const;

    ComPtr<IMMDeviceEnumerator> m_deviceEnumerator;
    ComPtr<CMMNotificationClient> m_notificationClient;

    static QByteArray s_currentDefaultOutputDeviceId;
    static QByteArray s_currentDefaultInputDeviceId;

    friend CMMNotificationClient;
};

#endif
