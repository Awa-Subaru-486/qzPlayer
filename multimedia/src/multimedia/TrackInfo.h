// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_MULTIMEDIA_TRACKINFO_H
#define QT_MULTIMEDIA_TRACKINFO_H

#include <QtCore/qobject.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qabstractitemmodel.h>
#include <QtQml/qqml.h>
#include <qzMultimedia/MultimediaGlobal.h>
#include <qzMultimedia/MediaMetadata.h>

// 轨道信息：包含轨道元数据和激活状态，可被 QML 访问
// 同类型轨道中 active 互斥：激活一个会自动取消其他轨道的激活
class QZ_MULTIMEDIA_EXPORT TrackInfo
{
    Q_GADGET
    Q_PROPERTY(int index READ index CONSTANT)
    Q_PROPERTY(TrackType trackType READ trackType CONSTANT)
    Q_PROPERTY(MediaMetaData metaData READ metaData CONSTANT)
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString language READ language CONSTANT)
    Q_PROPERTY(bool active READ active)
public:
    enum TrackType { AudioTrack, VideoTrack, SubtitleTrack };
    Q_ENUM(TrackType)

    TrackInfo() = default;
    TrackInfo(int idx, TrackType type, const MediaMetaData &meta, bool isActive)
        : m_index(idx), m_trackType(type), m_metaData(meta), m_active(isActive)
    {}

    int index() const { return m_index; }
    TrackType trackType() const { return m_trackType; }
    MediaMetaData metaData() const { return m_metaData; }
    QString title() const { return m_metaData.stringValue(MediaMetaData::Title); }
    QString language() const { return m_metaData.stringValue(MediaMetaData::Language); }
    bool active() const { return m_active; }

    void setActive(bool active) { m_active = active; }

private:
    int m_index = -1;
    TrackType m_trackType = AudioTrack;
    MediaMetaData m_metaData;
    bool m_active = false;
};

Q_DECLARE_METATYPE(TrackInfo)

// 轨道信息模型：QAbstractListModel 子类，供 QML Repeater/ListView 使用
class QZ_MULTIMEDIA_EXPORT TrackInfoModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Roles {
        IndexRole = Qt::UserRole + 1,
        TrackTypeRole,
        TitleRole,
        LanguageRole,
        ActiveRole,
        TrackInfoRole,
    };

    explicit TrackInfoModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : m_tracks.size();
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_tracks.size())
            return {};
        const auto &track = m_tracks.at(index.row());
        switch (role) {
        case IndexRole:    return track.index();
        case TrackTypeRole:return track.trackType();
        case TitleRole:    return track.title();
        case LanguageRole: return track.language();
        case ActiveRole:   return track.active();
        case TrackInfoRole:return QVariant::fromValue(track);
        default:           return {};
        }
    }

    QHash<int, QByteArray> roleNames() const override
    {
        return {
            {IndexRole,     "index"},
            {TrackTypeRole, "trackType"},
            {TitleRole,     "title"},
            {LanguageRole,  "language"},
            {ActiveRole,    "active"},
            {TrackInfoRole, "trackInfo"},
        };
    }

    int count() const { return m_tracks.size(); }

    void setTracks(const QList<TrackInfo> &tracks)
    {
        beginResetModel();
        m_tracks = tracks;
        endResetModel();
        emit countChanged();
    }

    const QList<TrackInfo> &tracks() const { return m_tracks; }

signals:
    void countChanged();

private:
    QList<TrackInfo> m_tracks;
};

#endif
