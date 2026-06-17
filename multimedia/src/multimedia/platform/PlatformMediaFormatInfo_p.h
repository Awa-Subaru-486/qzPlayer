// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_PLATFORM_PLATFORMMEDIAFORMATINFO_P_H
#define QT_PLATFORM_PLATFORMMEDIAFORMATINFO_P_H

#include <private/MultimediaGlobal_p.h>
#include <MediaFormat.h>
#include <QtCore/qlist.h>

// 平台媒体格式信息抽象接口：查询支持的编解码器和容器格式
class QZ_MULTIMEDIA_EXPORT PlatformMediaFormatInfo
{
public:
    PlatformMediaFormatInfo();
    virtual ~PlatformMediaFormatInfo();

    // 获取支持的文件格式
    QList<MediaFormat::FileFormat> supportedFileFormats(const MediaFormat &constraints, MediaFormat::ConversionMode m) const;
    // 获取支持的音频编解码器
    QList<MediaFormat::AudioCodec> supportedAudioCodecs(const MediaFormat &constraints, MediaFormat::ConversionMode m) const;
    // 获取支持的视频编解码器
    QList<MediaFormat::VideoCodec> supportedVideoCodecs(const MediaFormat &constraints, MediaFormat::ConversionMode m) const;

    // 检查格式是否支持
    bool isSupported(const MediaFormat &format, MediaFormat::ConversionMode m) const;

    struct CodecMap {
        MediaFormat::FileFormat format;
        QList<MediaFormat::AudioCodec> audio;
        QList<MediaFormat::VideoCodec> video;
    };
    QList<CodecMap> encoders;
    QList<CodecMap> decoders;
};

#endif
