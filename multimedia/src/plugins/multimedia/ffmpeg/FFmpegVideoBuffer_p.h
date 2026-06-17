#ifndef FFMPEGVIDEOBUFFER_P_H
#define FFMPEGVIDEOBUFFER_P_H

#include <qzMultimedia/private/HwVideoBuffer_p.h>
#include <QtCore/qvariant.h>

#include <qzFFmpegMediaPluginImpl/private/FFmpeg_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegHwAccel_p.h>

QT_BEGIN_NAMESPACE

namespace ffmpeg {
// 视频缓冲区，管理视频帧数据和硬件加速
class VideoBuffer : public HwVideoBuffer
{
public:
    using AVFrameUPtr = AVFrameUPtr;

    VideoBuffer(AVFrameUPtr frame, AVRational pixelAspectRatio = { 1, 1 });
    ~VideoBuffer() override;

    MapData map(::VideoFrame::MapMode mode) override;
    void unmap() override;

    VideoFrameTexturesUPtr mapTextures(QRhi &, VideoFrameTexturesUPtr& oldTextures) override;

    VideoFrameFormat::PixelFormat pixelFormat() const;
    QSize size() const;

    static VideoFrameFormat::PixelFormat toQtPixelFormat(AVPixelFormat avPixelFormat, bool *needsConversion = nullptr);
    static AVPixelFormat toAVPixelFormat(VideoFrameFormat::PixelFormat pixelFormat);

    void convertSWFrame();

    AVFrame *getHWFrame() const { return m_hwFrame.get(); }

    void initTextureConverter(QRhi &rhi) override;

    QRhi *rhi() const override;

    VideoFrameFormat::ColorSpace colorSpace() const;
    VideoFrameFormat::ColorTransfer colorTransfer() const;
    VideoFrameFormat::ColorRange colorRange() const;

    float maxNits();

private:
    VideoFrameFormat::PixelFormat m_pixelFormat;
    AVFrame *m_frame = nullptr;
    AVFrameUPtr m_hwFrame;
    AVFrameUPtr m_swFrame;
    QSize m_size;
    ::VideoFrame::MapMode m_mode = ::VideoFrame::NotMapped;
    bool m_isSWZeroCopy = false;

    SwsContextUPtr m_cachedSwsContext;
    int m_cachedSwsSrcW = 0;
    int m_cachedSwsSrcH = 0;
    AVPixelFormat m_cachedSwsSrcFmt = AV_PIX_FMT_NONE;
    int m_cachedSwsDstW = 0;
    int m_cachedSwsDstH = 0;
    AVPixelFormat m_cachedSwsDstFmt = AV_PIX_FMT_NONE;

    TextureConverter &ensureTextureConverter(QRhi &rhi);

    VideoFrameTexturesUPtr createTexturesFromHwFrame(QRhi &, VideoFrameTexturesUPtr& oldTextures);

#ifdef Q_OS_WINDOWS
    VideoFrameTexturesUPtr createTexturesFromSWZeroCopyFrame(QRhi &, VideoFrameTexturesUPtr& oldTextures);
#endif
};
}
QT_END_NAMESPACE

#endif
