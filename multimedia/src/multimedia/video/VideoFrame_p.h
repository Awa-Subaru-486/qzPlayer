// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_VIDEO_VIDEOFRAME_P_H
#define QT_VIDEO_VIDEOFRAME_P_H

#include "VideoFrame.h"
#include "HwVideoBuffer_p.h"
#include "SubtitleStyle.h"
#include "private/VideoTransformation_p.h"

#include <qmutex.h>

class VideoFramePrivate : public QSharedData
{
public:
    VideoFramePrivate() = default;

    ~VideoFramePrivate()
    {
        if (videoBuffer && mapMode != VideoFrame::NotMapped)
            videoBuffer->unmap();
    }

    template <typename Buffer>
    static VideoFrame createFrame(std::unique_ptr<Buffer> buffer, VideoFrameFormat format)
    {
        VideoFrame result;
        result.d.reset(new VideoFramePrivate(std::move(format), std::move(buffer)));
        return result;
    }

    template <typename Buffer = AbstractVideoBuffer>
    VideoFramePrivate(VideoFrameFormat format, std::unique_ptr<Buffer> buffer = nullptr)
        : format{ std::move(format) }, videoBuffer{ std::move(buffer) }
    {
        if constexpr (std::is_base_of_v<HwVideoBuffer, Buffer>)
            hwVideoBuffer = static_cast<HwVideoBuffer *>(videoBuffer.get());
        else if constexpr (std::is_same_v<AbstractVideoBuffer, Buffer>)
            hwVideoBuffer = dynamic_cast<HwVideoBuffer *>(videoBuffer.get());

    }

    static VideoFramePrivate *handle(VideoFrame &frame) { return frame.d.get(); }

    static HwVideoBuffer *hwBuffer(const VideoFrame &frame)
    {
        return frame.d ? frame.d->hwVideoBuffer : nullptr;
    }

    static bool hasDmaBuf(const VideoFrame &frame)
    {
        HwVideoBuffer *hwVideoBuffer = hwBuffer(frame);
        return hwVideoBuffer && hwVideoBuffer->isDmaBuf();
    }

    static AbstractVideoBuffer *buffer(const VideoFrame &frame)
    {
        return frame.d ? frame.d->videoBuffer.get() : nullptr;
    }

    VideoFrame adoptThisByVideoFrame()
    {
        VideoFrame frame;
        frame.d = QExplicitlySharedDataPointer(this, QAdoptSharedDataTag{});
        return frame;
    }

    qint64 startTime = -1;
    qint64 endTime = -1;
    AbstractVideoBuffer::MapData mapData;
    VideoFrame::MapMode mapMode = VideoFrame::NotMapped;
    VideoFrameFormat format;
    std::unique_ptr<AbstractVideoBuffer> videoBuffer;
    HwVideoBuffer *hwVideoBuffer = nullptr;
    int mappedCount = 0;
    QMutex mapMutex;
    QString subtitleText;
    QImage subtitleImage;
    QRect subtitleRect;
    SubtitleBitmapData subtitleBitmapData;
    SubtitleStyle subtitleStyle;
    QImage image;
    QMutex imageMutex;
    VideoTransformation presentationTransformation;

private:
    Q_DISABLE_COPY(VideoFramePrivate)
};

#endif
