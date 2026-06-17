#include "FFmpegHwAccel_d3d11_p.h"
#include "playbackengine/FFmpegStreamDecoder_p.h"

#include <VideoFrameFormat.h>
#include "FFmpegVideoBuffer_p.h"

#include <private/VideoTextureHelper_p.h>
#include <QtCore/private/qcomptr_p.h>
#include <private/quniquehandle_p.h>

#include <rhi/qrhi.h>
#include <d3d10.h>

#include <qdebug.h>
import qzLog;

#include <libavutil/hwcontext_d3d11va.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>

QT_BEGIN_NAMESPACE

namespace {

qz::Log::LogCategory qLcMediaFFmpegHWAccel("qz.multimedia.hwaccel");

ComPtr<ID3D11Device1> GetD3DDevice(QRhi *rhi)
{
    const auto native = static_cast<const QRhiD3D11NativeHandles *>(rhi->nativeHandles());
    if (!native)
        return {};

    const ComPtr<ID3D11Device> rhiDevice = static_cast<ID3D11Device *>(native->dev);

    ComPtr<ID3D11Device1> dev1;
    if (rhiDevice.As(&dev1) != S_OK)
        return nullptr;

    return dev1;
}

ComPtr<ID3D11Texture2D> getAvFrameTexture(const AVFrame *frame)
{
    return reinterpret_cast<ID3D11Texture2D *>(frame->data[0]);
}

int getAvFramePoolIndex(const AVFrame *frame)
{
    return static_cast<int>(reinterpret_cast<intptr_t>(frame->data[1]));
}

const AVD3D11VADeviceContext *getHwDeviceContext(const AVHWDeviceContext *ctx)
{
    return static_cast<AVD3D11VADeviceContext *>(ctx->hwctx);
}

void freeTextureAndData(void *opaque, uint8_t *data)
{
    static_cast<ID3D11Texture2D *>(opaque)->Release();
    av_free(data);
}

AVBufferRef *wrapTextureAsBuffer(const ComPtr<ID3D11Texture2D> &tex)
{
    AVD3D11FrameDescriptor *avFrameDesc =
            static_cast<AVD3D11FrameDescriptor *>(av_mallocz(sizeof(AVD3D11FrameDescriptor)));
    avFrameDesc->index = 0;
    avFrameDesc->texture = tex.Get();

    return av_buffer_create(reinterpret_cast<uint8_t *>(avFrameDesc),
                            sizeof(AVD3D11FrameDescriptor *), freeTextureAndData, tex.Get(), 0);
}

ComPtr<ID3D11Texture2D> copyTexture(const AVD3D11VADeviceContext *hwDevCtx, const AVFrame *src)
{
    const int poolIndex = getAvFramePoolIndex(src);
    const ComPtr<ID3D11Texture2D> poolTex = getAvFrameTexture(src);

    D3D11_TEXTURE2D_DESC texDesc{};
    poolTex->GetDesc(&texDesc);

    texDesc.ArraySize = 1;
    texDesc.MiscFlags = 0;
    texDesc.BindFlags = 0;

    ComPtr<ID3D11Texture2D> destTex;
    if (hwDevCtx->device->CreateTexture2D(&texDesc, nullptr, &destTex) != S_OK) {
        qz::Log::cat_critical(qLcMediaFFmpegHWAccel, "Unable to copy frame from decoder pool");
        return {};
    }

    hwDevCtx->device_context->CopySubresourceRegion(destTex.Get(), 0, 0, 0, 0, poolTex.Get(),
                                                    poolIndex, nullptr);

    return destTex;
}

}
namespace ffmpeg {

bool TextureBridge::copyToSharedTex(ID3D11Device *dev, ID3D11DeviceContext *ctx,
                                    const ComPtr<ID3D11Texture2D> &tex, UINT index,
                                    const QSize &frameSize)
{
    if (!ensureSrcTex(dev, tex, frameSize))
        return false;

    ctx->Flush();

    if (m_srcMutex->AcquireSync(m_srcKey, INFINITE) != S_OK)
        return false;

    const UINT width = static_cast<UINT>(frameSize.width());
    const UINT height = static_cast<UINT>(frameSize.height());

    const D3D11_BOX crop{ 0, 0, 0, width, height, 1 };
    ctx->CopySubresourceRegion(m_srcTex.Get(), 0, 0, 0, 0, tex.Get(), index, &crop);

    m_srcMutex->ReleaseSync(m_destKey);
    return true;
}

ComPtr<ID3D11Texture2D> TextureBridge::copyFromSharedTex(const ComPtr<ID3D11Device1> &dev,
                                                         const ComPtr<ID3D11DeviceContext> &ctx)
{
    if (!ensureDestTex(dev))
        return {};

    if (m_destMutex->AcquireSync(m_destKey, INFINITE) != S_OK)
        return {};

    ctx->CopySubresourceRegion(m_outputTex.Get(), 0, 0, 0, 0, m_destTex.Get(), 0, nullptr);

    m_destMutex->ReleaseSync(m_srcKey);

    return m_outputTex;
}

bool TextureBridge::ensureDestTex(const ComPtr<ID3D11Device1> &dev)
{
    if (m_destDevice.Get() != dev.Get()) {

        m_destTex = nullptr;
        m_destDevice = dev;
    }

    if (m_destTex)
        return true;

    if (m_destDevice->OpenSharedResource1(m_sharedHandle.get(), IID_PPV_ARGS(&m_destTex)) != S_OK)
        return false;

    CD3D11_TEXTURE2D_DESC desc{};
    m_destTex->GetDesc(&desc);

    desc.MiscFlags = 0;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    if (m_destDevice->CreateTexture2D(&desc, nullptr, m_outputTex.ReleaseAndGetAddressOf()) != S_OK)
        return false;

    if (m_destTex.As(&m_destMutex) != S_OK)
        return false;

    return true;
}

bool TextureBridge::ensureSrcTex(ID3D11Device *dev, const ComPtr<ID3D11Texture2D> &tex, const QSize &frameSize)
{
    if (!isSrcInitialized(dev, tex, frameSize))
        return recreateSrc(dev, tex, frameSize);

    return true;
}

bool TextureBridge::isSrcInitialized(const ID3D11Device *dev,
                                     const ComPtr<ID3D11Texture2D> &tex,
                                     const QSize &frameSize) const
{
    if (!m_srcTex)
        return false;

    ComPtr<ID3D11Device> texDevice;
    m_srcTex->GetDevice(texDevice.GetAddressOf());
    if (dev != texDevice.Get())
        return false;

    CD3D11_TEXTURE2D_DESC inputDesc{};
    tex->GetDesc(&inputDesc);

    CD3D11_TEXTURE2D_DESC currentDesc{};
    m_srcTex->GetDesc(&currentDesc);

    if (inputDesc.Format != currentDesc.Format)
        return false;

    const UINT width = static_cast<UINT>(frameSize.width());
    const UINT height = static_cast<UINT>(frameSize.height());

    if (currentDesc.Width != width || currentDesc.Height != height)
        return false;

    return true;
}

bool TextureBridge::recreateSrc(ID3D11Device *dev, const ComPtr<ID3D11Texture2D> &tex, const QSize &frameSize)
{
    m_sharedHandle.close();

    CD3D11_TEXTURE2D_DESC desc{};
    tex->GetDesc(&desc);

    const UINT width = static_cast<UINT>(frameSize.width());
    const UINT height = static_cast<UINT>(frameSize.height());

    CD3D11_TEXTURE2D_DESC texDesc{ desc.Format, width, height };
    texDesc.MipLevels = 1;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;

    if (dev->CreateTexture2D(&texDesc, nullptr, m_srcTex.ReleaseAndGetAddressOf()) != S_OK)
        return false;

    ComPtr<IDXGIResource1> res;
    if (m_srcTex.As(&res) != S_OK)
        return false;

    const HRESULT hr =
            res->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ, nullptr, &m_sharedHandle);

    if (hr != S_OK || !m_sharedHandle)
        return false;

    if (m_srcTex.As(&m_srcMutex) != S_OK || !m_srcMutex)
        return false;

    m_destTex = nullptr;
    m_destMutex = nullptr;
    return true;
}

namespace {
class D3D11TextureHandles : public VideoFrameTexturesHandles
{
public:
    D3D11TextureHandles(TextureConverterBackendPtr &&converterBackend, QRhi *rhi,
                        ComPtr<ID3D11Texture2D> &&tex)
        : m_parentConverterBackend(std::move(converterBackend)), m_owner{ rhi }, m_tex(std::move(tex))
    {
    }

    quint64 textureHandle(QRhi &rhi, int ) override
    {
        if (&rhi != m_owner)
            return 0u;
        return reinterpret_cast<qint64>(m_tex.Get());
    }

private:
    TextureConverterBackendPtr m_parentConverterBackend;
    QRhi *m_owner = nullptr;
    ComPtr<ID3D11Texture2D> m_tex;
};
}

D3D11TextureConverter::D3D11TextureConverter(QRhi *rhi)
    : TextureConverterBackend(rhi), m_rhiDevice{ GetD3DDevice(rhi) }
{
    if (!m_rhiDevice)
        return;

    m_rhiDevice->GetImmediateContext(m_rhiCtx.GetAddressOf());
}

VideoFrameTexturesHandlesUPtr
D3D11TextureConverter::createTextureHandles(AVFrame *frame,
                                            VideoFrameTexturesHandlesUPtr )
{
    if (!m_rhiDevice)
        return nullptr;

    if (!frame || !frame->hw_frames_ctx || frame->format != AV_PIX_FMT_D3D11)
        return nullptr;

    const auto *ctx = avFrameDeviceContext(frame);

    if (!ctx || ctx->type != AV_HWDEVICE_TYPE_D3D11VA)
        return nullptr;

    const ComPtr<ID3D11Texture2D> ffmpegTex = getAvFrameTexture(frame);
    const int index = getAvFramePoolIndex(frame);

    if (rhi->backend() == QRhi::D3D11) {
        const auto *avDeviceCtx = getHwDeviceContext(ctx);

        if (!avDeviceCtx)
            return nullptr;

        bool sameDevice = false;
        {
            ComPtr<ID3D11Device> ffmpegDevice;
            ffmpegTex->GetDevice(ffmpegDevice.GetAddressOf());
            sameDevice = (ffmpegDevice.Get() == m_rhiDevice.Get());

            qz::Log::cat_debug(qLcMediaFFmpegHWAccel, "Device comparison: ffmpegDevice={} rhiDevice={} sameDevice={}", static_cast<const void*>(ffmpegDevice.Get()), static_cast<const void*>(m_rhiDevice.Get()), sameDevice);
        }

        if (sameDevice) {
            qz::Log::cat_debug(qLcMediaFFmpegHWAccel, "Using same device optimization - direct texture access (no TextureBridge needed)");

            if (avDeviceCtx->lock && avDeviceCtx->lock_ctx)
                avDeviceCtx->lock(avDeviceCtx->lock_ctx);
            QScopeGuard autoUnlock([&] {
                if (avDeviceCtx->unlock && avDeviceCtx->lock_ctx)
                    avDeviceCtx->unlock(avDeviceCtx->lock_ctx);
            });

            D3D11_TEXTURE2D_DESC srcDesc{};
            ffmpegTex->GetDesc(&srcDesc);

            const UINT width = static_cast<UINT>(frame->width);
            const UINT height = static_cast<UINT>(frame->height);

            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = width;
            desc.Height = height;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = srcDesc.Format;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.MiscFlags = 0;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.CPUAccessFlags = 0;

            ComPtr<ID3D11Texture2D> outputTex;
            HRESULT hr = m_rhiDevice->CreateTexture2D(&desc, nullptr, &outputTex);

            if (hr != S_OK) {
                qz::Log::cat_warn(qLcMediaFFmpegHWAccel, "Failed to create output texture");
                return nullptr;
            }

            const D3D11_BOX crop{ 0, 0, 0, width, height, 1 };
            m_rhiCtx->CopySubresourceRegion(outputTex.Get(), 0, 0, 0, 0,
                                            ffmpegTex.Get(), index, &crop);

            return std::make_unique<D3D11TextureHandles>(shared_from_this(), rhi, std::move(outputTex));
        }

        qz::Log::cat_debug(qLcMediaFFmpegHWAccel, "Using cross-device texture sharing (TextureBridge)");

        if (avDeviceCtx->lock && avDeviceCtx->lock_ctx)
            avDeviceCtx->lock(avDeviceCtx->lock_ctx);
        QScopeGuard autoUnlock([&] {
            if (avDeviceCtx->unlock && avDeviceCtx->lock_ctx)
                avDeviceCtx->unlock(avDeviceCtx->lock_ctx);
        });

        QSize frameSize{ frame->width, frame->height };
        if (!m_bridge.copyToSharedTex(avDeviceCtx->device, avDeviceCtx->device_context,
                                      ffmpegTex, index, frameSize)) {
            return nullptr;
        }

        ComPtr<ID3D11Texture2D> output = m_bridge.copyFromSharedTex(m_rhiDevice, m_rhiCtx);

        if (!output)
            return nullptr;

        return std::make_unique<D3D11TextureHandles>(shared_from_this(), rhi, std::move(output));
    }

    return nullptr;
}

void D3D11TextureConverter::SetupDecoderTextures(AVCodecContext *s)
{
    int ret = avcodec_get_hw_frames_parameters(s, s->hw_device_ctx, AV_PIX_FMT_D3D11,
                                               &s->hw_frames_ctx);
    if (ret < 0) {
        qz::Log::cat_debug(qLcMediaFFmpegHWAccel, "Failed to allocate HW frames context {}", ret);
        return;
    }

    const auto *frames_ctx = reinterpret_cast<const AVHWFramesContext *>(s->hw_frames_ctx->data);
    auto *hwctx = static_cast<AVD3D11VAFramesContext *>(frames_ctx->hwctx);
    hwctx->MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    hwctx->BindFlags = D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE;
    ret = av_hwframe_ctx_init(s->hw_frames_ctx);
    if (ret < 0) {
        qz::Log::cat_debug(qLcMediaFFmpegHWAccel, "Failed to initialize HW frames context {}", ret);
        av_buffer_unref(&s->hw_frames_ctx);
    }
}

AVFrameUPtr copyFromHwPoolD3D11(AVFrameUPtr src)
{
    if (!src || !src->hw_frames_ctx || src->format != AV_PIX_FMT_D3D11)
        return src;

    const AVHWDeviceContext *avDevCtx = avFrameDeviceContext(src.get());
    if (!avDevCtx || avDevCtx->type != AV_HWDEVICE_TYPE_D3D11VA)
        return src;

    AVFrameUPtr dest = makeAVFrame();
    if (av_frame_copy_props(dest.get(), src.get()) != 0) {
        qz::Log::cat_critical(qLcMediaFFmpegHWAccel, "Unable to copy frame from decoder pool");
        return src;
    }

    const AVD3D11VADeviceContext *hwDevCtx = getHwDeviceContext(avDevCtx);
    ComPtr<ID3D11Texture2D> destTex;
    {
        if (hwDevCtx->lock && hwDevCtx->lock_ctx)
            hwDevCtx->lock(hwDevCtx->lock_ctx);
        destTex = copyTexture(hwDevCtx, src.get());
        if (hwDevCtx->unlock && hwDevCtx->lock_ctx)
            hwDevCtx->unlock(hwDevCtx->lock_ctx);
    }

    if (!destTex) {
        qz::Log::cat_warn(qLcMediaFFmpegHWAccel, "Failed to copy texture from HW pool");
        return src;
    }

    dest->buf[0] = wrapTextureAsBuffer(destTex);
    dest->data[0] = reinterpret_cast<uint8_t *>(destTex.Detach());
    dest->data[1] = reinterpret_cast<uint8_t *>(0);

    dest->width = src->width;
    dest->height = src->height;
    dest->format = src->format;
    dest->hw_frames_ctx = av_buffer_ref(src->hw_frames_ctx);

    return dest;
}

}

ffmpeg::AVBufferUPtr ffmpeg::createD3D11DeviceContextFromRhi(QRhi *rhi)
{
    if (!rhi || rhi->backend() != QRhi::D3D11)
        return {};

    const auto native = static_cast<const QRhiD3D11NativeHandles *>(rhi->nativeHandles());
    if (!native || !native->dev)
        return {};

    ID3D11Device *device = static_cast<ID3D11Device *>(native->dev);
    ID3D11DeviceContext *deviceContext = static_cast<ID3D11DeviceContext *>(native->context);

    ComPtr<ID3D10Multithread> multithread;
    if (SUCCEEDED(deviceContext->QueryInterface(IID_PPV_ARGS(&multithread)))) {
        multithread->SetMultithreadProtected(TRUE);
        qz::Log::cat_debug(qLcMediaFFmpegHWAccel, "Enabled D3D11 multithread protection");
    } else {
        qz::Log::cat_warn(qLcMediaFFmpegHWAccel, "Failed to enable D3D11 multithread protection");
    }

    AVBufferRef *hwDeviceCtx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
    if (!hwDeviceCtx) {
        qz::Log::cat_warn(qLcMediaFFmpegHWAccel, "Failed to allocate D3D11VA device context");
        return {};
    }

    AVHWDeviceContext *ctx = reinterpret_cast<AVHWDeviceContext *>(hwDeviceCtx->data);
    AVD3D11VADeviceContext *d3d11Ctx = static_cast<AVD3D11VADeviceContext *>(ctx->hwctx);

    device->AddRef();
    deviceContext->AddRef();

    d3d11Ctx->device = device;
    d3d11Ctx->device_context = deviceContext;

    d3d11Ctx->lock = nullptr;
    d3d11Ctx->unlock = nullptr;
    d3d11Ctx->lock_ctx = nullptr;

    int ret = av_hwdevice_ctx_init(hwDeviceCtx);
    if (ret < 0) {
        qz::Log::cat_warn(qLcMediaFFmpegHWAccel, "Failed to initialize D3D11VA device context {}", ret);
        device->Release();
        deviceContext->Release();
        av_buffer_unref(&hwDeviceCtx);
        return {};
    }

    qz::Log::cat_debug(qLcMediaFFmpegHWAccel, "Successfully created D3D11VA device context from RHI device");
    return AVBufferUPtr(hwDeviceCtx);
}

QT_END_NAMESPACE
