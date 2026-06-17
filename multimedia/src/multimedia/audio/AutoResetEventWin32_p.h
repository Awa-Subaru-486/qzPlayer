// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.


#ifndef QT_AUDIO_AUTORESETEVENTWIN32_P_H
#define QT_AUDIO_AUTORESETEVENTWIN32_P_H
#include <QtCore/qwineventnotifier.h>
#include <qzMultimedia/qtmultimediaexports.h>

namespace QtPrivate {

class QZ_MULTIMEDIA_EXPORT AutoResetEventWin32 final : public QObject
{
    Q_OBJECT

public:
    explicit AutoResetEventWin32(QObject *parent = nullptr);
    ~AutoResetEventWin32();
    Q_DISABLE_COPY_MOVE(AutoResetEventWin32)

    bool isValid() const;
    void set();

    template <typename... Args>
    QMetaObject::Connection callOnActivated(Args &&...args)
    {
        return connect(this, &AutoResetEventWin32::activated, std::forward<Args>(args)...);
    }

    template <typename Functor>
    QMetaObject::Connection callOnActivated(Functor &&functor)
    {
        return connect(this, &AutoResetEventWin32::activated, this,
                       std::forward<Functor>(functor));
    }

Q_SIGNALS:
    void activated();

private:
    QWinEventNotifier m_notifier;
    Qt::HANDLE m_handle;
};

}

#endif
