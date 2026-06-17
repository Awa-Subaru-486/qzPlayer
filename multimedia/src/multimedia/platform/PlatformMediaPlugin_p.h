// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.


#ifndef QT_PLATFORM_PLATFORMMEDIAPLUGIN_P_H
#define QT_PLATFORM_PLATFORMMEDIAPLUGIN_P_H
#include <qzMultimedia/MultimediaGlobal.h>
#include <QtCore/qplugin.h>
#include <QtCore/qfactoryinterface.h>
#include <QtCore/private/qglobal_p.h>

class PlatformMediaIntegration;

#define QPlatformMediaPlugin_iid "org.qt-project.Qt.PlatformMediaPlugin"

// 平台媒体插件接口：用于动态加载后端实现
class QZ_MULTIMEDIA_EXPORT PlatformMediaPlugin : public QObject
{
    Q_OBJECT
public:
    explicit PlatformMediaPlugin(QObject *parent = nullptr);
    ~PlatformMediaPlugin() override;

    // 创建平台集成实例
    virtual PlatformMediaIntegration *create(const QString &key) = 0;

};

#endif
