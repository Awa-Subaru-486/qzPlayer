// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_MEDIASTORAGELOCATION_P_H
#define QT_MEDIASTORAGELOCATION_P_H

#include <MultimediaGlobal.h>
#include <QStandardPaths>
#include <QDir>
#include <private/qglobal_p.h>

// 媒体存储位置：获取默认存储目录和生成文件名
namespace MediaStorageLocation
{
// 获取默认目录
QZ_MULTIMEDIA_EXPORT QDir defaultDirectory(QStandardPaths::StandardLocation type);
// 生成文件名
QZ_MULTIMEDIA_EXPORT QString generateFileName(const QString &requestedName,
                                             QStandardPaths::StandardLocation type,
                                             const QString &extension);
};

#endif
