// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "PlatformVideoSink_p.h"
#include "MultimediaUtils_p.h"
import qzLog;

static qz::Log::LogCategory qLcPlatformVideoSink("qz.multimedia.platform.videosink");

PlatformVideoSink::PlatformVideoSink(VideoSink *parent) : QObject(parent), m_sink(parent) { }

PlatformVideoSink::~PlatformVideoSink() = default;

QSize PlatformVideoSink::nativeSize() const
{
    QMutexLocker locker(&m_mutex);
    return m_nativeSize;
}

void PlatformVideoSink::setNativeSize(QSize s)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_nativeSize == s)
            return;
        m_nativeSize = s;
    }
    emit m_sink->videoSizeChanged();
}

void PlatformVideoSink::setVideoFrame(const VideoFrame &frame)
{
    bool sizeChanged = false;
    VideoFrame currentFrame;
    {
        QMutexLocker locker(&m_mutex);
        if (frame == m_currentVideoFrame)
            return;
        m_currentVideoFrame = frame;
        m_currentVideoFrame.setSubtitleText(m_subtitleText);
        m_currentVideoFrame.setSubtitleImage(m_subtitleImage);
        m_currentVideoFrame.setSubtitleRect(m_subtitleRect);
        m_currentVideoFrame.setSubtitleBitmapData(m_subtitleBitmapData);
        m_currentVideoFrame.setSubtitleStyle(m_subtitleStyle);
        const QSize size = qRotatedFramePresentationSize(frame);
        if (size != m_nativeSize) {
            m_nativeSize = size;
            sizeChanged = true;
        }
        currentFrame = m_currentVideoFrame;
    }

    onVideoFrameChanged(currentFrame);

    if (sizeChanged)
        emit m_sink->videoSizeChanged();
    emit m_sink->videoFrameChanged(currentFrame);
}

VideoFrame PlatformVideoSink::currentVideoFrame() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentVideoFrame;
}

void PlatformVideoSink::setSubtitleText(const QString &subtitleText)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_subtitleText == subtitleText)
            return;
        m_subtitleText = subtitleText;
        if (m_currentVideoFrame.isValid())
            m_currentVideoFrame.setSubtitleText(subtitleText);
        m_currentVideoFrame.setSubtitleStyle(m_subtitleStyle);
    }
    emit m_sink->subtitleTextChanged(subtitleText);
    emit subtitleChanged();
}

QString PlatformVideoSink::subtitleText() const
{
    QMutexLocker locker(&m_mutex);
    return m_subtitleText;
}

void PlatformVideoSink::setSubtitleImage(const QImage &image, const QRect &rect)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_subtitleRect == rect && m_subtitleImage.size() == image.size()
            && m_subtitleImage.constBits() == image.constBits())
            return;
        qz::Log::cat_debug(qLcPlatformVideoSink, "Set subtitle image: imageSize={}x{} rect={} frameValid={}",
                            image.width(), image.height(), rect,
                            m_currentVideoFrame.isValid());
        m_subtitleImage = image;
        m_subtitleRect = rect;
        if (m_currentVideoFrame.isValid()) {
            m_currentVideoFrame.setSubtitleImage(image);
            m_currentVideoFrame.setSubtitleRect(rect);
        }
    }
    emit subtitleChanged();
}

QImage PlatformVideoSink::subtitleImage() const
{
    QMutexLocker locker(&m_mutex);
    return m_subtitleImage;
}

QRect PlatformVideoSink::subtitleRect() const
{
    QMutexLocker locker(&m_mutex);
    return m_subtitleRect;
}

void PlatformVideoSink::setSubtitleStyle(const SubtitleStyle &style)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_subtitleStyle == style)
            return;
        m_subtitleStyle = style;
        if (m_currentVideoFrame.isValid())
            m_currentVideoFrame.setSubtitleStyle(style);
    }
    emit subtitleStyleChanged();
    emit subtitleChanged();
}

SubtitleStyle PlatformVideoSink::subtitleStyle() const
{
    QMutexLocker locker(&m_mutex);
    return m_subtitleStyle;
}

void PlatformVideoSink::setSubtitleBitmapData(const SubtitleBitmapData &data)
{
    {
        QMutexLocker locker(&m_mutex);
        // 去重：rect 数量和指针相同即为相同数据
        if (m_subtitleBitmapData.isSameData(data))
            return;
        m_subtitleBitmapData = data;
        if (m_currentVideoFrame.isValid())
            m_currentVideoFrame.setSubtitleBitmapData(data);
    }
    emit subtitleChanged();
}

SubtitleBitmapData PlatformVideoSink::subtitleBitmapData() const
{
    QMutexLocker locker(&m_mutex);
    return m_subtitleBitmapData;
}

#include "moc_PlatformVideoSink_p.cpp"
