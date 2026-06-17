// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "MediaMetadata.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qobject.h>
#include <QtCore/qsize.h>
#include <QtCore/qurl.h>
#include <QtCore/qvariant.h>
#include <QtGui/qimage.h>
#include <qzMultimedia/MediaFormat.h>

QMetaType MediaMetaData::keyType(Key key)
{
    switch (key) {
    case Title:
    case Comment:
    case Description:
    case Publisher:
    case Copyright:
    case MediaType:
    case AlbumTitle:
    case AlbumArtist:
        return QMetaType::fromType<QString>();
    case Genre:
    case Author:
    case ContributingArtist:
    case Composer:
    case LeadPerformer:
        return QMetaType::fromType<QStringList>();

    case Date:
        return QMetaType::fromType<QDateTime>();

    case Language:
        return QMetaType::fromType<QLocale::Language>();
    case Url:
        return QMetaType::fromType<QUrl>();

    case Duration:
        return QMetaType::fromType<qint64>();
    case FileFormat:
        return QMetaType::fromType<MediaFormat::FileFormat>();

    case AudioBitRate:
    case VideoBitRate:
    case TrackNumber:
    case Orientation:
        return QMetaType::fromType<int>();
    case AudioCodec:
        return QMetaType::fromType<MediaFormat::AudioCodec>();
    case VideoCodec:
        return QMetaType::fromType<MediaFormat::VideoCodec>();
    case VideoFrameRate:
        return QMetaType::fromType<qreal>();

    case ThumbnailImage:
    case CoverArtImage:
        return QMetaType::fromType<QImage>();

    case Resolution:
        return QMetaType::fromType<QSize>();

    case HasHdrContent:
        return QMetaType::fromType<bool>();

    default:
        return QMetaType::fromType<void>();
    }
}

QString MediaMetaData::stringValue(MediaMetaData::Key key) const
{
    QVariant value = data.value(key);
    if (value.isNull())
        return QString();

    switch (key) {

    case Title:
    case Author:
    case Comment:
    case Description:
    case Genre:
    case Publisher:
    case Copyright:
    case Date:
    case Url:
    case MediaType:
    case AudioBitRate:
    case VideoBitRate:
    case VideoFrameRate:
    case AlbumTitle:
    case AlbumArtist:
    case ContributingArtist:
    case TrackNumber:
    case Composer:
    case Orientation:
    case LeadPerformer:
    case HasHdrContent:
        return value.toString();
    case Language: {
        auto l = value.value<QLocale::Language>();
        return QLocale::languageToString(l);
    }
    case Duration: {
        QTime time = QTime::fromMSecsSinceStartOfDay(value.toInt());
        return time.toString();
    }
    case FileFormat:
        return MediaFormat::fileFormatName(value.value<MediaFormat::FileFormat>());
    case AudioCodec:
        return MediaFormat::audioCodecName(value.value<MediaFormat::AudioCodec>());
    case VideoCodec:
        return MediaFormat::videoCodecName(value.value<MediaFormat::VideoCodec>());
    case Resolution: {
        QSize size = value.toSize();
        return QStringLiteral("%1 x %2").arg(size.width()).arg(size.height());
    }
    case ThumbnailImage:
    case CoverArtImage:
        break;
    }
    return QString();
}

QString MediaMetaData::metaDataKeyToString(MediaMetaData::Key key)
{
    switch (key) {
        case MediaMetaData::Title:
            return (QCoreApplication::translate("MediaMetaData", "Title"));
        case MediaMetaData::Author:
            return (QCoreApplication::translate("MediaMetaData", "Author"));
        case MediaMetaData::Comment:
            return (QCoreApplication::translate("MediaMetaData", "Comment"));
        case MediaMetaData::Description:
            return (QCoreApplication::translate("MediaMetaData", "Description"));
        case MediaMetaData::Genre:
            return (QCoreApplication::translate("MediaMetaData", "Genre"));
        case MediaMetaData::Date:
            return (QCoreApplication::translate("MediaMetaData", "Date"));
        case MediaMetaData::Language:
            return (QCoreApplication::translate("MediaMetaData", "Language"));
        case MediaMetaData::Publisher:
            return (QCoreApplication::translate("MediaMetaData", "Publisher"));
        case MediaMetaData::Copyright:
            return (QCoreApplication::translate("MediaMetaData", "Copyright"));
        case MediaMetaData::Url:
            return (QCoreApplication::translate("MediaMetaData", "Url"));
        case MediaMetaData::Duration:
            return (QCoreApplication::translate("MediaMetaData", "Duration"));
        case MediaMetaData::MediaType:
            return (QCoreApplication::translate("MediaMetaData", "Media type"));
        case MediaMetaData::FileFormat:
            return (QCoreApplication::translate("MediaMetaData", "Container Format"));
        case MediaMetaData::AudioBitRate:
            return (QCoreApplication::translate("MediaMetaData", "Audio bit rate"));
        case MediaMetaData::AudioCodec:
            return (QCoreApplication::translate("MediaMetaData", "Audio codec"));
        case MediaMetaData::VideoBitRate:
            return (QCoreApplication::translate("MediaMetaData", "Video bit rate"));
        case MediaMetaData::VideoCodec:
            return (QCoreApplication::translate("MediaMetaData", "Video codec"));
        case MediaMetaData::VideoFrameRate:
            return (QCoreApplication::translate("MediaMetaData", "Video frame rate"));
        case MediaMetaData::AlbumTitle:
            return (QCoreApplication::translate("MediaMetaData", "Album title"));
        case MediaMetaData::AlbumArtist:
            return (QCoreApplication::translate("MediaMetaData", "Album artist"));
        case MediaMetaData::ContributingArtist:
            return (QCoreApplication::translate("MediaMetaData", "Contributing artist"));
        case MediaMetaData::TrackNumber:
            return (QCoreApplication::translate("MediaMetaData", "Track number"));
        case MediaMetaData::Composer:
            return (QCoreApplication::translate("MediaMetaData", "Composer"));
        case MediaMetaData::ThumbnailImage:
            return (QCoreApplication::translate("MediaMetaData", "Thumbnail image"));
        case MediaMetaData::CoverArtImage:
            return (QCoreApplication::translate("MediaMetaData", "Cover art image"));
        case MediaMetaData::Orientation:
            return (QCoreApplication::translate("MediaMetaData", "Orientation"));
        case MediaMetaData::Resolution:
            return (QCoreApplication::translate("MediaMetaData", "Resolution"));
        case MediaMetaData::LeadPerformer:
            return (QCoreApplication::translate("MediaMetaData", "Lead performer"));
        case MediaMetaData::HasHdrContent:
            return (QCoreApplication::translate("MediaMetaData", "Has HDR content"));
    }
    return QString();
}

QDebug operator<<(QDebug dbg, const MediaMetaData &metaData)
{
    QDebugStateSaver sv(dbg);
    dbg.nospace();

    dbg << "MediaMetaData{";
    auto range = metaData.asKeyValueRange();
    auto begin = std::begin(range);

    for (auto it = begin; it != std::end(range); ++it) {
        if (it != begin)
            dbg << ", ";
        dbg << it->first << ": " << it->second;
    }

    dbg << "}";
    return dbg;
}

#include "moc_MediaMetadata.cpp"
