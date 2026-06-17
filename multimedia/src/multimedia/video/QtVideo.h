// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_VIDEO_QTVIDEO_H
#define QT_VIDEO_QTVIDEO_H
#include <qzMultimedia/qtmultimediaexports.h>
#include <QtCore/qobjectdefs.h>
#include <QtCore/qtconfigmacros.h>
#include <QtCore/qtypes.h>

// 视频命名空间：定义视频旋转角度等枚举类型
namespace QtVideo
{
Q_NAMESPACE_EXPORT(QZ_MULTIMEDIA_EXPORT)

// 视频旋转角度
enum class Rotation {
    None = 0,
    Clockwise90 = 90,
    Clockwise180 = 180,
    Clockwise270 = 270,
};
Q_ENUM_NS(Rotation)

}

#endif
