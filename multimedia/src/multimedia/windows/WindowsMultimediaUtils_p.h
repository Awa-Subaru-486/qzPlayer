// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_WINDOWS_WINDOWSMULTIMEDIAUTILS_P_H
#define QT_WINDOWS_WINDOWSMULTIMEDIAUTILS_P_H

#include <private/MultimediaGlobal_p.h>
#include <private/PlatformMediaFormatInfo_p.h>
#include <VideoFrameFormat.h>
#include <guiddef.h>
#include <qstring.h>

namespace WindowsMultimediaUtils {

    QZ_MULTIMEDIA_EXPORT VideoFrameFormat::PixelFormat pixelFormatFromMediaSubtype(const GUID &subtype);

    QZ_MULTIMEDIA_EXPORT GUID videoFormatForCodec(MediaFormat::VideoCodec codec);

    QZ_MULTIMEDIA_EXPORT MediaFormat::VideoCodec codecForVideoFormat(GUID format);

    QZ_MULTIMEDIA_EXPORT GUID audioFormatForCodec(MediaFormat::AudioCodec codec);

    QZ_MULTIMEDIA_EXPORT MediaFormat::AudioCodec codecForAudioFormat(GUID format);

    QZ_MULTIMEDIA_EXPORT GUID containerForVideoFileFormat(MediaFormat::FileFormat format);

    QZ_MULTIMEDIA_EXPORT GUID containerForAudioFileFormat(MediaFormat::FileFormat format);
}

#endif
