// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef AUDIOAUDIODEVICEMONITOR_P_H
#define AUDIOAUDIODEVICEMONITOR_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/private/qglobal_p.h>
#include <QtCore/qobject.h>

QT_BEGIN_NAMESPACE

class AudioDeviceMonitor : public QObject
{
    Q_OBJECT

public:
    static AudioDeviceMonitor &instance();

    // Called from platform-specific code (JNI callback, AAudio error callback, etc.)
    // Thread-safe: can be called from any thread.
    static void notifyOutputDeviceChanged();

signals:
    // Emitted on the main thread when the audio output device changes.
    // PlatformMediaPlayer connects this to onAudioOutputDeviceChanged().
    void audioOutputDeviceChanged();

private:
    explicit AudioDeviceMonitor(QObject *parent = nullptr);
};

QT_END_NAMESPACE

#endif // AUDIOAUDIODEVICEMONITOR_P_H
