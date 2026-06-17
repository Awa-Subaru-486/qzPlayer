// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.


#ifndef QT_AUDIO_AUTORESETEVENTLINUX_P_H
#define QT_AUDIO_AUTORESETEVENTLINUX_P_H
#include <QtCore/qsocketnotifier.h>
#include <qzMultimedia/qtmultimediaexports.h>

namespace QtPrivate {

class QZ_MULTIMEDIA_EXPORT AutoResetEventEventFD final : public QObject
{
    Q_OBJECT

public:
    explicit AutoResetEventEventFD(QObject *parent = nullptr);
    ~AutoResetEventEventFD();
    Q_DISABLE_COPY_MOVE(AutoResetEventEventFD)

    bool isValid() const { return m_fd != -1; }
    void set();

    template <typename... Args>
    QMetaObject::Connection callOnActivated(Args &&...args)
    {
        return connect(this, &AutoResetEventEventFD::activated, std::forward<Args>(args)...);
    }

    template <typename Functor>
    QMetaObject::Connection callOnActivated(Functor &&functor)
    {
        return connect(this, &AutoResetEventEventFD::activated, this,
                       std::forward<Functor>(functor));
    }

Q_SIGNALS:
    void activated();

private:
    QSocketNotifier m_notifier;
    int m_fd = -1;
};

}

#endif
