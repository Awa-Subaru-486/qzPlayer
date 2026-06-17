#ifndef FFMPEGHWACCEL_MEDIACODEC_P_H
#define FFMPEGHWACCEL_MEDIACODEC_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegHwAccel_p.h>
#include <memory>
#include <rhi/qrhi.h>

#include <QtCore/qtconfigmacros.h>

QT_BEGIN_NAMESPACE

namespace ffmpeg {

// Android MediaCodec 纹理转换器
class MediaCodecTextureConverter : public TextureConverterBackend
{
public:
    MediaCodecTextureConverter(QRhi *rhi) : TextureConverterBackend(rhi){};

    VideoFrameTexturesUPtr createTextures(AVFrame *frame,
                                           VideoFrameTexturesUPtr &oldTextures) override;

    VideoFrameTexturesHandlesUPtr
    createTextureHandles(AVFrame *frame, VideoFrameTexturesHandlesUPtr oldHandles) override;

    static void setupDecoderSurface(AVCodecContext *s);
private:
    std::unique_ptr<QRhiTexture> externalTexture;
    quint64 m_currentSurfaceIndex = 0;
};

} // namespace ffmpeg

QT_END_NAMESPACE

#endif // FFMPEGHWACCEL_MEDIACODEC_P_H
