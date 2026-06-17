// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef PREVIEWFRAMEPROVIDER_P_H
#define PREVIEWFRAMEPROVIDER_P_H

#include <qzMultimedia/private/PlatformMediaIntegration_p.h>
#include <qzMultimedia/private/PlatformPreviewFrameProvider_p.h>

QT_BEGIN_NAMESPACE

// 预览帧提供者私有实现
class PreviewFrameProviderPrivate
{
public:
    static PlatformPreviewFrameProvider *createBackend();
};

QT_END_NAMESPACE

#endif // PREVIEWFRAMEPROVIDER_P_H
