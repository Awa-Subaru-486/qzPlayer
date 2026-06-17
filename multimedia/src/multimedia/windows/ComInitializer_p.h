// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.


#ifndef QT_WINDOWS_COMINITIALIZER_P_H
#define QT_WINDOWS_COMINITIALIZER_P_H
#include <qzMultimedia/qtmultimediaexports.h>
#include <QtCore/qtconfigmacros.h>

// COM 初始化器：确保当前线程已初始化 COM
QZ_MULTIMEDIA_EXPORT void ensureComInitializedOnThisThread();

// COM 初始化辅助结构体
struct ComInitializer
{
    ComInitializer()
    {
        ensureComInitializedOnThisThread();
    }
};

#endif
