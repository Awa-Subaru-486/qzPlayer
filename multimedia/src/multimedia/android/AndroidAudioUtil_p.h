// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_ANDROID_ANDROIDAUDIOUTIL_P_H
#define QT_ANDROID_ANDROIDAUDIOUTIL_P_H

#include <qzMultimedia/AudioDevice.h>
#include <qzMultimedia/MultimediaGlobal.h>

QT_BEGIN_NAMESPACE

namespace AndroidAudioUtil {

QZ_MULTIMEDIA_EXPORT bool supportsLowLatency();
QZ_MULTIMEDIA_EXPORT bool isDefaultBluetoothDevice(const AudioDevice &device);

// 获取 content:// URI 的显示名称（通过 ContentResolver 查询 DISPLAY_NAME）
QZ_MULTIMEDIA_EXPORT QString getContentDisplayName(const QString &uriString);

} // namespace AndroidAudioUtil

QT_END_NAMESPACE

#endif
