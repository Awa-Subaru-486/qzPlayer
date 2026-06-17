#ifndef FFMPEGTEXTURECONVERTER_P_H
#define FFMPEGTEXTURECONVERTER_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegDefs_p.h>
#include <qzMultimedia/private/HwVideoBuffer_p.h>

#include <memory>

QT_BEGIN_NAMESPACE

class QRhi;

namespace ffmpeg {

// 纹理转换器后端基类，定义创建纹理的虚接口
class TextureConverterBackend : public std::enable_shared_from_this<TextureConverterBackend>
{
public:
    TextureConverterBackend(QRhi *rhi) : rhi(rhi) { }

    virtual ~TextureConverterBackend();

    // 创建纹理
    virtual VideoFrameTexturesUPtr createTextures(AVFrame * ,
                                                   VideoFrameTexturesUPtr & )
    {
        return nullptr;
    }

    // 创建纹理句柄
    virtual VideoFrameTexturesHandlesUPtr
    createTextureHandles(AVFrame * , VideoFrameTexturesHandlesUPtr )
    {
        return nullptr;
    }

    QRhi *rhi = nullptr;
};
using TextureConverterBackendPtr = std::shared_ptr<TextureConverterBackend>;

// 纹理转换器，用于将硬件解码的视频帧转换为 GPU 纹理。
class TextureConverter
{
public:

    TextureConverter(QRhi &rhi);

    // 初始化转换器
    bool init(AVFrame &hwFrame);

    // 创建纹理
    VideoFrameTexturesUPtr createTextures(AVFrame &hwFrame, VideoFrameTexturesUPtr &oldTextures);

    // 创建纹理句柄
    VideoFrameTexturesHandlesUPtr createTextureHandles(AVFrame &hwFrame,
                                                        VideoFrameTexturesHandlesUPtr oldHandles);

    bool isNull() const { return !m_backend || !m_backend->rhi; }

    // 应用解码器预设
    static void applyDecoderPreset(AVPixelFormat format, AVCodecContext &codecContext);

    // 检查硬件纹理转换是否启用
    static bool hwTextureConversionEnabled();

    // 检查后端是否可用
    static bool isBackendAvailable(AVFrame &hwFrame, const QRhi &rhi);

private:
    void updateBackend(AVPixelFormat format);

private:
    QRhi &m_rhi;
    AVPixelFormat m_format = AV_PIX_FMT_NONE;
    TextureConverterBackendPtr m_backend;
};

}

QT_END_NAMESPACE

#endif
