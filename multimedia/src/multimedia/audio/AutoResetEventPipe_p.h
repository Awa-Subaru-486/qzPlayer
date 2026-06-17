// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.


#ifndef QT_AUDIO_AUTORESETEVENTPIPE_P_H
#define QT_AUDIO_AUTORESETEVENTPIPE_P_H
#include <QtCore/qsocketnotifier.h>
#include <qzMultimedia/qtmultimediaexports.h>
#include <atomic>

namespace QtPrivate {

class QZ_MULTIMEDIA_EXPORT AutoResetEventPipe final : public QObject
{
    Q_OBJECT

public:
    explicit AutoResetEventPipe(QObject *parent = nullptr);
    ~AutoResetEventPipe();
    Q_DISABLE_COPY_MOVE(AutoResetEventPipe)

    bool isValid() const { return m_fdProducer != -1; }

    void set();

    template <typename... Args>
    QMetaObject::Connection callOnActivated(Args &&...args)
    {
        return connect(this, &AutoResetEventPipe::activated, std::forward<Args>(args)...);
    }

    template <typename Functor>
    QMetaObject::Connection callOnActivated(Functor &&functor)
    {
        return connect(this, &AutoResetEventPipe::activated, this, std::forward<Functor>(functor));
    }

Q_SIGNALS:
    void activated();

private:
    QSocketNotifier m_notifier;
    int m_fdProducer = -1;
    int m_fdConsumer = -1;
    std::atomic_flag m_consumePending{};
};

}

#endif
