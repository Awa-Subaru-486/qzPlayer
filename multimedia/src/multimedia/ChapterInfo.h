// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_MULTIMEDIA_CHAPTERINFO_H
#define QT_MULTIMEDIA_CHAPTERINFO_H

#include <QtCore/qobject.h>
#include <QtCore/qmetatype.h>
#include <qzMultimedia/MultimediaGlobal.h>

// 章节信息：存储章节的起止时间和标题，可被 QML 访问
class QZ_MULTIMEDIA_EXPORT ChapterInfo
{
    Q_GADGET
    Q_PROPERTY(qint64 startTime READ startTime WRITE setStartTime)
    Q_PROPERTY(qint64 endTime READ endTime WRITE setEndTime)
    Q_PROPERTY(QString title READ title WRITE setTitle)
public:
    ChapterInfo() = default;
    ChapterInfo(qint64 start, qint64 end, const QString &t)
        : m_startTime(start), m_endTime(end), m_title(t)
    {}

    qint64 startTime() const { return m_startTime; }
    void setStartTime(const qint64 ms) { m_startTime = ms; }

    qint64 endTime() const { return m_endTime; }
    void setEndTime(const qint64 ms) { m_endTime = ms; }

    QString title() const { return m_title; }
    void setTitle(const QString &t) { m_title = t; }

private:
    qint64 m_startTime = 0;
    qint64 m_endTime = 0;
    QString m_title;
};

Q_DECLARE_METATYPE(ChapterInfo)

#endif
