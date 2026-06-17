// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include <QtQml/qqmlextensionplugin.h>

#include "qzmultimediaquickexports.h"

import qml_register_types_qzMultimedia;

QT_BEGIN_NAMESPACE

class qzMultimediaQuickModule : public QQmlEngineExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlEngineExtensionInterface_iid)

public:
    explicit qzMultimediaQuickModule(QObject *parent = nullptr)
        : QQmlEngineExtensionPlugin(parent)
    {
        volatile auto registration = qz::qml_register_types_qz_multimedia;
        Q_UNUSED(registration);
    }
};

QT_END_NAMESPACE

#include "MultimediaPlugin.moc"
