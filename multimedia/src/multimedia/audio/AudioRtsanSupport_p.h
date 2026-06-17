// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_AUDIORTSANSUPPORT_P_H
#define QT_AUDIO_AUDIORTSANSUPPORT_P_H

#include <QtCore/qtconfigmacros.h>
#include <QtCore/qtclasshelpermacros.h>

#if defined(__has_feature) && __has_feature(realtime_sanitizer)
#  include <sanitizer/rtsan_interface.h>
#endif

#if defined(__has_cpp_attribute) && __has_cpp_attribute(clang::nonblocking)
#  define QT_MM_NONBLOCKING [[clang::nonblocking]]
#else
#  define QT_MM_NONBLOCKING
#endif

namespace QtPrivate {

#if defined(__has_feature) && __has_feature(realtime_sanitizer)
using ScopedRTSanDisabler = __rtsan::ScopedDisabler;
#else
struct ScopedRTSanDisabler
{
    ScopedRTSanDisabler()
    {

        (void)this;
    }
    ~ScopedRTSanDisabler() = default;
    Q_DISABLE_COPY_MOVE(ScopedRTSanDisabler)
};
#endif

template <typename Functor>
auto withRTSanDisabled(const Functor &f)
{
#if defined(__has_feature) && __has_feature(realtime_sanitizer)
    __rtsan::ScopedDisabler disabler;
#endif
    return f();
}

}

#endif
