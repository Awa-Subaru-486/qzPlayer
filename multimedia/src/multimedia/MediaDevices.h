// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_MEDIADEVICES_H
#define QT_MEDIADEVICES_H
#include <qzMultimedia/MultimediaGlobal.h>
#include <QtCore/qobject.h>
#include <QtCore/qstringlist.h>

class AudioDevice;

// 媒体设备管理：枚举系统音频输入/输出设备
class QZ_MULTIMEDIA_EXPORT MediaDevices : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QList<AudioDevice> audioInputs READ audioInputs NOTIFY audioInputsChanged)
    Q_PROPERTY(QList<AudioDevice> audioOutputs READ audioOutputs NOTIFY audioOutputsChanged)
    Q_PROPERTY(AudioDevice defaultAudioInput READ defaultAudioInput NOTIFY audioInputsChanged)
    Q_PROPERTY(AudioDevice defaultAudioOutput READ defaultAudioOutput NOTIFY audioOutputsChanged)

public:
    MediaDevices(QObject *parent = nullptr);
    ~MediaDevices() override;

    // 获取音频输入设备列表
    static QList<AudioDevice> audioInputs();
    // 获取音频输出设备列表
    static QList<AudioDevice> audioOutputs();

    // 获取默认音频输入设备
    static AudioDevice defaultAudioInput();
    // 获取默认音频输出设备
    static AudioDevice defaultAudioOutput();

Q_SIGNALS:
    void audioInputsChanged();
    void audioOutputsChanged();

protected:
    void connectNotify(const QMetaMethod &signal) override;
};

#endif
