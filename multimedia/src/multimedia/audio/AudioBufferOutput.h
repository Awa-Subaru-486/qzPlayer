// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_AUDIOBUFFEROUTPUT_H
#define QT_AUDIO_AUDIOBUFFEROUTPUT_H
#include <qzMultimedia/qtmultimediaexports.h>
#include <QtCore/qobject.h>

class AudioFormat;
class AudioBuffer;
class AudioBufferOutputPrivate;

// 音频缓冲输出：用于直接推送音频数据到输出设备
class QZ_MULTIMEDIA_EXPORT AudioBufferOutput : public QObject
{
    Q_OBJECT
public:
    explicit AudioBufferOutput(QObject *parent = nullptr);

    explicit AudioBufferOutput(const AudioFormat &format, QObject *parent = nullptr);

    ~AudioBufferOutput() override;

    // 获取音频格式
    AudioFormat format() const;

Q_SIGNALS:
    // 新音频缓冲就绪信号
    void audioBufferReceived(const AudioBuffer &buffer);

private:
    Q_DISABLE_COPY(AudioBufferOutput)
    Q_DECLARE_PRIVATE(AudioBufferOutput)
};

#endif
