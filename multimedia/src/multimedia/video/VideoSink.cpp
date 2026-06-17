// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "VideoSink.h"

#include "VideoFrameFormat.h"
#include "VideoFrame.h"
#include "MediaPlayer.h"

#include <qvariant.h>
#include <qpainter.h>
#include <qmatrix4x4.h>
#include <QDebug>
#include <private/PlatformMediaIntegration_p.h>
#include <private/PlatformVideoSink_p.h>

import qzLog;

class VideoSinkPrivate {
public:
    explicit VideoSinkPrivate(VideoSink *q)
        : q_ptr(q)
    {
        if (auto maybeVideoSink = PlatformMediaIntegration::instance()->createVideoSink(q)) {
            videoSink = maybeVideoSink.value();
        } else {
            qWarning() << "Failed to create VideoSink" << maybeVideoSink.error();
        }
    }
    ~VideoSinkPrivate()
    {
        delete videoSink;
    }
    void unregisterSource()
    {
        if (!source)
            return;
        auto *old = source;
        source = nullptr;
        if (auto *player = qobject_cast<MediaPlayer *>(old))
            player->setVideoSink(nullptr);
    }

    VideoSink *q_ptr = nullptr;
    PlatformVideoSink *videoSink = nullptr;
    QObject *source = nullptr;
    QRhi *rhi = nullptr;
    PlaybackOptions::HdrPolicy m_hdrPolicy = PlaybackOptions::HdrPolicy::Enabled;
    bool m_activeHdr = false;
};

VideoSink::VideoSink(QObject *parent)
    : QObject(parent),
    d(new VideoSinkPrivate(this))
{
    qRegisterMetaType<VideoFrame>();
}

VideoSink::~VideoSink()
{
    disconnect(this);
    d->unregisterSource();
    delete d;
}

QRhi *VideoSink::rhi() const
{
    return d->rhi;
}

void VideoSink::setRhi(QRhi *rhi)
{
    if (d->rhi == rhi)
        return;
    d->rhi = rhi;
    if (d->videoSink)
        d->videoSink->setRhi(rhi);
}

PlatformVideoSink *VideoSink::platformVideoSink() const
{
    return d->videoSink;
}

VideoFrame VideoSink::videoFrame() const
{
    return d->videoSink ? d->videoSink->currentVideoFrame() : VideoFrame{};
}

void VideoSink::setVideoFrame(const VideoFrame &frame)
{
    if (d->videoSink)
        d->videoSink->setVideoFrame(frame);
}

QString VideoSink::subtitleText() const
{
    return d->videoSink ? d->videoSink->subtitleText() : QString{};
}

void VideoSink::setSubtitleText(const QString &subtitle)
{
    if (d->videoSink)
        d->videoSink->setSubtitleText(subtitle);
}

SubtitleStyle VideoSink::subtitleStyle() const
{
    return d->videoSink ? d->videoSink->subtitleStyle() : SubtitleStyle{};
}

void VideoSink::setSubtitleStyle(const SubtitleStyle &style)
{
    if (d->videoSink)
        d->videoSink->setSubtitleStyle(style);
}

QImage VideoSink::subtitleImage() const
{
    return d->videoSink ? d->videoSink->subtitleImage() : QImage{};
}

void VideoSink::setSubtitleImage(const QImage &image, const QRect &rect)
{
    if (d->videoSink)
        d->videoSink->setSubtitleImage(image, rect);
}

QRect VideoSink::subtitleRect() const
{
    return d->videoSink ? d->videoSink->subtitleRect() : QRect{};
}

void VideoSink::setSubtitleBitmapData(const SubtitleBitmapData &data)
{
    if (d->videoSink)
        d->videoSink->setSubtitleBitmapData(data);
}

SubtitleBitmapData VideoSink::subtitleBitmapData() const
{
    return d->videoSink ? d->videoSink->subtitleBitmapData() : SubtitleBitmapData{};
}

QSize VideoSink::videoSize() const
{
    return d->videoSink ? d->videoSink->nativeSize() : QSize{};
}

PlaybackOptions::HdrPolicy VideoSink::hdrPolicy() const
{
    return d->m_hdrPolicy;
}

void VideoSink::setHdrPolicy(PlaybackOptions::HdrPolicy policy)
{
    if (d->m_hdrPolicy == policy)
        return;
    d->m_hdrPolicy = policy;
    emit hdrPolicyChanged();
}

bool VideoSink::activeHdr() const
{
    return d->m_activeHdr;
}

void VideoSink::setActiveHdr(bool active)
{
    if (d->m_activeHdr == active)
        return;
    d->m_activeHdr = active;
    emit activeHdrChanged(active);
}

void VideoSink::setSource(QObject *source)
{
    if (d->source == source)
        return;
    if (source)
        d->unregisterSource();
    d->source = source;
}

#include "moc_VideoSink.cpp"

