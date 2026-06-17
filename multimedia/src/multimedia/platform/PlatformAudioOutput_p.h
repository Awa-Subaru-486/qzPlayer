// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_PLATFORM_PLATFORMAUDIOOUTPUT_P_H
#define QT_PLATFORM_PLATFORMAUDIOOUTPUT_P_H

#include <private/MultimediaGlobal_p.h>
#include <AudioDevice.h>

class AudioOutput;

// 平台音频输出抽象接口：扬声器等音频播放
class QZ_MULTIMEDIA_EXPORT PlatformAudioOutput
{
public:
    explicit PlatformAudioOutput(AudioOutput *qq) : q(qq) { }
    virtual ~PlatformAudioOutput() = default;

    // 设置音频设备
    virtual void setAudioDevice(const AudioDevice &) {}
    // 设置静音
    virtual void setMuted(bool ) {}
    // 设置音量
    virtual void setVolume(float ) {}

    AudioOutput *q = nullptr;
    AudioDevice device;
    float volume = 1.;
    bool muted = false;
    std::function<void()>  disconnectFunction;
};

#endif
