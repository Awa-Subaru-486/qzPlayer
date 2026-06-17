// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_AUDIOSOURCE_H
#define QT_AUDIO_AUDIOSOURCE_H
#include <QtCore/qiodevice.h>

#include <qzMultimedia/MultimediaGlobal.h>

#include <qzMultimedia/QtAudio.h>
#include <qzMultimedia/AudioFormat.h>
#include <qzMultimedia/AudioDevice.h>

class PlatformAudioSource;

// 音频输入流：底层音频采集接口，用于读取麦克风数据
class QZ_MULTIMEDIA_EXPORT AudioSource : public QObject
{
    Q_OBJECT

public:
    explicit AudioSource(const AudioFormat &format = AudioFormat(), QObject *parent = nullptr);
    explicit AudioSource(const AudioDevice &audioDeviceInfo, const AudioFormat &format = AudioFormat(), QObject *parent = nullptr);
    ~AudioSource() override;

    // 是否为空
    bool isNull() const { return !d; }

    // 音频格式
    AudioFormat format() const;

    // 开始采集
    void start(QIODevice *device);
    QIODevice* start();

    // 停止/重置/暂停/恢复
    void stop();
    void reset();
    void suspend();
    void resume();

    // 缓冲区大小设置
    void setBufferSize(qsizetype bytes);
    qsizetype bufferSize() const;

    void setBufferFrameCount(qsizetype frames);
    qsizetype bufferFrameCount() const;

    // 可用数据量
    qsizetype bytesAvailable() const;
    qsizetype framesAvailable() const;

    // 音量
    void setVolume(qreal volume);
    qreal volume() const;

    // 处理时间
    qint64 processedUSecs() const;
    qint64 elapsedUSecs() const;

    // 错误和状态
    QtAudio::Error error() const;
    QtAudio::State state() const;

Q_SIGNALS:
#if defined(Q_QDOC)
    void stateChanged(QtAudio::State state);
#else

    void stateChanged(Audio::State state);
#endif

private:
    Q_DISABLE_COPY(AudioSource)
    friend class PlatformAudioSource;

    PlatformAudioSource *d;
};

#endif
