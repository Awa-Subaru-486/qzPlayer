// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_AUDIOINPUT_H
#define QT_AUDIO_AUDIOINPUT_H
#include <QtCore/qobject.h>
#include <qzMultimedia/MultimediaGlobal.h>
#include <qzMultimedia/QtAudio.h>

#include <functional>

class AudioDevice;
class PlatformAudioInput;

// 音频输入：用于从麦克风等设备采集音频
class QZ_MULTIMEDIA_EXPORT AudioInput : public QObject
{
    Q_OBJECT
    Q_PROPERTY(AudioDevice device READ device WRITE setDevice NOTIFY deviceChanged)
    Q_PROPERTY(float volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ isMuted WRITE setMuted NOTIFY mutedChanged)

public:
    explicit AudioInput(QObject *parent = nullptr);
    explicit AudioInput(const AudioDevice &deviceInfo, QObject *parent = nullptr);
    ~AudioInput() override;

    // 音频设备
    AudioDevice device() const;
    // 音量
    float volume() const;
    // 是否静音
    bool isMuted() const;

public Q_SLOTS:
    void setDevice(const AudioDevice &device);
    void setVolume(float volume);
    void setMuted(bool muted);

Q_SIGNALS:
    void deviceChanged();
    void volumeChanged(float volume);
    void mutedChanged(bool muted);

private:
    PlatformAudioInput *handle() const { return d; }
    void setDisconnectFunction(std::function<void()> disconnectFunction);
    friend class QMediaCaptureSession;
    Q_DISABLE_COPY(AudioInput)
    PlatformAudioInput *d = nullptr;
};

#endif
