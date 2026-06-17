// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QZMULTIMEDIAQUICKEXPORTS_H
#define QZMULTIMEDIAQUICKEXPORTS_H

#include <QtCore/qcompilerdetection.h>
#include <QtCore/qtconfigmacros.h>

#if defined(QT_STATICPLUGIN)
#  define QZ_MULTIMEDIAQUICK_EXPORT
#elif defined(QT_SHARED) || !defined(QT_STATIC)
#  if defined(QT_BUILD_MULTIMEDIAQUICK_LIB)
#    define QZ_MULTIMEDIAQUICK_EXPORT Q_DECL_EXPORT
#  else
#    define QZ_MULTIMEDIAQUICK_EXPORT Q_DECL_IMPORT
#  endif
#else
#  define QZ_MULTIMEDIAQUICK_EXPORT
#endif

#endif
