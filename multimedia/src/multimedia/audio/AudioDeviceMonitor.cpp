// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "AudioDeviceMonitor_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>

QT_BEGIN_NAMESPACE

AudioDeviceMonitor::AudioDeviceMonitor(QObject *parent)
    : QObject(parent)
{
}

AudioDeviceMonitor &AudioDeviceMonitor::instance()
{
    static AudioDeviceMonitor monitor;
    return monitor;
}

void AudioDeviceMonitor::notifyOutputDeviceChanged()
{
    // Schedule signal emission on the main thread's event loop.
    // This is safe to call from any thread (JNI callback, AAudio error callback, etc.).
    QTimer::singleShot(0, &instance(), [] {
        emit instance().audioOutputDeviceChanged();
    });
}

QT_END_NAMESPACE

#include "moc_AudioDeviceMonitor_p.cpp"
