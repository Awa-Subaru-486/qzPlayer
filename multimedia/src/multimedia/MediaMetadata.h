// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_MEDIAMETADATA_H
#define QT_MEDIAMETADATA_H
#if 0
#pragma qt_class(MediaMetaData)
#endif

#include <QtCore/qvariant.h>
#include <QtCore/qstring.h>
#include <QtCore/qhash.h>
#include <qzMultimedia/MultimediaGlobal.h>

// 媒体元数据：存储标题、作者、时长、封面等信息
class QZ_MULTIMEDIA_EXPORT MediaMetaData
{
    Q_GADGET
public:
    // 元数据键枚举
    enum Key {
        Title,
        Author,
        Comment,
        Description,
        Genre,
        Date,

        Language,
        Publisher,
        Copyright,
        Url,

        Duration,
        MediaType,
        FileFormat,

        AudioBitRate,
        AudioCodec,
        VideoBitRate,
        VideoCodec,
        VideoFrameRate,

        AlbumTitle,
        AlbumArtist,
        ContributingArtist,
        TrackNumber,
        Composer,
        LeadPerformer,

        ThumbnailImage,
        CoverArtImage,

        Orientation,
        Resolution,

        HasHdrContent,
    };
    Q_ENUM(Key)

    static constexpr int NumMetaData = HasHdrContent + 1;

    // 元数据访问
    Q_INVOKABLE QVariant value(Key k) const { return data.value(k); }
    Q_INVOKABLE void insert(Key k, const QVariant &value) { data.insert(k, value); }
    Q_INVOKABLE void remove(Key k) { data.remove(k); }
    Q_INVOKABLE QList<Key> keys() const { return data.keys(); }

    QVariant &operator[](Key k) { return data[k]; }
    Q_INVOKABLE void clear() { data.clear(); }

    Q_INVOKABLE bool isEmpty() const { return data.isEmpty(); }
    Q_INVOKABLE QString stringValue(Key k) const;

    // 元数据键名称
    Q_INVOKABLE static QString metaDataKeyToString(Key k);

    QT_POST_CXX17_API_IN_EXPORTED_CLASS
    auto asKeyValueRange() const { return data.asKeyValueRange(); }

protected:
    QZ_MULTIMEDIA_EXPORT friend QDebug operator<<(QDebug, const MediaMetaData &);

    friend bool operator==(const MediaMetaData &a, const MediaMetaData &b)
    { return a.data == b.data; }
    friend bool operator!=(const MediaMetaData &a, const MediaMetaData &b)
    { return a.data != b.data; }

    static QMetaType keyType(Key key);

    QHash<Key, QVariant> data;
};

Q_DECLARE_METATYPE(MediaMetaData)

#endif
