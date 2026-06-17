#ifndef FFMPEGVIDEOSINK_P_H
#define FFMPEGVIDEOSINK_P_H

#include <qzMultimedia/private/PlatformVideoSink_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegHwAccel_p.h>
#include <QtCore/qmutex.h>

QT_BEGIN_NAMESPACE

class QString;

namespace ffmpeg {

// 视频输出实现，将视频帧输出到接收器
class VideoSink : public PlatformVideoSink
{
    Q_OBJECT

public:
    VideoSink(::VideoSink *sink);
    void setRhi(QRhi *rhi) override;

protected:
    void onVideoFrameChanged(const ::VideoFrame &frame) override;

private:
    QMutex m_rhiMutex;
    QRhi *m_rhi = nullptr;
};
}
QT_END_NAMESPACE

#endif
