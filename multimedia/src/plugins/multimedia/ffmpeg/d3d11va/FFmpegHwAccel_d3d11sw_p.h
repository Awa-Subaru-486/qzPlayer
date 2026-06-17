#ifndef FFMPEGHWACCEL_D3D11SW_P_H
#define FFMPEGHWACCEL_D3D11SW_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegTextureConverter_p.h>
#include <QtCore/private/qcomptr_p.h>

#include <d3d11.h>
#include <d3d11_1.h>

#include <memory>

#ifdef Q_OS_WINDOWS

QT_BEGIN_NAMESPACE

class QRhi;

namespace ffmpeg {

class D3D11SWTexturePool
{
public:
    D3D11SWTexturePool(ID3D11Device *device, ID3D11DeviceContext *context);
    ~D3D11SWTexturePool();

    bool isValid() const;
};

// D3D11 软件零拷贝纹理转换器
class D3D11SWTextureConverter : public TextureConverterBackend
{
public:
    D3D11SWTextureConverter(QRhi *rhi);

    VideoFrameTexturesHandlesUPtr
    createTextureHandles(AVFrame *frame, VideoFrameTexturesHandlesUPtr oldHandles) override;

private:
    ComPtr<ID3D11Device1> m_rhiDevice;
    ComPtr<ID3D11DeviceContext> m_rhiCtx;
};

int d3d11SWGetBuffer2(AVCodecContext *codecCtx, AVFrame *frame, int flags);

void setD3D11SWTexturePoolForContext(AVCodecContext *codecCtx,
                                      std::shared_ptr<D3D11SWTexturePool> pool);

void initD3D11SWContext(AVCodecContext *codecCtx, ID3D11Device *device, ID3D11DeviceContext *context);

std::shared_ptr<D3D11SWTexturePool> getD3D11SWTexturePoolForContext(AVCodecContext *codecCtx);

bool isD3D11SWZeroCopyFrame(const AVFrame *frame);

}

QT_END_NAMESPACE

#endif

#endif
