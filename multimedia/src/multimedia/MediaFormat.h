// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_MEDIAFORMAT_H
#define QT_MEDIAFORMAT_H
#include <QtCore/qobjectdefs.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qtmetamacros.h>
#include <qzMultimedia/MultimediaGlobal.h>

class QMimeType;
class MediaFormat;
class MediaFormatPrivate;

QT_DECLARE_QESDP_SPECIALIZATION_DTOR_WITH_EXPORT(MediaFormatPrivate, QZ_MULTIMEDIA_EXPORT)

// 媒体格式：描述容器格式、音频编解码器、视频编解码器
class QZ_MULTIMEDIA_EXPORT MediaFormat
{
    Q_GADGET
    Q_PROPERTY(FileFormat fileFormat READ fileFormat WRITE setFileFormat)
    Q_PROPERTY(AudioCodec audioCodec READ audioCodec WRITE setAudioCodec)
    Q_PROPERTY(VideoCodec videoCodec READ videoCodec WRITE setVideoCodec)
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")
public:
    // 文件格式枚举
    enum FileFormat {
        UnspecifiedFormat = -1,

        WMV,
        AVI,
        Matroska,
        MPEG4,
        Ogg,
        QuickTime,
        WebM,

        Mpeg4Audio,
        AAC,
        WMA,
        MP3,
        FLAC,
        Wave,
        LastFileFormat = Wave
    };
    Q_ENUM(FileFormat)

    // 音频编解码器枚举
    enum class AudioCodec {
        Unspecified = -1,
        MP3,
        AAC,
        AC3,
        EAC3,
        FLAC,
        DolbyTrueHD,
        Opus,
        Vorbis,
        Wave,
        WMA,
        ALAC,
        LastAudioCodec = ALAC
    };
    Q_ENUM(AudioCodec)

    // 视频编解码器枚举
    enum class VideoCodec {
        Unspecified = -1,
        MPEG1,
        MPEG2,
        MPEG4,
        H264,
        H265,
        VP8,
        VP9,
        AV1,
        Theora,
        WMV,
        MotionJPEG,
        LastVideoCodec = MotionJPEG
    };
    Q_ENUM(VideoCodec)

    // 转换模式：编码或解码
    enum ConversionMode {
        Encode,
        Decode
    };
    Q_ENUM(ConversionMode)

    // 解析标志
    enum ResolveFlags
    {
        NoFlags,
        RequiresVideo
    };

    MediaFormat(FileFormat format = UnspecifiedFormat);
    ~MediaFormat();
    MediaFormat(const MediaFormat &other) noexcept;
    MediaFormat &operator=(const MediaFormat &other) noexcept;

    MediaFormat(MediaFormat &&other) noexcept = default;
    QT_MOVE_ASSIGNMENT_OPERATOR_IMPL_VIA_PURE_SWAP(MediaFormat)
    void swap(MediaFormat &other) noexcept
    {
        std::swap(fmt, other.fmt);
        std::swap(audio, other.audio);
        std::swap(video, other.video);
        d.swap(other.d);
    }

    // 文件格式
    FileFormat fileFormat() const { return fmt; }
    void setFileFormat(FileFormat f) { fmt = f; }

    // 视频编解码器
    void setVideoCodec(VideoCodec codec) { video = codec; }
    VideoCodec videoCodec() const { return video; }

    // 音频编解码器
    void setAudioCodec(AudioCodec codec) { audio = codec; }
    AudioCodec audioCodec() const { return audio; }

    // 检查格式是否支持
    Q_INVOKABLE bool isSupported(ConversionMode mode) const;

#if QT_CONFIG(mimetype)
    // 获取 MIME 类型
    QMimeType mimeType() const;
#endif

    // 获取支持的格式列表
    Q_INVOKABLE QList<FileFormat> supportedFileFormats(ConversionMode m);
    Q_INVOKABLE QList<VideoCodec> supportedVideoCodecs(ConversionMode m);
    Q_INVOKABLE QList<AudioCodec> supportedAudioCodecs(ConversionMode m);

    // 格式名称和描述
    Q_INVOKABLE static QString fileFormatName(FileFormat fileFormat);
    Q_INVOKABLE static QString audioCodecName(AudioCodec codec);
    Q_INVOKABLE static QString videoCodecName(VideoCodec codec);

    Q_INVOKABLE static QString fileFormatDescription(MediaFormat::FileFormat fileFormat);
    Q_INVOKABLE static QString audioCodecDescription(MediaFormat::AudioCodec codec);
    Q_INVOKABLE static QString videoCodecDescription(MediaFormat::VideoCodec codec);

    bool operator==(const MediaFormat &other) const;
    bool operator!=(const MediaFormat &other) const
    { return !operator==(other); }

    // 为编码解析格式
    void resolveForEncoding(ResolveFlags flags);

protected:
    friend class MediaFormatPrivate;
    FileFormat fmt;
    AudioCodec audio = AudioCodec::Unspecified;
    VideoCodec video = VideoCodec::Unspecified;
    QExplicitlySharedDataPointer<MediaFormatPrivate> d;
};

#endif
