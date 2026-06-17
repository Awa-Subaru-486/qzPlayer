// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef WINDOWSSMTCCONTROL_P_H
#define WINDOWSSMTCCONTROL_P_H

#include <QtCore/qobject.h>
#include <qzMultimedia/MediaPlayer.h>
#include <qzMultimedia/MediaMetadata.h>
#include <QtGui/qimage.h>

// SMTC worker object designed to run in a dedicated thread with MTA.
// Public methods are invoked via signal/slot connections from WindowsSmtcManager.
class WindowsSmtcControl : public QObject
{
    Q_OBJECT
public:
    explicit WindowsSmtcControl(QObject *parent = nullptr);
    ~WindowsSmtcControl() override;

    bool isInitialized() const { return m_initialized; }

    void init(WId windowId);
    void setPlaybackState(MediaPlayer::PlaybackState state);
    void updateMetadata(const MediaMetaData &metaData, bool hasVideo);
    void updateThumbnail(const QImage &image) const;
    void clearMetadata();
    void setPosition(qint64 positionMs);
    void setDuration(qint64 durationMs);

signals:
    void playRequested();
    void pauseRequested();
    void stopRequested();
    void nextRequested();
    void previousRequested();
    void seekRequested(qint64 positionMs);

private:
    struct SmtcImpl;
    std::unique_ptr<SmtcImpl> m_impl;
    bool m_initialized = false;
};

#endif
