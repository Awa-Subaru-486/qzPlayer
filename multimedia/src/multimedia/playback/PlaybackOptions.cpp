// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "PlaybackOptions.h"
#include <chrono>

using namespace std::chrono_literals;

class PlaybackOptionsPrivate : public QSharedData
{
public:
    PlaybackOptionsPrivate()
        : m_videoDecoderPriority(defaultVideoDecoderPriority())
    {

    }

    friend bool comparesEqual(const PlaybackOptionsPrivate &lhs,
                              const PlaybackOptionsPrivate &rhs)
    {
        return lhs.m_networkTimeout == rhs.m_networkTimeout
                && lhs.m_playbackIntent == rhs.m_playbackIntent
                && lhs.m_probeSizeBytes == rhs.m_probeSizeBytes
                && lhs.m_videoDecoderPriority == rhs.m_videoDecoderPriority
                && lhs.m_zeroCopy == rhs.m_zeroCopy
                && lhs.m_hdrPolicy == rhs.m_hdrPolicy;
    }

    friend Qt::strong_ordering compareThreeWay(const PlaybackOptionsPrivate &lhs,
                                               const PlaybackOptionsPrivate &rhs)
    {
        if (lhs.m_networkTimeout != rhs.m_networkTimeout)
            return qCompareThreeWay(lhs.m_networkTimeout.count(), rhs.m_networkTimeout.count());
        if (lhs.m_playbackIntent != rhs.m_playbackIntent)
            return qCompareThreeWay(lhs.m_playbackIntent, rhs.m_playbackIntent);
        if (lhs.m_probeSizeBytes != rhs.m_probeSizeBytes)
            return qCompareThreeWay(lhs.m_probeSizeBytes, rhs.m_probeSizeBytes);
        if (lhs.m_videoDecoderPriority != rhs.m_videoDecoderPriority)
        return qCompareThreeWay(lhs.m_videoDecoderPriority, rhs.m_videoDecoderPriority);
        if (lhs.m_zeroCopy != rhs.m_zeroCopy)
            return qCompareThreeWay(lhs.m_zeroCopy, rhs.m_zeroCopy);
        return qCompareThreeWay(lhs.m_hdrPolicy, rhs.m_hdrPolicy);
    }

    static QVector<PlaybackOptions::VideoDecoderPolicy> defaultVideoDecoderPriority()
    {
        return {
    #ifdef Q_OS_WINDOWS
            PlaybackOptions::VideoDecoderPolicy::HardwareVkVideo,
            PlaybackOptions::VideoDecoderPolicy::HardwareD3D11VA,
            PlaybackOptions::VideoDecoderPolicy::Software,
    #elif defined(Q_OS_ANDROID)
            PlaybackOptions::VideoDecoderPolicy::HardwareMediaVideo,
            PlaybackOptions::VideoDecoderPolicy::HardwareVkVideo,
            PlaybackOptions::VideoDecoderPolicy::Software,
    #else
            PlaybackOptions::VideoDecoderPolicy::HardwareVkVideo,
            PlaybackOptions::VideoDecoderPolicy::Software,
    #endif
        };
    }

    std::chrono::milliseconds m_networkTimeout = 20s;
    PlaybackOptions::PlaybackIntent m_playbackIntent = PlaybackOptions::PlaybackIntent::Playback;
    int m_probeSizeBytes = -1;
    QVector<PlaybackOptions::VideoDecoderPolicy> m_videoDecoderPriority;
    PlaybackOptions::ZeroCopy m_zeroCopy = PlaybackOptions::ZeroCopy::Enabled;
    PlaybackOptions::HdrPolicy m_hdrPolicy = PlaybackOptions::HdrPolicy::Enabled;
};

QT_DEFINE_QESDP_SPECIALIZATION_DTOR(PlaybackOptionsPrivate)

PlaybackOptions::PlaybackOptions() : d{ new PlaybackOptionsPrivate } { }
PlaybackOptions::PlaybackOptions(const PlaybackOptions &) = default;
PlaybackOptions &PlaybackOptions::operator=(const PlaybackOptions &) = default;
PlaybackOptions::~PlaybackOptions() = default;

bool comparesEqual(const PlaybackOptions &lhs, const PlaybackOptions &rhs) noexcept
{
    if (lhs.d == rhs.d)
        return true;

    return comparesEqual(*lhs.d, *rhs.d);
}

Qt::strong_ordering compareThreeWay(const PlaybackOptions &lhs, const PlaybackOptions &rhs) noexcept
{
    return compareThreeWay(*lhs.d, *rhs.d);
}

std::chrono::milliseconds PlaybackOptions::networkTimeout() const
{
    return d->m_networkTimeout;
}

void PlaybackOptions::setNetworkTimeout(std::chrono::milliseconds timeout)
{
    d.detach();
    d->m_networkTimeout = timeout;
}

void PlaybackOptions::resetNetworkTimeout()
{
    d.detach();
    d->m_networkTimeout = PlaybackOptionsPrivate{}.m_networkTimeout;
}

PlaybackOptions::PlaybackIntent PlaybackOptions::playbackIntent() const
{
    return d->m_playbackIntent;
}

void PlaybackOptions::setPlaybackIntent(PlaybackIntent intent)
{
    d.detach();
    d->m_playbackIntent = intent;
}

void PlaybackOptions::resetPlaybackIntent()
{
    d.detach();
    d->m_playbackIntent = PlaybackOptionsPrivate{}.m_playbackIntent;
}

qsizetype PlaybackOptions::probeSize() const
{
    return d->m_probeSizeBytes;
}

void PlaybackOptions::setProbeSize(qsizetype probeSizeBytes)
{
    d.detach();
    d->m_probeSizeBytes = static_cast<int>(probeSizeBytes);
}

void PlaybackOptions::resetProbeSize()
{
    d.detach();
    d->m_probeSizeBytes = PlaybackOptionsPrivate{}.m_probeSizeBytes;
}


QVector<PlaybackOptions::VideoDecoderPolicy> PlaybackOptions::videoDecoderPriority() const
{
    return d->m_videoDecoderPriority;
}

void PlaybackOptions::setVideoDecoderPriority(const QVector<VideoDecoderPolicy> &priority)
{
    d.detach();
    d->m_videoDecoderPriority = priority;
}

void PlaybackOptions::prioritizeDecoder(const VideoDecoderPolicy policy)
{
    d.detach();

    auto &priority = d->m_videoDecoderPriority;
    auto it = std::ranges::find(priority, policy);

    if (it != priority.end()) {
        std::rotate(priority.begin(), it, it + 1);
    } else {
        priority.prepend(policy);
    }
}

void PlaybackOptions::deprioritizeDecoder(VideoDecoderPolicy policy)
{
    d.detach();

    auto &priority = d->m_videoDecoderPriority;
    auto it = std::ranges::find(priority, policy);
    if (it != priority.end())
    {
        std::rotate(it, it + 1, priority.end());
    }
    else
    {
        priority.append(policy);
    }
}

PlaybackOptions::ZeroCopy PlaybackOptions::zeroCopy() const
{
    return d->m_zeroCopy;
}

void PlaybackOptions::setZeroCopy(const ZeroCopy zeroCopy)
{
    d.detach();
    d->m_zeroCopy = zeroCopy;
}

void PlaybackOptions::resetZeroCopy()
{
    d.detach();
    d->m_zeroCopy = PlaybackOptionsPrivate{}.m_zeroCopy;
}

PlaybackOptions::HdrPolicy PlaybackOptions::hdrPolicy() const
{
    return d->m_hdrPolicy;
}

void PlaybackOptions::setHdrPolicy(HdrPolicy policy)
{
    d.detach();
    d->m_hdrPolicy = policy;
}

void PlaybackOptions::resetHdrPolicy()
{
    d.detach();
    d->m_hdrPolicy = PlaybackOptionsPrivate{}.m_hdrPolicy;
}

#include "moc_PlaybackOptions.cpp"
