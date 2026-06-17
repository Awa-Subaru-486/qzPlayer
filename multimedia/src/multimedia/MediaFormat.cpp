// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "MediaFormat.h"
#include "private/PlatformMediaIntegration_p.h"
#include "private/PlatformMediaFormatInfo_p.h"
#include "private/MultimediaEnumToStringConverter_p.h"
#if QT_CONFIG(mimetype)
#include <QtCore/qmimedatabase.h>
#endif

struct DescriptionRole{};

QT_MM_MAKE_STRING_RESOLVER(MediaFormat::FileFormat, QtMultimediaPrivate::EnumName,
                           (MediaFormat::UnspecifiedFormat, "Unspecified")
                           (MediaFormat::WMV, "WMV")
                           (MediaFormat::AVI, "AVI")
                           (MediaFormat::Matroska, "Matroska")
                           (MediaFormat::MPEG4, "MPEG-4")
                           (MediaFormat::Ogg, "Ogg")
                           (MediaFormat::QuickTime, "QuickTime")
                           (MediaFormat::WebM, "WebM")

                           (MediaFormat::Mpeg4Audio, "MPEG-4 Audio")
                           (MediaFormat::AAC, "AAC")
                           (MediaFormat::WMA, "WMA")
                           (MediaFormat::MP3, "MP3")
                           (MediaFormat::FLAC, "FLAC")
                           (MediaFormat::Wave, "Wave")
                          );

QT_MM_MAKE_STRING_RESOLVER(MediaFormat::FileFormat, DescriptionRole,
                           (MediaFormat::UnspecifiedFormat, "Unspecified File Format")
                           (MediaFormat::WMV, "Windows Media Video (WMV)")
                           (MediaFormat::AVI, "Audio Video Interleave (AVI)")
                           (MediaFormat::Matroska, "Matroska Multimedia Container")
                           (MediaFormat::MPEG4, "MPEG-4 Video Container")
                           (MediaFormat::Ogg, "Ogg")
                           (MediaFormat::QuickTime, "QuickTime Container")
                           (MediaFormat::WebM, "WebM")

                           (MediaFormat::Mpeg4Audio, "MPEG-4 Audio")
                           (MediaFormat::AAC, "Advanced Audio Coding (AAC)")
                           (MediaFormat::WMA, "Windows Media Audio (WMA)")
                           (MediaFormat::MP3, "MP3")
                           (MediaFormat::FLAC, "Free Lossless Audio Codec (FLAC)")
                           (MediaFormat::Wave, "Wave Audio File Format (WAVE)")
                          );

QT_MM_MAKE_STRING_RESOLVER(MediaFormat::AudioCodec, QtMultimediaPrivate::EnumName,
                           (MediaFormat::AudioCodec::Unspecified, "Unspecified")
                           (MediaFormat::AudioCodec::MP3, "MP3")
                           (MediaFormat::AudioCodec::AAC, "AAC")
                           (MediaFormat::AudioCodec::AC3, "AC3")
                           (MediaFormat::AudioCodec::EAC3, "EAC3")
                           (MediaFormat::AudioCodec::FLAC, "FLAC")
                           (MediaFormat::AudioCodec::DolbyTrueHD, "DolbyTrueHD")
                           (MediaFormat::AudioCodec::Opus, "Opus")
                           (MediaFormat::AudioCodec::Vorbis, "Vorbis")
                           (MediaFormat::AudioCodec::Wave, "Wave")
                           (MediaFormat::AudioCodec::WMA, "WMA")
                           (MediaFormat::AudioCodec::ALAC, "ALAC")
                          );

QT_MM_MAKE_STRING_RESOLVER(MediaFormat::AudioCodec, DescriptionRole,
                           (MediaFormat::AudioCodec::Unspecified, "Unspecified Audio Codec")
                           (MediaFormat::AudioCodec::MP3, "MP3")
                           (MediaFormat::AudioCodec::AAC, "Advanced Audio Coding (AAC)")
                           (MediaFormat::AudioCodec::AC3, "Dolby Digital (AC3)")
                           (MediaFormat::AudioCodec::EAC3, "Dolby Digital Plus (E-AC3)")
                           (MediaFormat::AudioCodec::FLAC, "Free Lossless Audio Codec (FLAC)")
                           (MediaFormat::AudioCodec::DolbyTrueHD, "Dolby True HD")
                           (MediaFormat::AudioCodec::Opus, "Opus")
                           (MediaFormat::AudioCodec::Vorbis, "Vorbis")
                           (MediaFormat::AudioCodec::Wave, "Wave")
                           (MediaFormat::AudioCodec::WMA, "Windows Media Audio (WMA)")
                           (MediaFormat::AudioCodec::ALAC, "Apple Lossless Audio Codec (ALAC)")
                          );

QT_MM_MAKE_STRING_RESOLVER(MediaFormat::VideoCodec, QtMultimediaPrivate::EnumName,
                           (MediaFormat::VideoCodec::Unspecified, "Unspecified")
                           (MediaFormat::VideoCodec::MPEG1, "MPEG1")
                           (MediaFormat::VideoCodec::MPEG2, "MPEG2")
                           (MediaFormat::VideoCodec::MPEG4, "MPEG4")
                           (MediaFormat::VideoCodec::H264, "H264")
                           (MediaFormat::VideoCodec::H265, "H265")
                           (MediaFormat::VideoCodec::VP8, "VP8")
                           (MediaFormat::VideoCodec::VP9, "VP9")
                           (MediaFormat::VideoCodec::AV1, "AV1")
                           (MediaFormat::VideoCodec::Theora, "Theora")
                           (MediaFormat::VideoCodec::WMV, "WMV")
                           (MediaFormat::VideoCodec::MotionJPEG, "MotionJPEG")
                          );

QT_MM_MAKE_STRING_RESOLVER(MediaFormat::VideoCodec, DescriptionRole,
                           (MediaFormat::VideoCodec::Unspecified, "Unspecified Video Codec")
                           (MediaFormat::VideoCodec::MPEG1, "MPEG-1 Video")
                           (MediaFormat::VideoCodec::MPEG2, "MPEG-2 Video")
                           (MediaFormat::VideoCodec::MPEG4, "MPEG-4 Video")
                           (MediaFormat::VideoCodec::H264, "H.264")
                           (MediaFormat::VideoCodec::H265, "H.265")
                           (MediaFormat::VideoCodec::VP8, "VP8")
                           (MediaFormat::VideoCodec::VP9, "VP9")
                           (MediaFormat::VideoCodec::AV1, "AV1")
                           (MediaFormat::VideoCodec::Theora, "Theora")
                           (MediaFormat::VideoCodec::WMV, "Windows Media Video (WMV)")
                           (MediaFormat::VideoCodec::MotionJPEG, "MotionJPEG")
                          );

namespace {

const char *mimeTypeForFormat[MediaFormat::LastFileFormat + 2] =
{
    "",
    "video/x-ms-wmv",
    "video/x-msvideo",
    "video/x-matroska",
    "video/mp4",
    "video/ogg",
    "video/quicktime",
    "video/webm",

    "audio/mp4",
    "audio/aac",
    "audio/x-ms-wma",
    "audio/mpeg",
    "audio/flac",
    "audio/wav"
};

constexpr MediaFormat::FileFormat videoFormatPriorityList[] =
{
    MediaFormat::MPEG4,
    MediaFormat::QuickTime,
    MediaFormat::AVI,
    MediaFormat::WebM,
    MediaFormat::WMV,
    MediaFormat::Matroska,
    MediaFormat::Ogg,
    MediaFormat::UnspecifiedFormat
};

constexpr MediaFormat::FileFormat audioFormatPriorityList[] =
{
    MediaFormat::Mpeg4Audio,
    MediaFormat::MP3,
    MediaFormat::WMA,
    MediaFormat::FLAC,
    MediaFormat::Wave,
    MediaFormat::UnspecifiedFormat
};

constexpr MediaFormat::AudioCodec audioPriorityList[] =
{
    MediaFormat::AudioCodec::AAC,
    MediaFormat::AudioCodec::MP3,
    MediaFormat::AudioCodec::AC3,
    MediaFormat::AudioCodec::Opus,
    MediaFormat::AudioCodec::EAC3,
    MediaFormat::AudioCodec::DolbyTrueHD,
    MediaFormat::AudioCodec::WMA,
    MediaFormat::AudioCodec::FLAC,
    MediaFormat::AudioCodec::Vorbis,
    MediaFormat::AudioCodec::Wave,
    MediaFormat::AudioCodec::Unspecified
};

constexpr MediaFormat::VideoCodec videoPriorityList[] =
{
    MediaFormat::VideoCodec::H265,
    MediaFormat::VideoCodec::VP9,
    MediaFormat::VideoCodec::H264,
    MediaFormat::VideoCodec::AV1,
    MediaFormat::VideoCodec::VP8,
    MediaFormat::VideoCodec::WMV,
    MediaFormat::VideoCodec::Theora,
    MediaFormat::VideoCodec::MPEG4,
    MediaFormat::VideoCodec::MPEG2,
    MediaFormat::VideoCodec::MPEG1,
    MediaFormat::VideoCodec::MotionJPEG,
};

}

class MediaFormatPrivate : public QSharedData
{};

QT_DEFINE_QESDP_SPECIALIZATION_DTOR(MediaFormatPrivate);

MediaFormat::MediaFormat(FileFormat format)
    : fmt(format)
{
}

MediaFormat::~MediaFormat() = default;

MediaFormat::MediaFormat(const MediaFormat &other) noexcept = default;

MediaFormat &MediaFormat::operator=(const MediaFormat &other) noexcept = default;

bool MediaFormat::isSupported(ConversionMode mode) const
{
    return PlatformMediaIntegration::instance()->formatInfo()->isSupported(*this, mode);
}

#if QT_CONFIG(mimetype)
QMimeType MediaFormat::mimeType() const
{
    return QMimeDatabase().mimeTypeForName(QString::fromLatin1(mimeTypeForFormat[fmt + 1]));
}
#endif

QList<MediaFormat::FileFormat> MediaFormat::supportedFileFormats(MediaFormat::ConversionMode m)
{
    return PlatformMediaIntegration::instance()->formatInfo()->supportedFileFormats(*this, m);
}

QList<MediaFormat::VideoCodec> MediaFormat::supportedVideoCodecs(MediaFormat::ConversionMode m)
{
    return PlatformMediaIntegration::instance()->formatInfo()->supportedVideoCodecs(*this, m);
}

QList<MediaFormat::AudioCodec> MediaFormat::supportedAudioCodecs(MediaFormat::ConversionMode m)
{
    return PlatformMediaIntegration::instance()->formatInfo()->supportedAudioCodecs(*this, m);
}

QString MediaFormat::fileFormatName(MediaFormat::FileFormat fileFormat)
{
    return QtMultimediaPrivate::StringResolver<MediaFormat::FileFormat>::toQString(fileFormat)
            .value_or(QStringLiteral("Unknown File Format"));
}

QString MediaFormat::audioCodecName(MediaFormat::AudioCodec codec)
{
    return QtMultimediaPrivate::StringResolver<MediaFormat::AudioCodec>::toQString(codec).value_or(
            QStringLiteral("Unknown Audio Codec"));
}

QString MediaFormat::videoCodecName(MediaFormat::VideoCodec codec)
{
    return QtMultimediaPrivate::StringResolver<MediaFormat::VideoCodec>::toQString(codec).value_or(
            QStringLiteral("Unknown Video Codec"));
}

QString MediaFormat::fileFormatDescription(MediaFormat::FileFormat fileFormat)
{
    return QtMultimediaPrivate::StringResolver<MediaFormat::FileFormat,
                                               DescriptionRole>::toQString(fileFormat)
            .value_or(QStringLiteral("Unknown File Format"));
}

QString MediaFormat::audioCodecDescription(MediaFormat::AudioCodec codec)
{
    return QtMultimediaPrivate::StringResolver<MediaFormat::AudioCodec,
                                               DescriptionRole>::toQString(codec)
            .value_or(QStringLiteral("Unknown Audio Codec"));
}

QString MediaFormat::videoCodecDescription(MediaFormat::VideoCodec codec)
{
    return QtMultimediaPrivate::StringResolver<MediaFormat::VideoCodec,
                                               DescriptionRole>::toQString(codec)
            .value_or(QStringLiteral("Unknown Video Codec"));
}

bool MediaFormat::operator==(const MediaFormat &other) const
{
    Q_ASSERT(!d);
    return fmt == other.fmt &&
            audio == other.audio &&
           video == other.video;
}

void MediaFormat::resolveForEncoding(ResolveFlags flags)
{
    const bool requiresVideo = (flags & ResolveFlags::RequiresVideo) != 0;

    if (!requiresVideo)
        video = VideoCodec::Unspecified;

    MediaFormat nullFormat;
    auto supportedFormats = nullFormat.supportedFileFormats(MediaFormat::Encode);
    auto supportedAudioCodecs = nullFormat.supportedAudioCodecs(MediaFormat::Encode);
    auto supportedVideoCodecs = nullFormat.supportedVideoCodecs(MediaFormat::Encode);

    auto bestSupportedFileFormat = [&](MediaFormat::AudioCodec audio = MediaFormat::AudioCodec::Unspecified,
                                       MediaFormat::VideoCodec video = MediaFormat::VideoCodec::Unspecified)
    {
        MediaFormat f;
        f.setAudioCodec(audio);
        f.setVideoCodec(video);
        auto supportedFormats = f.supportedFileFormats(MediaFormat::Encode);
        auto *list = (flags == NoFlags) ? audioFormatPriorityList : videoFormatPriorityList;
        while (*list != MediaFormat::UnspecifiedFormat) {
            if (supportedFormats.contains(*list))
                break;
            ++list;
        }
        return *list;
    };

    if (requiresVideo && this->supportedVideoCodecs(MediaFormat::Encode).empty())
        fmt = MediaFormat::UnspecifiedFormat;

    if (!supportedFormats.contains(fmt))
        fmt = MediaFormat::UnspecifiedFormat;
    if (!supportedAudioCodecs.contains(audio))
        audio = MediaFormat::AudioCodec::Unspecified;
    if (!requiresVideo || !supportedVideoCodecs.contains(video))
        video = MediaFormat::VideoCodec::Unspecified;

    if (requiresVideo) {

        if (fmt == MediaFormat::UnspecifiedFormat)
            fmt = bestSupportedFileFormat(audio, video);

        if (fmt == MediaFormat::UnspecifiedFormat)
            fmt = bestSupportedFileFormat(MediaFormat::AudioCodec::Unspecified, video);
    }

    if (fmt == MediaFormat::UnspecifiedFormat)
        fmt = bestSupportedFileFormat(audio);

    if (fmt == MediaFormat::UnspecifiedFormat)
        fmt = bestSupportedFileFormat();

    if (fmt == MediaFormat::UnspecifiedFormat) {
        *this = {};
        return;
    }

    if (requiresVideo) {

        auto a = audio;
        audio = MediaFormat::AudioCodec::Unspecified;
        auto videoCodecs = this->supportedVideoCodecs(MediaFormat::Encode);
        if (!videoCodecs.contains(video)) {

            auto *list = videoPriorityList;
            while (*list != MediaFormat::VideoCodec::Unspecified) {
                if (videoCodecs.contains(*list))
                    break;
                ++list;
            }
            video = *list;
        }
        audio = a;
    } else {
        video = MediaFormat::VideoCodec::Unspecified;
    }

    auto audioCodecs = this->supportedAudioCodecs(MediaFormat::Encode);
    if (!audioCodecs.contains(audio)) {
        auto *list = audioPriorityList;
        while (*list != MediaFormat::AudioCodec::Unspecified) {
            if (audioCodecs.contains(*list))
                break;
            ++list;
        }
        audio = *list;
    }
}

#include "moc_MediaFormat.cpp"
