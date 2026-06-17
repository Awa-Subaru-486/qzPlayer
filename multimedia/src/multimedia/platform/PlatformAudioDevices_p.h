// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_PLATFORM_PLATFORMAUDIODEVICES_P_H
#define QT_PLATFORM_PLATFORMAUDIODEVICES_P_H

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <qzMultimedia/private/AudioDevice_p.h>
#include <qzMultimedia/private/CachedValue_p.h>
#include <qzMultimedia/private/MultimediaGlobal_p.h>
#include <memory>

class PlatformAudioSource;
class PlatformAudioSink;
class AudioFormat;

// 平台音频设备枚举抽象接口：查询系统可用输入/输出设备
class QZ_MULTIMEDIA_EXPORT PlatformAudioDevices : public QObject
{
    Q_OBJECT

    QT_DEFINE_TAG_STRUCT(PrivateTag);

public:
    PlatformAudioDevices();
    ~PlatformAudioDevices() override;

    // 创建平台音频设备管理器
    static std::unique_ptr<PlatformAudioDevices> create();

    // 获取可用输入/输出设备列表
    QList<AudioDevice> audioInputs() const;
    QList<AudioDevice> audioOutputs() const;

    // 创建音频源/宿
    virtual PlatformAudioSource *createAudioSource(const AudioDevice &, const AudioFormat &,
                                                    QObject *parent);
    virtual PlatformAudioSink *createAudioSink(const AudioDevice &, const AudioFormat &,
                                                QObject *parent);

    PlatformAudioSource *audioInputDevice(AudioFormat, const AudioDevice &, QObject *parent);
    PlatformAudioSink *audioOutputDevice(AudioFormat, const AudioDevice &, QObject *parent);

    void initVideoDevicesConnection();
    // 获取后端名称
    virtual QLatin1String backendName() const { return QLatin1String{ "null" }; }

protected:
    virtual QList<AudioDevice> findAudioInputs() const { return {}; }
    virtual QList<AudioDevice> findAudioOutputs() const { return {}; }

    void onAudioInputsChanged();
    void onAudioOutputsChanged();

    void updateAudioInputsCache();
    void updateAudioOutputsCache();

Q_SIGNALS:
    void audioInputsChanged(PrivateTag);
    void audioOutputsChanged(PrivateTag);

private:
    mutable CachedValue<QList<AudioDevice>> m_audioInputs;
    mutable CachedValue<QList<AudioDevice>> m_audioOutputs;
};

#endif
