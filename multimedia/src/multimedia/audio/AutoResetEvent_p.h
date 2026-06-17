// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.


#ifndef QT_AUDIO_AUTORESETEVENT_P_H
#define QT_AUDIO_AUTORESETEVENT_P_H
#include <QtCore/qglobal.h>

#ifdef Q_OS_LINUX
#  include "AutoResetEventLinux_p.h"

namespace QtPrivate {
using QAutoResetEvent = AutoResetEventEventFD;
}

#elif defined(Q_OS_WIN)
#  include "AutoResetEventWin32_p.h"

namespace QtPrivate {
using QAutoResetEvent = AutoResetEventWin32;
}

#else
#  include "AutoResetEventPipe_p.h"

namespace QtPrivate {
using QAutoResetEvent = AutoResetEventPipe;
}

#endif

#endif
