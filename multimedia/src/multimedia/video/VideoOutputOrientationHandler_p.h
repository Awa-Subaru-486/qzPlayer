// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_VIDEO_VIDEOOUTPUTORIENTATIONHANDLER_P_H
#define QT_VIDEO_VIDEOOUTPUTORIENTATIONHANDLER_P_H

#include <MultimediaGlobal.h>

#include <QObject>
#include <private/qglobal_p.h>

class QZ_MULTIMEDIA_EXPORT VideoOutputOrientationHandler : public QObject
{
    Q_OBJECT
public:
    explicit VideoOutputOrientationHandler(QObject *parent = nullptr);

    int currentOrientation() const;

    static void setIsRecording(bool isRecording) { m_isRecording = isRecording; }
    static bool isRecording() { return m_isRecording; }

Q_SIGNALS:
    void orientationChanged(int angle);

private Q_SLOTS:
    void screenOrientationChanged(Qt::ScreenOrientation orientation);

private:
    int m_currentOrientation;
    static bool m_isRecording;
};

#endif
