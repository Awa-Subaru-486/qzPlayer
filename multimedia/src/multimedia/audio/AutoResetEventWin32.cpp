// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "AutoResetEventWin32_p.h"

#include <QtCore/qt_windows.h>
#include <QtCore/qdebug.h>

namespace QtPrivate {

AutoResetEventWin32::AutoResetEventWin32(QObject *parent)
    : QObject{
          parent,
      },
      m_handle{
          nullptr,
      }
{
    m_handle = ::CreateEventW(0,
                              false,
                              false,
                              nullptr);

    if (!m_handle) {
        qCritical() << "CreateEventW failed:" << qt_error_string(GetLastError());
        return;
    }

    connect(&m_notifier, &QWinEventNotifier::activated, this, &AutoResetEventWin32::activated);

    m_notifier.setHandle(m_handle);
    m_notifier.setEnabled(true);
}

AutoResetEventWin32::~AutoResetEventWin32()
{
    if (m_handle)
        ::CloseHandle(m_handle);
}

bool AutoResetEventWin32::isValid() const
{
    return m_handle;
}

void AutoResetEventWin32::set()
{
    Q_ASSERT(isValid());

    bool status = ::SetEvent(m_handle);
    if (!status)
        qCritical("QAutoResetEvent::set failed");
}

}

#include "moc_AutoResetEventWin32_p.cpp"
