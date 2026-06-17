// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "VideoOutputOrientationHandler_p.h"

#include <QGuiApplication>
#include <QScreen>

bool VideoOutputOrientationHandler::m_isRecording = false;

VideoOutputOrientationHandler::VideoOutputOrientationHandler(QObject *parent)
    : QObject(parent)
    , m_currentOrientation(0)
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    connect(screen, &QScreen::orientationChanged, this,
            &VideoOutputOrientationHandler::screenOrientationChanged);

    screenOrientationChanged(screen->orientation());
}

int VideoOutputOrientationHandler::currentOrientation() const
{
    return m_currentOrientation;
}

void VideoOutputOrientationHandler::screenOrientationChanged(Qt::ScreenOrientation orientation)
{
    if (m_isRecording)
        return;

    const QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const int angle = (360 - screen->angleBetween(screen->nativeOrientation(), orientation)) % 360;

    if (angle == m_currentOrientation)
        return;

    m_currentOrientation = angle;
    emit orientationChanged(m_currentOrientation);
}

#include "moc_VideoOutputOrientationHandler_p.cpp"
