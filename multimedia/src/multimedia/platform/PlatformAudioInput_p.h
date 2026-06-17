// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_PLATFORM_PLATFORMAUDIOINPUT_P_H
#define QT_PLATFORM_PLATFORMAUDIOINPUT_P_H

#include <private/MultimediaGlobal_p.h>
#include <AudioDevice.h>

#include <functional>

class AudioInput;

// 平台音频输入抽象接口：麦克风等音频采集
class QZ_MULTIMEDIA_EXPORT PlatformAudioInput
{
public:
    explicit PlatformAudioInput(AudioInput *qq) : q(qq) { }
    virtual ~PlatformAudioInput() = default;

    // 设置音频设备
    virtual void setAudioDevice(const AudioDevice & ) { }
    // 设置静音
    virtual void setMuted(bool ) {}
    // 设置音量
    virtual void setVolume(float ) {}

    AudioInput *q = nullptr;
    AudioDevice device;
    float volume = 1.;
    bool muted = false;
    std::function<void()> disconnectFunction;
};

#endif
