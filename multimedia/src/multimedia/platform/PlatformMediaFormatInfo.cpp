// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "PlatformMediaFormatInfo_p.h"

#include <set>

PlatformMediaFormatInfo::PlatformMediaFormatInfo() = default;

PlatformMediaFormatInfo::~PlatformMediaFormatInfo() = default;

QList<MediaFormat::FileFormat> PlatformMediaFormatInfo::supportedFileFormats(const MediaFormat &constraints, MediaFormat::ConversionMode m) const
{
    std::set<MediaFormat::FileFormat> formats;

    const auto &codecMap = (m == MediaFormat::Encode) ? encoders : decoders;
    for (const auto &m : codecMap) {
        if (constraints.audioCodec() != MediaFormat::AudioCodec::Unspecified && !m.audio.contains(constraints.audioCodec()))
            continue;
        if (constraints.videoCodec() != MediaFormat::VideoCodec::Unspecified && !m.video.contains(constraints.videoCodec()))
            continue;
        formats.insert(m.format);
    }
    return { formats.begin(), formats.end() };
}

QList<MediaFormat::AudioCodec> PlatformMediaFormatInfo::supportedAudioCodecs(const MediaFormat &constraints, MediaFormat::ConversionMode m) const
{
    std::set<MediaFormat::AudioCodec> codecs;

    const auto &codecMap = (m == MediaFormat::Encode) ? encoders : decoders;
    for (const auto &m : codecMap) {
        if (constraints.fileFormat() != MediaFormat::UnspecifiedFormat && m.format != constraints.fileFormat())
            continue;
        if (constraints.videoCodec() != MediaFormat::VideoCodec::Unspecified && !m.video.contains(constraints.videoCodec()))
            continue;
        for (const auto &c : m.audio)
            codecs.insert(c);
    }

    return { codecs.begin(), codecs.end() };
}

QList<MediaFormat::VideoCodec> PlatformMediaFormatInfo::supportedVideoCodecs(const MediaFormat &constraints, MediaFormat::ConversionMode m) const
{
    std::set<MediaFormat::VideoCodec> codecs;

    const auto &codecMap = (m == MediaFormat::Encode) ? encoders : decoders;
    for (const auto &m : codecMap) {
        if (constraints.fileFormat() != MediaFormat::UnspecifiedFormat && m.format != constraints.fileFormat())
            continue;
        if (constraints.audioCodec() != MediaFormat::AudioCodec::Unspecified && !m.audio.contains(constraints.audioCodec()))
            continue;
        for (const auto &c : m.video)
            codecs.insert(c);
    }
    return { codecs.begin(), codecs.end() };
}

bool PlatformMediaFormatInfo::isSupported(const MediaFormat &format, MediaFormat::ConversionMode m) const
{
    const auto &codecMap = (m == MediaFormat::Encode) ? encoders : decoders;

    for (const auto &m : codecMap) {
        if (m.format != format.fileFormat())
            continue;
        if (format.audioCodec() != MediaFormat::AudioCodec::Unspecified && !m.audio.contains(format.audioCodec()))
            continue;
        if (format.videoCodec() != MediaFormat::VideoCodec::Unspecified && !m.video.contains(format.videoCodec()))
            continue;
        return true;
    }
    return false;
}

