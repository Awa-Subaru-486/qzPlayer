// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_PLATFORM_PLATFORMAUDIORESAMPLER_P_H
#define QT_PLATFORM_PLATFORMAUDIORESAMPLER_P_H

#include <private/MultimediaGlobal_p.h>
#include <AudioBuffer.h>

// 平台音频重采样器抽象接口：音频格式转换
class PlatformAudioResampler
{
public:
    virtual ~PlatformAudioResampler() = default;

    // 执行重采样
    virtual AudioBuffer resample(const char *data, size_t size) = 0;
};

#endif
