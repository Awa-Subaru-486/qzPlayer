// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.


#ifndef QT_WINDOWS_WINDOWSAUDIODEVICE_P_H
#define QT_WINDOWS_WINDOWSAUDIODEVICE_P_H
#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>
#include <QtCore/private/qcomptr_p.h>

#include <qzMultimedia/AudioDevice.h>
#include <qzMultimedia/private/AudioFormat_p.h>
#include <qzMultimedia/private/AudioSystem_p.h>
#include <qzMultimedia/private/AudioDevice_p.h>

struct IMMDevice;

// Windows 音频设备：封装 IMMDevice 设备信息
class WindowsAudioDevice : public AudioDevicePrivate
{
public:
    WindowsAudioDevice(QByteArray deviceId, ComPtr<IMMDevice> immdev, QString description,
                        AudioDevice::Mode mode);
    ~WindowsAudioDevice();

    // 打开设备
    ComPtr<IMMDevice> open() const;

    // 通过设备 ID 打开设备
    static ComPtr<IMMDevice> openDeviceById(const QByteArray &deviceId);

    // 探测的声道数和采样率范围
    std::pair<int, int> m_probedChannelCountRange{ 1, 2 };
    std::pair<int, int> m_probedSampleRateRange{
        QtMultimediaPrivate::allSupportedSampleRates.front(),
        QtMultimediaPrivate::allSupportedSampleRates.back(),
    };
};

#endif
