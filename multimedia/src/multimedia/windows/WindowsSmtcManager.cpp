// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "WindowsSmtcManager_p.h"
#include "WindowsSmtcControl_p.h"

#include <qzMultimedia/MediaPlayer.h>
#include <qzMultimedia/MediaMetadata.h>

import qzLog;
#include <QtGui/qwindow.h>
#include <QtGui/qguiapplication.h>

static qz::Log::LogCategory qLcSmtcManager("qz.multimedia.smtc.manager");

WindowsSmtcManager::WindowsSmtcManager(QObject *parent)
    : QObject(parent)
    , m_smtcControl(new WindowsSmtcControl) // no parent — will be moved to worker thread
    , m_workerThread(new QThread(this))
{
    // 将 SMTC worker 移到独立线程
    m_smtcControl->moveToThread(m_workerThread);

    // Manager 信号 -> Control 槽（跨线程 QueuedConnection）
    connect(this, &WindowsSmtcManager::smtcInitRequested,
            m_smtcControl, &WindowsSmtcControl::init, Qt::QueuedConnection);
    connect(this, &WindowsSmtcManager::playbackStateChanged,
            m_smtcControl, &WindowsSmtcControl::setPlaybackState, Qt::QueuedConnection);
    connect(this, &WindowsSmtcManager::metadataUpdateRequested,
            m_smtcControl, &WindowsSmtcControl::updateMetadata, Qt::QueuedConnection);
    connect(this, &WindowsSmtcManager::thumbnailUpdateRequested,
            m_smtcControl, &WindowsSmtcControl::updateThumbnail, Qt::QueuedConnection);
    connect(this, &WindowsSmtcManager::clearMetadataRequested,
            m_smtcControl, &WindowsSmtcControl::clearMetadata, Qt::QueuedConnection);
    connect(this, &WindowsSmtcManager::positionChanged,
            m_smtcControl, &WindowsSmtcControl::setPosition, Qt::QueuedConnection);
    connect(this, &WindowsSmtcManager::durationChanged,
            m_smtcControl, &WindowsSmtcControl::setDuration, Qt::QueuedConnection);

    // SMTC 按钮信号 -> MediaPlayer 操作（跨线程，自动 QueuedConnection）
    connect(m_smtcControl, &WindowsSmtcControl::playRequested, this, [this]() {
        if (m_mediaPlayer) m_mediaPlayer->play();
    });
    connect(m_smtcControl, &WindowsSmtcControl::pauseRequested, this, [this]() {
        if (m_mediaPlayer) m_mediaPlayer->pause();
    });
    connect(m_smtcControl, &WindowsSmtcControl::stopRequested, this, [this]() {
        if (m_mediaPlayer) m_mediaPlayer->stop();
    });
    connect(m_smtcControl, &WindowsSmtcControl::nextRequested, this, [this]() {
        if (m_mediaPlayer) m_mediaPlayer->next();
    });
    connect(m_smtcControl, &WindowsSmtcControl::previousRequested, this, [this]() {
        if (m_mediaPlayer) m_mediaPlayer->previous();
    });

    // 启动工作线程
    m_workerThread->start();
}

WindowsSmtcManager::~WindowsSmtcManager()
{
    // 在工作线程中安全析构 SMTC 对象
    m_smtcControl->deleteLater();
    m_workerThread->quit();
    m_workerThread->wait();
}

void WindowsSmtcManager::setMediaPlayer(MediaPlayer *player)
{
    if (m_mediaPlayer == player)
        return;

    disconnectMediaPlayerSignals();
    m_mediaPlayer = player;

    if (player)
        connectMediaPlayerSignals();
}

void WindowsSmtcManager::connectMediaPlayerSignals()
{
    if (!m_mediaPlayer)
        return;

    connect(m_mediaPlayer, &MediaPlayer::playbackStateChanged, this,
            [this](MediaPlayer::PlaybackState state) {
                if (!m_smtcInitialized)
                    tryInitializeSmtc();
                if (m_smtcInitialized)
                    emit playbackStateChanged(state);
            });
    connect(m_mediaPlayer, &MediaPlayer::metaDataChanged, this,
            [this]() {
                if (!m_smtcInitialized)
                    tryInitializeSmtc();
                updateSmtcMetadata();
            });
    connect(m_mediaPlayer, &MediaPlayer::positionChanged, this,
            [this](qint64 position) {
                if (m_smtcInitialized)
                    emit positionChanged(position);
            });
    connect(m_mediaPlayer, &MediaPlayer::durationChanged, this,
            [this](qint64 duration) {
                if (m_smtcInitialized)
                    emit durationChanged(duration);
            });
    connect(m_mediaPlayer, &MediaPlayer::mediaStatusChanged, this,
            [this](MediaPlayer::MediaStatus status) {
                if (!m_smtcInitialized && status != MediaPlayer::NoMedia)
                    tryInitializeSmtc();
                if (status == MediaPlayer::NoMedia && m_smtcInitialized)
                    emit clearMetadataRequested();
            });
}

void WindowsSmtcManager::disconnectMediaPlayerSignals()
{
    if (!m_mediaPlayer)
        return;

    disconnect(m_mediaPlayer, nullptr, this, nullptr);
}

void WindowsSmtcManager::tryInitializeSmtc()
{
    if (m_smtcInitialized)
        return;

    // 从 QGuiApplication 的顶层窗口获取窗口句柄
    WId windowId = 0;
    if (QGuiApplication::instance()) {
        const auto topWindows = QGuiApplication::topLevelWindows();
        for (auto *window : topWindows) {
            if (auto wid = window->winId()) {
                windowId = wid;
                break;
            }
        }
    }

    if (!windowId) {
        qz::Log::cat_debug(qLcSmtcManager, "No window handle available, SMTC init deferred");
        return;
    }

    emit smtcInitRequested(windowId);
    m_smtcInitialized = true;
    qz::Log::cat_debug(qLcSmtcManager, "SMTC init requested for window");
}

void WindowsSmtcManager::updateSmtcMetadata()
{
    if (!m_smtcInitialized || !m_mediaPlayer)
        return;

    const auto metaData = m_mediaPlayer->metaData();
    const bool hasVideo = m_mediaPlayer->hasVideo();
    emit metadataUpdateRequested(metaData, hasVideo);

    // 更新缩略图
    updateSmtcThumbnail();

    // 更新时长
    const auto duration = m_mediaPlayer->duration();
    if (duration > 0)
        emit durationChanged(duration);
}

void WindowsSmtcManager::updateSmtcThumbnail()
{
    if (!m_smtcInitialized || !m_mediaPlayer)
        return;

    // SMTC 缩略图推荐尺寸 200x200
    if (const QImage cover = m_mediaPlayer->getMediaCover(QSize(200, 200)); !cover.isNull())
        emit thumbnailUpdateRequested(cover);
}
