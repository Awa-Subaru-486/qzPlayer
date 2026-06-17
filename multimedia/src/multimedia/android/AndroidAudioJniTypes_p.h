// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_ANDROID_ANDROIDAUDIOJNITYPES_P_H
#define QT_ANDROID_ANDROIDAUDIOJNITYPES_P_H

#include <QtCore/qjnitypes.h>

QT_BEGIN_NAMESPACE

Q_DECLARE_JNI_CLASS(QtAudioDeviceManager,
                    "qz/multimedia/android/QtAudioDeviceManager");

Q_DECLARE_JNI_CLASS(AudioDeviceInfo, "android/media/AudioDeviceInfo");

QT_END_NAMESPACE

#endif
