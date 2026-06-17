// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef WINDOWSSMTCMANAGER_P_H
#define WINDOWSSMTCMANAGER_P_H

#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtCore/qthread.h>
#include <qzMultimedia/MediaPlayer.h>
#include <qzMultimedia/MediaMetadata.h>
#include <QtGui/qimage.h>

class WindowsSmtcControl;

class WindowsSmtcManager : public QObject
{
    Q_OBJECT
public:
    explicit WindowsSmtcManager(QObject *parent = nullptr);
    ~WindowsSmtcManager() override;

    void setMediaPlayer(MediaPlayer *player);

signals:
    void smtcInitRequested(WId windowId);
    void playbackStateChanged(MediaPlayer::PlaybackState state);
    void metadataUpdateRequested(const MediaMetaData &metaData, bool hasVideo);
    void thumbnailUpdateRequested(const QImage &image);
    void clearMetadataRequested();
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);

private:
    void connectMediaPlayerSignals();
    void disconnectMediaPlayerSignals();
    void tryInitializeSmtc();
    void updateSmtcMetadata();
    void updateSmtcThumbnail();

    QPointer<MediaPlayer> m_mediaPlayer;
    WindowsSmtcControl *m_smtcControl = nullptr;
    QThread *m_workerThread = nullptr;
    bool m_smtcInitialized = false;
};

#endif
