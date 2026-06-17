#include "FFmpegMediaMetadata_p.h"
#include <QDebug>
#include <QtCore/qdatetime.h>
#include <qstringlist.h>
#include <qurl.h>
#include <qlocale.h>

import qzLog;

QT_BEGIN_NAMESPACE

namespace ffmpeg {
static qz::Log::LogCategory qLcMetaData("qz.multimedia.ffmpeg.metadata");

namespace  {

struct ffmpegTagToMetaDataKey
{
    const char *tag;
    ::MediaMetaData::Key key;
};

constexpr ffmpegTagToMetaDataKey ffmpegTagToMetaDataKey[] = {
    { "title", ::MediaMetaData::Title },
    { "comment", ::MediaMetaData::Comment },
    { "description", ::MediaMetaData::Description },
    { "genre", ::MediaMetaData::Genre },
    { "date", ::MediaMetaData::Date },
    { "year", ::MediaMetaData::Date },
    { "creation_time", ::MediaMetaData::Date },

    { "language", ::MediaMetaData::Language },

    { "copyright", ::MediaMetaData::Copyright },

    { "album", ::MediaMetaData::AlbumTitle },
    { "album_artist", ::MediaMetaData::AlbumArtist },
    { "artist", ::MediaMetaData::ContributingArtist },
    { "track", ::MediaMetaData::TrackNumber },

    { "performer", ::MediaMetaData::LeadPerformer },

    { nullptr, ::MediaMetaData::Title }
};

}

static ::MediaMetaData::Key tagToKey(const char *tag)
{
    const auto *map = ffmpegTagToMetaDataKey;
    while (map->tag) {
        if (!strcmp(map->tag, tag))
            return map->key;
        ++map;
    }
    return ::MediaMetaData::Key(-1);
}

static const char *keyToTag(::MediaMetaData::Key key)
{
    const auto *map = ffmpegTagToMetaDataKey;
    while (map->tag) {
        if (map->key == key)
            return map->tag;
        ++map;
    }
    return nullptr;
}

void MetaData::addEntry(::MediaMetaData &metaData, AVDictionaryEntry *entry)
{
    qz::Log::cat_debug(qLcMetaData, "   checking:{} {}", entry->key, entry->value);
    QByteArray tag(entry->key);
    ::MediaMetaData::Key key = tagToKey(tag.toLower());
    if (key == ::MediaMetaData::Key(-1))
        return;
    qz::Log::cat_debug(qLcMetaData, "       adding {}", static_cast<int>(key));

    auto *map = &metaData;

    int metaTypeId = keyType(key).id();
    switch (metaTypeId) {
    case qMetaTypeId<QString>():
        map->insert(key, QString::fromUtf8(static_cast<const char*>(entry->value)));
        return;
    case qMetaTypeId<QStringList>():
        map->insert(key, QString::fromUtf8(static_cast<const char*>(entry->value)).split(QLatin1Char(',')));
        return;
    case qMetaTypeId<QDateTime>(): {
        QDateTime date;
        if (!qstrcmp(entry->key, "year")) {
            if (map->keys().contains(::MediaMetaData::Date))
                return;
            date = QDateTime(QDate(QByteArray(entry->value).toInt(), 1, 1), QTime(0, 0, 0));
        } else {
            date = QDateTime::fromString(QString::fromUtf8(static_cast<const char*>(entry->value)), Qt::ISODate);
        }
        map->insert(key, date);
        return;
    }
    case qMetaTypeId<QUrl>():
        map->insert(key, QUrl::fromEncoded(QByteArray(static_cast<const char*>(entry->value))));
        return;
    case qMetaTypeId<qint64>():
        map->insert(key, (qint64)QByteArray(entry->value).toLongLong());
        return;
    case qMetaTypeId<int>():
        map->insert(key, QByteArray(entry->value).toInt());
        return;
    case qMetaTypeId<qreal>():
        map->insert(key, (qreal)QByteArray(entry->value).toDouble());
        return;
    default:
        break;
    }
    if (metaTypeId == qMetaTypeId<QLocale::Language>()) {
        map->insert(key, QVariant::fromValue(QLocale::codeToLanguage(QString::fromUtf8(static_cast<const char*>(entry->value)), QLocale::ISO639Part2)));
    }
}

::MediaMetaData MetaData::fromAVMetaData(const AVDictionary *tags)
{
    ::MediaMetaData metaData;
    AVDictionaryEntry *entry = nullptr;
    while ((entry = av_dict_get(tags, "", entry, AV_DICT_IGNORE_SUFFIX)))
        addEntry(metaData, entry);

    return metaData;
}

QByteArray MetaData::value(const ::MediaMetaData &metaData, ::MediaMetaData::Key key)
{
    const int metaTypeId = keyType(key).id();
    const QVariant val = metaData.value(key);
    switch (metaTypeId) {
    case qMetaTypeId<QString>():
        return val.toString().toUtf8();
    case qMetaTypeId<QStringList>():
        return val.toStringList().join(u",").toUtf8();
    case qMetaTypeId<QDateTime>():
        return val.toDateTime().toString(Qt::ISODate).toUtf8();
    case qMetaTypeId<QUrl>():
        return val.toUrl().toEncoded();
    case qMetaTypeId<qint64>():
    case qMetaTypeId<int>():
        return QByteArray::number(val.toLongLong());
    case qMetaTypeId<qreal>():
        return QByteArray::number(val.toDouble());
    default:
        break;
    }
    if (metaTypeId == qMetaTypeId<QLocale::Language>())
        return QLocale::languageToCode(val.value<QLocale::Language>(), QLocale::ISO639Part2).toUtf8();
    return {};
}

AVDictionary *MetaData::toAVMetaData(const ::MediaMetaData &metaData)
{
    AVDictionary *dict = nullptr;
    for (const auto &&[k, v] : metaData.asKeyValueRange()) {
        const char *key = keyToTag(k);
        if (!key)
            continue;
        QByteArray val = value(metaData, k);
        if (val.isEmpty())
            continue;
        av_dict_set(&dict, key, val.constData(), 0);
    }
    return dict;
}
}
QT_END_NAMESPACE
