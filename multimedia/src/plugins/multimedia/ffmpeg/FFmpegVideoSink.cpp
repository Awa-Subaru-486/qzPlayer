#include "FFmpegVideoSink_p.h"
#include "FFmpegVideoBuffer_p.h"
#include <private/VideoFrame_p.h>

QT_BEGIN_NAMESPACE

namespace ffmpeg {
VideoSink::VideoSink(::VideoSink *sink)
    : PlatformVideoSink(sink)
{
}

void VideoSink::setRhi(QRhi *rhi)
{
    {
        QMutexLocker guard{ &m_rhiMutex };
        if (m_rhi == rhi)
            return;
        m_rhi = rhi;
    }

    emit rhiChanged();
}

void VideoSink::onVideoFrameChanged(const ::VideoFrame &frame)
{
    QMutexLocker guard { &m_rhiMutex };
    auto *buffer = VideoFramePrivate::hwBuffer(frame);
    if (buffer && m_rhi)
        buffer->initTextureConverter(*m_rhi);
}
}
QT_END_NAMESPACE

#include "moc_FFmpegVideoSink_p.cpp"
