// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_AUDIODEVICE_H
#define QT_AUDIO_AUDIODEVICE_H
#include <QtCore/qobject.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qlist.h>

#include <qzMultimedia/MultimediaGlobal.h>

#include <qzMultimedia/QtAudio.h>
#include <qzMultimedia/AudioFormat.h>

class AudioDevicePrivate;
QT_DECLARE_QESDP_SPECIALIZATION_DTOR_WITH_EXPORT(AudioDevicePrivate, QZ_MULTIMEDIA_EXPORT)

// 音频设备：表示系统中的一个音频输入或输出设备
class QZ_MULTIMEDIA_EXPORT AudioDevice
{
    Q_GADGET
    Q_PROPERTY(QByteArray id READ id CONSTANT)
    Q_PROPERTY(QString description READ description CONSTANT)
    Q_PROPERTY(bool isDefault READ isDefault CONSTANT)
    Q_PROPERTY(Mode mode READ mode CONSTANT)
public:
    // 设备模式枚举
    enum Mode {
        Null,
        Input,
        Output
    };
    Q_ENUM(Mode)

    AudioDevice();
    AudioDevice(const AudioDevice& other);
    ~AudioDevice();

    AudioDevice(AudioDevice &&other) noexcept = default;
    QT_MOVE_ASSIGNMENT_OPERATOR_IMPL_VIA_PURE_SWAP(AudioDevice)
    void swap(AudioDevice &other) noexcept
    { d.swap(other.d); }

    AudioDevice& operator=(const AudioDevice& other);

    bool operator==(const AudioDevice &other) const;
    bool operator!=(const AudioDevice &other) const;

    // 是否为空设备
    bool isNull() const;

    // 设备 ID 和描述
    QByteArray id() const;
    QString description() const;

    // 是否为默认设备
    bool isDefault() const;
    // 设备模式（输入/输出）
    AudioDevice::Mode mode() const;

    // 格式支持查询
    bool isFormatSupported(const AudioFormat &format) const;
    AudioFormat preferredFormat() const;

    // 采样率范围
    int minimumSampleRate() const;
    int maximumSampleRate() const;
    // 声道数范围
    int minimumChannelCount() const;
    int maximumChannelCount() const;
    // 支持的采样格式
    QList<AudioFormat::SampleFormat> supportedSampleFormats() const;
    // 声道配置
    AudioFormat::ChannelConfig channelConfiguration() const;

#if QT_DEPRECATED_SINCE(6, 10)
    QT_DEPRECATED_VERSION_X_6_10("The method is internal and deprecated")
    const AudioDevicePrivate *handle() const { return d.get(); }
#endif
private:
    friend class AudioDevicePrivate;
    explicit AudioDevice(AudioDevicePrivate *p);
    QExplicitlySharedDataPointer<AudioDevicePrivate> d;
};

#ifndef QT_NO_DEBUG_STREAM
QZ_MULTIMEDIA_EXPORT QDebug operator<<(QDebug dbg, AudioDevice::Mode mode);
#endif

#endif
