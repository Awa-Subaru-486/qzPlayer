// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "MediaStorageLocation_p.h"

#include <QStandardPaths>
#include <QUrl>

QDir MediaStorageLocation::defaultDirectory(QStandardPaths::StandardLocation type)
{
    QStringList dirCandidates;

#if defined(Q_OS_QNX) && QT_CONFIG(mmrenderer)
    dirCandidates << QLatin1String("shared/camera");
#endif

    dirCandidates << QStandardPaths::writableLocation(type);
    dirCandidates << QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    dirCandidates << QDir::homePath();
    dirCandidates << QDir::currentPath();
    dirCandidates << QDir::tempPath();

    for (const QString &path : std::as_const(dirCandidates)) {
        QDir dir(path);
        if (dir.exists() && QFileInfo(path).isWritable())
            return dir;
    }

    return QDir();
}

static QString generateFileName(const QDir &dir, const QString &prefix, const QString &extension)
{

    int lastMediaIndex = 0;
    const QStringView maybeDot = !extension.isEmpty() && !extension.startsWith(u'.') ? u"." : u"";
    const auto filesList =
            dir.entryList({ QStringView(u"%1*%2%3").arg(prefix, maybeDot, extension) });
    for (const QString &fileName : filesList) {
        const qsizetype mediaIndexSize =
                fileName.size() - prefix.size() - extension.size() - maybeDot.size();
        const int mediaIndex = QStringView{ fileName }.mid(prefix.size(), mediaIndexSize).toInt();
        lastMediaIndex = qMax(lastMediaIndex, mediaIndex);
    }

    const QString newMediaIndexStr = QStringLiteral("%1").arg(lastMediaIndex + 1, 4, 10, QLatin1Char(u'0'));
    const QString name = prefix + newMediaIndexStr + maybeDot + extension;

    return dir.absoluteFilePath(name);
}

QString MediaStorageLocation::generateFileName(const QString &requestedName,
                                                QStandardPaths::StandardLocation type,
                                                const QString &extension)
{
    using namespace Qt::StringLiterals;

    if (QUrl(requestedName).scheme() == "content"_L1)
        return requestedName;

    auto prefix = "clip_"_L1;
    switch (type) {
        case QStandardPaths::PicturesLocation: prefix = "image_"_L1; break;
        case QStandardPaths::MoviesLocation: prefix = "video_"_L1; break;
        case QStandardPaths::MusicLocation: prefix = "record_"_L1; break;
        default: break;
    }

    if (requestedName.isEmpty())
        return generateFileName(defaultDirectory(type), prefix, extension);

    QString path = requestedName;

    const QFileInfo fileInfo{ path };

    if (fileInfo.isRelative() && QUrl(path).isRelative())
        path = defaultDirectory(type).absoluteFilePath(path);

    if (fileInfo.isDir())
        return generateFileName(QDir(path), prefix, extension);

    if (fileInfo.suffix().isEmpty() && !extension.isEmpty()) {

        if (!path.endsWith(u'.'))
            path.append(u'.');
        path.append(extension);
    }
    return path;
}

