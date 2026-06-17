// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "AutoResetEventLinux_p.h"

#include <QtCore/private/qcore_unix_p.h>
#include <QtCore/qdebug.h>
#include <qzMultimedia/private/AudioRtsanSupport_p.h>

#include <sys/eventfd.h>
#include <cstdint>

namespace QtPrivate {

AutoResetEventEventFD::AutoResetEventEventFD(QObject *parent)
        : QObject{
              parent,
          },
          m_notifier{
              QSocketNotifier::Type::Read,
          }
{
    m_fd = eventfd(0, EFD_NONBLOCK);
    if (m_fd == -1) {
        qCritical() << "eventfd failed:" << qt_error_string(errno);
        return;
    }

    connect(&m_notifier, &QSocketNotifier::activated, this, [this] {
        uint64_t payload;

        qt_safe_read(m_fd, &payload, sizeof(payload));

        emit activated();
    });
    m_notifier.setSocket(m_fd);
    m_notifier.setEnabled(true);
}

AutoResetEventEventFD::~AutoResetEventEventFD()
{
    if (m_fd != -1)
        qt_safe_close(m_fd);
}

void AutoResetEventEventFD::set()
{
    Q_ASSERT(isValid());

    constexpr uint64_t increment{ 1 };

    ScopedRTSanDisabler disabler;
    qint64 bytesWritten = qt_safe_write(m_fd, &increment, sizeof(increment));
    if (bytesWritten == -1)
        qCritical("QAutoResetEvent::set failed");
}

}

#include "moc_AutoResetEventLinux_p.cpp"
