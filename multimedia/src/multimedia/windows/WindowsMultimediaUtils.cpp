// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include <QtCore/qt_windows.h>
static_assert(WINVER >= _WIN32_WINNT_WIN10, "Win10 required for newer audio formats.");

#include "WindowsMultimediaUtils_p.h"

#include <mfapi.h>
#include <mfidl.h>

VideoFrameFormat::PixelFormat WindowsMultimediaUtils::pixelFormatFromMediaSubtype(const GUID &subtype)
{
    if (subtype == MFVideoFormat_ARGB32)
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        return VideoFrameFormat::Format_BGRA8888;
#else
        return VideoFrameFormat::Format_ARGB8888;
#endif
    if (subtype == MFVideoFormat_RGB32)
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        return VideoFrameFormat::Format_BGRX8888;
#else
        return VideoFrameFormat::Format_XRGB8888;
#endif
    if (subtype == MFVideoFormat_AYUV)
        return VideoFrameFormat::Format_AYUV;
    if (subtype == MFVideoFormat_I420)
        return VideoFrameFormat::Format_YUV420P;
    if (subtype == MFVideoFormat_UYVY)
        return VideoFrameFormat::Format_UYVY;
    if (subtype == MFVideoFormat_YV12)
        return VideoFrameFormat::Format_YV12;
    if (subtype == MFVideoFormat_NV12)
        return VideoFrameFormat::Format_NV12;
    if (subtype == MFVideoFormat_YUY2)
        return VideoFrameFormat::Format_YUYV;
    if (subtype == MFVideoFormat_P010)
        return VideoFrameFormat::Format_P010;
    if (subtype == MFVideoFormat_P016)
        return VideoFrameFormat::Format_P016;
    if (subtype == MFVideoFormat_L8)
        return VideoFrameFormat::Format_Y8;
    if (subtype == MFVideoFormat_L16)
        return VideoFrameFormat::Format_Y16;
    if (subtype == MFVideoFormat_MJPG)
        return VideoFrameFormat::Format_Jpeg;

    return VideoFrameFormat::Format_Invalid;
}

GUID WindowsMultimediaUtils::videoFormatForCodec(MediaFormat::VideoCodec codec)
{
    switch (codec) {
    case MediaFormat::VideoCodec::MPEG1:
        return MFVideoFormat_MPG1;
    case MediaFormat::VideoCodec::MPEG2:
        return MFVideoFormat_MPEG2;
    case MediaFormat::VideoCodec::MPEG4:
        return MFVideoFormat_MP4V;
    case MediaFormat::VideoCodec::H264:
        return MFVideoFormat_H264;
    case MediaFormat::VideoCodec::H265:
        return MFVideoFormat_H265;
    case MediaFormat::VideoCodec::VP8:
        return MFVideoFormat_VP80;
    case MediaFormat::VideoCodec::VP9:
        return MFVideoFormat_VP90;
    case MediaFormat::VideoCodec::AV1:
        return MFVideoFormat_AV1;
    case MediaFormat::VideoCodec::WMV:
        return MFVideoFormat_WMV3;
    case MediaFormat::VideoCodec::MotionJPEG:
        return MFVideoFormat_MJPG;
    default:
        return MFVideoFormat_H264;
    }
}

MediaFormat::VideoCodec WindowsMultimediaUtils::codecForVideoFormat(GUID format)
{
    if (format == MFVideoFormat_MPG1)
        return MediaFormat::VideoCodec::MPEG1;
    if (format == MFVideoFormat_MPEG2)
        return MediaFormat::VideoCodec::MPEG2;
    if (format == MFVideoFormat_MP4V
            || format == MFVideoFormat_M4S2
            || format == MFVideoFormat_MP4S
            || format == MFVideoFormat_MP43)
        return MediaFormat::VideoCodec::MPEG4;
    if (format == MFVideoFormat_H264)
        return MediaFormat::VideoCodec::H264;
    if (format == MFVideoFormat_H265)
        return MediaFormat::VideoCodec::H265;
    if (format == MFVideoFormat_VP80)
        return MediaFormat::VideoCodec::VP8;
    if (format == MFVideoFormat_VP90)
        return MediaFormat::VideoCodec::VP9;
    if (format == MFVideoFormat_AV1)
        return MediaFormat::VideoCodec::AV1;
    if (format == MFVideoFormat_WMV1
            || format == MFVideoFormat_WMV2
            || format == MFVideoFormat_WMV3)
        return MediaFormat::VideoCodec::WMV;
    if (format == MFVideoFormat_MJPG)
        return MediaFormat::VideoCodec::MotionJPEG;
    return MediaFormat::VideoCodec::Unspecified;
}

GUID WindowsMultimediaUtils::audioFormatForCodec(MediaFormat::AudioCodec codec)
{
    switch (codec) {
    case MediaFormat::AudioCodec::MP3:
        return MFAudioFormat_MP3;
    case MediaFormat::AudioCodec::AAC:
        return MFAudioFormat_AAC;
    case MediaFormat::AudioCodec::ALAC:
        return MFAudioFormat_ALAC;
    case MediaFormat::AudioCodec::FLAC:
        return MFAudioFormat_FLAC;
    case MediaFormat::AudioCodec::Vorbis:
        return MFAudioFormat_Vorbis;
    case MediaFormat::AudioCodec::Wave:
        return MFAudioFormat_PCM;
    case MediaFormat::AudioCodec::Opus:
        return MFAudioFormat_Opus;
    case MediaFormat::AudioCodec::AC3:
        return MFAudioFormat_Dolby_AC3;
    case MediaFormat::AudioCodec::EAC3:
        return MFAudioFormat_Dolby_DDPlus;
    case MediaFormat::AudioCodec::WMA:
        return MFAudioFormat_WMAudioV9;
    default:
        return MFAudioFormat_AAC;
    }
}

MediaFormat::AudioCodec WindowsMultimediaUtils::codecForAudioFormat(GUID format)
{
    if (format == MFAudioFormat_MP3)
        return MediaFormat::AudioCodec::MP3;
    if (format == MFAudioFormat_AAC)
        return MediaFormat::AudioCodec::AAC;
    if (format == MFAudioFormat_ALAC)
        return MediaFormat::AudioCodec::ALAC;
    if (format == MFAudioFormat_FLAC)
        return MediaFormat::AudioCodec::FLAC;
    if (format == MFAudioFormat_Vorbis)
        return MediaFormat::AudioCodec::Vorbis;
    if (format == MFAudioFormat_PCM)
        return MediaFormat::AudioCodec::Wave;
    if (format == MFAudioFormat_Opus)
        return MediaFormat::AudioCodec::Opus;
    if (format == MFAudioFormat_Dolby_AC3)
        return MediaFormat::AudioCodec::AC3;
    if (format == MFAudioFormat_Dolby_DDPlus)
        return MediaFormat::AudioCodec::EAC3;
    if (format == MFAudioFormat_WMAudioV8
            || format == MFAudioFormat_WMAudioV9
            || format == MFAudioFormat_WMAudio_Lossless)
        return MediaFormat::AudioCodec::WMA;
    return MediaFormat::AudioCodec::Unspecified;
}

GUID WindowsMultimediaUtils::containerForVideoFileFormat(MediaFormat::FileFormat format)
{
    switch (format) {
    case MediaFormat::FileFormat::MPEG4:
        return MFTranscodeContainerType_MPEG4;
    case MediaFormat::FileFormat::WMV:
        return MFTranscodeContainerType_ASF;
    case MediaFormat::FileFormat::AVI:
        return MFTranscodeContainerType_AVI;
    default:
        return MFTranscodeContainerType_MPEG4;
    }
}

GUID WindowsMultimediaUtils::containerForAudioFileFormat(MediaFormat::FileFormat format)
{
    switch (format) {
    case MediaFormat::FileFormat::MP3:
        return MFTranscodeContainerType_MP3;
    case MediaFormat::FileFormat::AAC:
        return MFTranscodeContainerType_ADTS;
    case MediaFormat::FileFormat::Mpeg4Audio:
        return MFTranscodeContainerType_MPEG4;
    case MediaFormat::FileFormat::WMA:
        return MFTranscodeContainerType_ASF;
    case MediaFormat::FileFormat::FLAC:
        return MFTranscodeContainerType_FLAC;
    case MediaFormat::FileFormat::Wave:
        return MFTranscodeContainerType_WAVE;
    default:
        return MFTranscodeContainerType_MPEG4;
    }
}

