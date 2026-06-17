// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_MULTIMEDIAASSUME_P_H
#define QT_MULTIMEDIAASSUME_P_H

#if defined(__has_cpp_attribute) && __has_cpp_attribute(assume)
#  define QT_MM_ASSUME(assumption) \
      Q_ASSERT(assumption);        \
      [[assume(assumption)]];      \
      static_assert(true, "force semicolon")
#else
#  define QT_MM_ASSUME(assumption) Q_ASSERT(assumption)
#endif

#endif
