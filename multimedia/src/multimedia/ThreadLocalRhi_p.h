// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_THREADLOCALRHI_P_H
#define QT_THREADLOCALRHI_P_H

#include <qzMultimedia/qtmultimediaexports.h>

#include <QtGui/rhi/qrhi.h>

// 线程本地 RHI：确保当前线程有可用的 RHI 实例
QZ_MULTIMEDIA_EXPORT QRhi *qEnsureThreadLocalRhi(QRhi *referenceRhi = nullptr);

// 设置首选的线程本地 RHI 后端
QZ_MULTIMEDIA_EXPORT void qSetPreferredThreadLocalRhiBackend(QRhi::Implementation backend);

#endif
