// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "PlatformMediaPlayer_p.h"
#include <private/MediaPlayer_p.h>
#include "MediaPlayer.h"
#include "PlatformAudioDevices_p.h"
#include "PlatformMediaIntegration_p.h"
#include <private/AudioDeviceMonitor_p.h>

void PlatformMediaPlayer::tracksChanged()
{
    // 在发射信号前先更新轨道缓存，确保 QML 读取到最新数据
    if (auto *d = MediaPlayerPrivate::get(player))
        d->updateTrackCache();
    emit player->tracksChanged();
    emit player->activeAudioTrackChanged();
    emit player->activeVideoTrackChanged();
    emit player->activeSubtitleTrackChanged();
    emit player->chaptersChanged();
}

PlatformMediaPlayer::PlatformMediaPlayer(MediaPlayer *parent) : player(parent)
{
    QObject::connect(&AudioDeviceMonitor::instance(), &AudioDeviceMonitor::audioOutputDeviceChanged,
                     parent, [this] { onAudioOutputDeviceChanged(); });
}

PlatformMediaPlayer::~PlatformMediaPlayer() = default;

QImage PlatformMediaPlayer::getMediaCover(QSize size, bool decodeFrame)
{
    if (m_cachedCover.isNull()) {
        m_cachedCover = getMediaCover(decodeFrame);
        if (m_cachedCover.isNull())
            return {};
    }

    if (!size.isValid())
        return m_cachedCover;

    const QImage scaledImage = m_cachedCover.scaled(size, Qt::KeepAspectRatioByExpanding,
                                             Qt::SmoothTransformation);

    const int x = (scaledImage.width() - size.width()) / 2;
    const int y = (scaledImage.height() - size.height()) / 2;

    return scaledImage.copy(x, y, size.width(), size.height());
}

void PlatformMediaPlayer::stateChanged(MediaPlayer::PlaybackState newState)
{
    if (newState == m_state)
        return;
    m_state = newState;
    player->d_func()->setState(newState);
}

void PlatformMediaPlayer::mediaStatusChanged(MediaPlayer::MediaStatus status)
{
    if (m_status == status)
        return;
    const auto oldStatus = std::exchange(m_status, status);
    player->d_func()->setStatus(status, oldStatus);
}

void PlatformMediaPlayer::error(MediaPlayer::Error error, const QString &errorString)
{
    player->d_func()->setError(error, errorString);
}

PlatformMediaPlayer::PitchCompensationAvailability
PlatformMediaPlayer::pitchCompensationAvailability() const
{
    return PitchCompensationAvailability::Unavailable;
}

void PlatformMediaPlayer::setPitchCompensation(bool )
{
    qWarning() << "MediaPlayer::setPitchCompensation not supported on this QtMultimedia "
                  "backend";
}

bool PlatformMediaPlayer::pitchCompensation() const
{
    return false;
}

void PlatformMediaPlayer::pitchCompensationChanged(bool enabled) const
{
    emit player->pitchCompensationChanged(enabled);
}

PlaybackOptions PlatformMediaPlayer::playbackOptions() const
{
    return player->d_func()->playbackOptions;
}

void PlatformMediaPlayer::setPlaybackOptions(const PlaybackOptions &)
{
}

