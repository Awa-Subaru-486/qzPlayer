// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_AUDIOSINK_H
#define QT_AUDIO_AUDIOSINK_H
#include <QtCore/qiodevice.h>

#include <qzMultimedia/MultimediaGlobal.h>

#include <qzMultimedia/QtAudio.h>
#include <qzMultimedia/AudioFormat.h>
#include <qzMultimedia/AudioDevice.h>

class PlatformAudioSink;

// 音频输出流：底层音频输出接口，支持直接写入音频数据
class QZ_MULTIMEDIA_EXPORT AudioSink : public QObject
{
    Q_OBJECT

public:
    explicit AudioSink(const AudioFormat &format = AudioFormat(), QObject *parent = nullptr);
    explicit AudioSink(const AudioDevice &audioDeviceInfo, const AudioFormat &format = AudioFormat(), QObject *parent = nullptr);
    ~AudioSink() override;

    // 是否为空
    bool isNull() const { return !d; }

    // 音频格式
    AudioFormat format() const;

    // 开始播放
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

    void setBufferFrameCount(qsizetype framesCount);
    qsizetype bufferFrameCount() const;

    // 可用空间
    qsizetype bytesFree() const;
    qsizetype framesFree() const;

    // 处理时间
    qint64 processedUSecs() const;
    qint64 elapsedUSecs() const;

    // 错误和状态
    QtAudio::Error error() const;
    QtAudio::State state() const;

    // 音量
    void setVolume(qreal);
    qreal volume() const;

Q_SIGNALS:
#if defined(Q_QDOC)
    void stateChanged(QtAudio::State state);
#else

    void stateChanged(Audio::State state);
#endif

private:
    Q_DISABLE_COPY(AudioSink)

    friend class PlatformAudioSink;
    PlatformAudioSink *d;
};

#endif
