// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_AUDIO_H
#define QT_AUDIO_AUDIO_H
#if 0
#pragma qt_class(Audio)
#endif

#include <qzMultimedia/MultimediaGlobal.h>

#if defined(Q_QDOC)
namespace QtAudio
#else
namespace Audio
#endif
{
enum Error
{
    NoError,
    OpenError,
    IOError,
    UnderrunError,
    FatalError,
};
enum State
{
    ActiveState,
    SuspendedState,
    StoppedState,
    IdleState,
};

enum VolumeScale
{
    LinearVolumeScale,
    CubicVolumeScale,
    LogarithmicVolumeScale,
    DecibelVolumeScale,
};

}

namespace QtAudio {

#if !defined(Q_QDOC)
using Error = Audio::Error;
using State = Audio::State;
using VolumeScale = Audio::VolumeScale;

inline constexpr auto NoError = Audio::NoError;
inline constexpr auto OpenError = Audio::OpenError;
inline constexpr auto IOError = Audio::IOError;
inline constexpr auto UnderrunError = Audio::UnderrunError;
inline constexpr auto FatalError = Audio::FatalError;
inline constexpr auto ActiveState = Audio::ActiveState;
inline constexpr auto SuspendedState = Audio::SuspendedState;
inline constexpr auto StoppedState = Audio::StoppedState;
inline constexpr auto IdleState = Audio::IdleState;
inline constexpr auto LinearVolumeScale = Audio::LinearVolumeScale;
inline constexpr auto CubicVolumeScale = Audio::CubicVolumeScale;
inline constexpr auto LogarithmicVolumeScale = Audio::LogarithmicVolumeScale;
inline constexpr auto DecibelVolumeScale = Audio::DecibelVolumeScale;
#endif

QZ_MULTIMEDIA_EXPORT float convertVolume(float volume, VolumeScale from, VolumeScale to);

}

#if !defined(Q_QDOC)
// 音频命名空间：定义音频状态、错误码等枚举类型
namespace Audio
{
using QtAudio::convertVolume;
}
#endif

#ifndef QT_NO_DEBUG_STREAM
QZ_MULTIMEDIA_EXPORT QDebug operator<<(QDebug dbg, QtAudio::Error error);
QZ_MULTIMEDIA_EXPORT QDebug operator<<(QDebug dbg, QtAudio::State state);
QZ_MULTIMEDIA_EXPORT QDebug operator<<(QDebug dbg, QtAudio::VolumeScale role);
#endif

#endif
