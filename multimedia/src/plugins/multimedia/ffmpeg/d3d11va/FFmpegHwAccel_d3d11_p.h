#ifndef FFMPEGHWACCEL_D3D11_P_H
#define FFMPEGHWACCEL_D3D11_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegHwAccel_p.h>
#include <QtCore/private/quniquehandle_types_p.h>
#include <QtCore/private/qcomptr_p.h>

#include <d3d11.h>
#include <d3d11_1.h>

#ifdef Q_OS_WINDOWS

QT_BEGIN_NAMESPACE

class QRhi;

namespace ffmpeg {

using SharedTextureHandle = QUniqueWin32NullHandle;

// D3D11 纹理桥接器，使用共享纹理跨设备拷贝
class TextureBridge final
{
public:

    bool copyToSharedTex(ID3D11Device *dev, ID3D11DeviceContext *ctx,
                         const ComPtr<ID3D11Texture2D> &tex, UINT index, const QSize &frameSize);

    ComPtr<ID3D11Texture2D> copyFromSharedTex(const ComPtr<ID3D11Device1> &dev,
                                              const ComPtr<ID3D11DeviceContext> &ctx);

private:
    bool ensureDestTex(const ComPtr<ID3D11Device1> &dev);
    bool ensureSrcTex(ID3D11Device *dev, const ComPtr<ID3D11Texture2D> &tex, const QSize &frameSize);
    bool isSrcInitialized(const ID3D11Device *dev, const ComPtr<ID3D11Texture2D> &tex, const QSize &frameSize) const;
    bool recreateSrc(ID3D11Device *dev, const ComPtr<ID3D11Texture2D> &tex, const QSize &frameSize);

    SharedTextureHandle m_sharedHandle{};

    const UINT m_srcKey = 0;
    ComPtr<ID3D11Texture2D> m_srcTex;
    ComPtr<IDXGIKeyedMutex> m_srcMutex;

    const UINT m_destKey = 1;
    ComPtr<ID3D11Device1> m_destDevice;
    ComPtr<ID3D11Texture2D> m_destTex;
    ComPtr<IDXGIKeyedMutex> m_destMutex;

    ComPtr<ID3D11Texture2D> m_outputTex;
};

// D3D11 硬件加速纹理转换器
class D3D11TextureConverter : public TextureConverterBackend
{
public:
    D3D11TextureConverter(QRhi *rhi);

    VideoFrameTexturesHandlesUPtr
    createTextureHandles(AVFrame *frame, VideoFrameTexturesHandlesUPtr oldHandles) override;

    static void SetupDecoderTextures(AVCodecContext *s);

private:
    ComPtr<ID3D11Device1> m_rhiDevice;
    ComPtr<ID3D11DeviceContext> m_rhiCtx;
    TextureBridge m_bridge;
};

AVFrameUPtr copyFromHwPoolD3D11(AVFrameUPtr src);

AVBufferUPtr createD3D11DeviceContextFromRhi(QRhi *rhi);

}

QT_END_NAMESPACE

#endif

#endif
