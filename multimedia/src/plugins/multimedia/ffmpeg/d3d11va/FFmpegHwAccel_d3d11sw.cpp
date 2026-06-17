#include "FFmpegHwAccel_d3d11sw_p.h"

#ifdef Q_OS_WINDOWS

#include "FFmpegHwAccel_d3d11_p.h"
#include <private/VideoTextureHelper_p.h>
#include <rhi/qrhi.h>
import qzLog;
#include <QtCore/qmutex.h>

extern "C" {
#include <libavutil/pixdesc.h>
}

QT_BEGIN_NAMESPACE

namespace {

qz::Log::LogCategory qLcD3D11SWZeroCopy("qz.multimedia.d3d11.swzerocopy");

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

struct PlaneFormat
{
    DXGI_FORMAT format;
    int planeCount;
    int bytesPerPixel[3];
};

PlaneFormat getPlaneFormat(AVPixelFormat format)
{
    PlaneFormat result{};

    switch (format) {
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
        result.format = DXGI_FORMAT_R8_UNORM;
        result.planeCount = 3;
        result.bytesPerPixel[0] = 1;
        result.bytesPerPixel[1] = 1;
        result.bytesPerPixel[2] = 1;
        break;
    case AV_PIX_FMT_YUV420P10:
    case AV_PIX_FMT_YUV420P12:
    case AV_PIX_FMT_YUV420P16:
        result.format = DXGI_FORMAT_R16_UNORM;
        result.planeCount = 3;
        result.bytesPerPixel[0] = 2;
        result.bytesPerPixel[1] = 2;
        result.bytesPerPixel[2] = 2;
        break;
    case AV_PIX_FMT_NV12:
        result.format = DXGI_FORMAT_R8_UNORM;
        result.planeCount = 2;
        result.bytesPerPixel[0] = 1;
        result.bytesPerPixel[1] = 2;
        break;
    case AV_PIX_FMT_P010:
    case AV_PIX_FMT_P016:
        result.format = DXGI_FORMAT_R16_UNORM;
        result.planeCount = 2;
        result.bytesPerPixel[0] = 2;
        result.bytesPerPixel[1] = 4;
        break;
    case AV_PIX_FMT_BGRA:
        result.format = DXGI_FORMAT_B8G8R8A8_UNORM;
        result.planeCount = 1;
        result.bytesPerPixel[0] = 4;
        break;
    case AV_PIX_FMT_RGBA:
        result.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        result.planeCount = 1;
        result.bytesPerPixel[0] = 4;
        break;
    default:
        result.format = DXGI_FORMAT_UNKNOWN;
        break;
    }

    return result;
}

struct SWZeroCopyFrameData
{
    ComPtr<ID3D11Texture2D> stagingTextures[3];
    D3D11_MAPPED_SUBRESOURCE mappedResources[3]{};
    QSize size;
    AVPixelFormat format = AV_PIX_FMT_NONE;
    int planeCount = 0;
    bool mapped = false;
    ID3D11DeviceContext *context = nullptr;
};

}

namespace ffmpeg {

namespace {

class D3D11SWTextureHandles : public VideoFrameTexturesHandles
{
public:
    D3D11SWTextureHandles(TextureConverterBackendPtr &&converterBackend, QRhi *rhi,
                          ComPtr<ID3D11Texture2D> &&tex)
        : m_parentConverterBackend(std::move(converterBackend))
        , m_owner{ rhi }
        , m_tex(std::move(tex))
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

struct CodecContextSWData
{
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    std::vector<std::shared_ptr<SWZeroCopyFrameData>> framePool;
    int maxPoolSize = 4;
};

}

D3D11SWTextureConverter::D3D11SWTextureConverter(QRhi *rhi)
    : TextureConverterBackend(rhi)
    , m_rhiDevice{ GetD3DDevice(rhi) }
{
    if (!m_rhiDevice)
        return;

    m_rhiDevice->GetImmediateContext(m_rhiCtx.GetAddressOf());
}

VideoFrameTexturesHandlesUPtr
D3D11SWTextureConverter::createTextureHandles(AVFrame *frame, VideoFrameTexturesHandlesUPtr oldHandles)
{
    if (!m_rhiDevice || !frame)
        return nullptr;

    if (!isD3D11SWZeroCopyFrame(frame))
        return nullptr;

    auto *frameData = reinterpret_cast<SWZeroCopyFrameData*>(frame->data[0]);
    if (!frameData || !frameData->stagingTextures[0])
        return nullptr;

    ComPtr<ID3D11Texture2D> outputTex;

    if (oldHandles) {
        quint64 handle = oldHandles->textureHandle(*rhi, 0);
        if (handle) {
            ID3D11Texture2D *existingTex = reinterpret_cast<ID3D11Texture2D*>(handle);
            D3D11_TEXTURE2D_DESC existingDesc{};
            existingTex->GetDesc(&existingDesc);

            if (existingDesc.Width == static_cast<UINT>(frame->width) &&
                existingDesc.Height == static_cast<UINT>(frame->height)) {
                outputTex = existingTex;
                outputTex->AddRef();
                qz::Log::cat_debug(qLcD3D11SWZeroCopy, "Reusing existing output texture");
            }
        }
    }

    if (!outputTex) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = frame->width;
        desc.Height = frame->height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_NV12;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.MiscFlags = 0;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;

        HRESULT hr = m_rhiDevice->CreateTexture2D(&desc, nullptr, &outputTex);

        if (hr != S_OK) {
            qz::Log::cat_warn(qLcD3D11SWZeroCopy, "Failed to create output texture: {:x}", static_cast<unsigned>(hr));
            return nullptr;
        }
        qz::Log::cat_debug(qLcD3D11SWZeroCopy, "Created new output texture");
    }

    if (frameData->mapped) {
        for (int i = 0; i < frameData->planeCount; ++i) {
            if (frameData->mappedResources[i].pData) {
                m_rhiCtx->Unmap(frameData->stagingTextures[i].Get(), 0);
                frameData->mappedResources[i] = {};
            }
        }
        frameData->mapped = false;
    }

    const UINT width = static_cast<UINT>(frame->width);
    const UINT height = static_cast<UINT>(frame->height);

    D3D11_TEXTURE2D_DESC yDesc{}, uvDesc{};
    frameData->stagingTextures[0]->GetDesc(&yDesc);

    if (frameData->planeCount >= 2 && frameData->stagingTextures[1]) {
        frameData->stagingTextures[1]->GetDesc(&uvDesc);
    }

    const UINT uvWidth = width / 2;
    const UINT uvHeight = height / 2;

    D3D11_BOX yBox{ 0, 0, 0, width, height, 1 };
    m_rhiCtx->CopySubresourceRegion(outputTex.Get(), 0, 0, 0, 0,
                                    frameData->stagingTextures[0].Get(), 0, &yBox);

    if (frameData->planeCount >= 2 && frameData->stagingTextures[1]) {
        D3D11_BOX uvBox{ 0, 0, 0, uvWidth, uvHeight, 1 };
        m_rhiCtx->CopySubresourceRegion(outputTex.Get(), 1, 0, 0, 0,
                                        frameData->stagingTextures[1].Get(), 0, &uvBox);
    }

    return std::make_unique<D3D11SWTextureHandles>(shared_from_this(), rhi, std::move(outputTex));
}

int d3d11SWGetBuffer2(AVCodecContext *codecCtx, AVFrame *frame, int )
{
    auto *data = static_cast<CodecContextSWData*>(codecCtx->opaque);
    if (!data || !data->device || !data->context) {
        return avcodec_default_get_buffer2(codecCtx, frame, 0);
    }

    int linesizeAligns[AV_NUM_DATA_POINTERS] = {};
    avcodec_align_dimensions2(codecCtx, &frame->width, &frame->height, linesizeAligns);

    const int lineSizeAlign = linesizeAligns[0];
    int w = FFALIGN(frame->width, lineSizeAlign);
    int h = frame->height;

    uint32_t paddingHeight = h - codecCtx->height + 1;

    QSize textureSize(w, h + paddingHeight);
    PlaneFormat planeFormat = getPlaneFormat(codecCtx->pix_fmt);

    qz::Log::cat_debug(qLcD3D11SWZeroCopy, "Creating staging texture: size={} pix_fmt={} ({}) planeCount={}", textureSize, static_cast<int>(codecCtx->pix_fmt), av_get_pix_fmt_name(codecCtx->pix_fmt), planeFormat.planeCount);

    if (planeFormat.format == DXGI_FORMAT_UNKNOWN) {
        qz::Log::cat_warn(qLcD3D11SWZeroCopy, "Unsupported pixel format: {} ({})", static_cast<int>(codecCtx->pix_fmt), av_get_pix_fmt_name(codecCtx->pix_fmt));
        return avcodec_default_get_buffer2(codecCtx, frame, 0);
    }

    std::shared_ptr<SWZeroCopyFrameData> frameData;

    for (auto &fd : data->framePool) {
        if (fd.use_count() == 1 && fd->size == textureSize && fd->format == codecCtx->pix_fmt) {
            frameData = fd;
            if (frameData->mapped && frameData->context) {
                for (int i = 0; i < frameData->planeCount; ++i) {
                    if (frameData->mappedResources[i].pData) {
                        frameData->context->Unmap(frameData->stagingTextures[i].Get(), 0);
                        frameData->mappedResources[i] = {};
                    }
                }
                frameData->mapped = false;
            }
            break;
        }
    }

    if (!frameData) {
        if (static_cast<int>(data->framePool.size()) >= data->maxPoolSize) {
            for (auto it = data->framePool.begin(); it != data->framePool.end(); ++it) {
                if (it->use_count() == 1) {
                    data->framePool.erase(it);
                    break;
                }
            }
        }

        frameData = std::make_shared<SWZeroCopyFrameData>();
        frameData->size = textureSize;
        frameData->format = codecCtx->pix_fmt;
        frameData->context = data->context.Get();
        frameData->planeCount = planeFormat.planeCount;

        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(codecCtx->pix_fmt);
        if (!desc) {
            qz::Log::cat_warn(qLcD3D11SWZeroCopy, "Failed to get pixel format descriptor");
            return avcodec_default_get_buffer2(codecCtx, frame, 0);
        }

        bool success = true;
        for (int plane = 0; plane < planeFormat.planeCount && success; ++plane) {
            int planeWidth = textureSize.width();
            int planeHeight = textureSize.height();

            if (plane > 0) {
                planeWidth = AV_CEIL_RSHIFT(textureSize.width(), desc->log2_chroma_w);
                planeHeight = AV_CEIL_RSHIFT(textureSize.height(), desc->log2_chroma_h);
            }

            D3D11_TEXTURE2D_DESC stagingDesc{};
            stagingDesc.Width = planeWidth;
            stagingDesc.Height = planeHeight;
            stagingDesc.MipLevels = 1;
            stagingDesc.ArraySize = 1;
            stagingDesc.Format = planeFormat.format;
            stagingDesc.SampleDesc.Count = 1;
            stagingDesc.SampleDesc.Quality = 0;
            stagingDesc.Usage = D3D11_USAGE_STAGING;
            stagingDesc.BindFlags = 0;
            stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            stagingDesc.MiscFlags = 0;

            HRESULT hr = data->device->CreateTexture2D(&stagingDesc, nullptr, &frameData->stagingTextures[plane]);
            if (FAILED(hr)) {
                qz::Log::cat_warn(qLcD3D11SWZeroCopy, "Failed to create staging texture for plane {} : {:x} width={} height={} format={}", plane, static_cast<unsigned>(hr), stagingDesc.Width, stagingDesc.Height, static_cast<int>(stagingDesc.Format));
                success = false;
            }
        }

        if (!success) {
            return avcodec_default_get_buffer2(codecCtx, frame, 0);
        }

        data->framePool.push_back(frameData);
        qz::Log::cat_debug(qLcD3D11SWZeroCopy, "Created new staging textures, pool size: {}", data->framePool.size());
    }

    bool mapSuccess = true;
    for (int plane = 0; plane < frameData->planeCount && mapSuccess; ++plane) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr = data->context->Map(frameData->stagingTextures[plane].Get(), 0,
                                         D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (FAILED(hr)) {
            mapSuccess = false;
        } else {
            frameData->mappedResources[plane] = mapped;
        }
    }

    if (!mapSuccess) {
        for (int i = 0; i < frameData->planeCount; ++i) {
            if (frameData->mappedResources[i].pData) {
                data->context->Unmap(frameData->stagingTextures[i].Get(), 0);
                frameData->mappedResources[i] = {};
            }
        }
        return avcodec_default_get_buffer2(codecCtx, frame, 0);
    }

    frameData->mapped = true;

    frame->data[0] = reinterpret_cast<uint8_t*>(frameData.get());
    frame->data[1] = nullptr;

    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(codecCtx->pix_fmt);
    if (desc) {
        for (int plane = 0; plane < frameData->planeCount && plane < AV_NUM_DATA_POINTERS; ++plane) {
            frame->data[plane] = static_cast<uint8_t*>(frameData->mappedResources[plane].pData);
            frame->linesize[plane] = frameData->mappedResources[plane].RowPitch;
        }
    }

    frame->buf[0] = av_buffer_create(
        reinterpret_cast<uint8_t*>(frameData.get()),
        sizeof(void*),
        [](void *opaque, uint8_t *data) {
            Q_UNUSED(data);
            delete static_cast<std::shared_ptr<SWZeroCopyFrameData>*>(opaque);
        },
        new std::shared_ptr<SWZeroCopyFrameData>(frameData),
        0
    );

    frame->extended_data = frame->data;

    return 0;
}

void setD3D11SWTexturePoolForContext(AVCodecContext *codecCtx,
                                      std::shared_ptr<D3D11SWTexturePool> pool)
{
    Q_UNUSED(pool);

    if (!codecCtx)
        return;

    if (codecCtx->opaque) {
        auto *oldData = reinterpret_cast<CodecContextSWData*>(codecCtx->opaque);
        delete oldData;
    }

    auto *data = new CodecContextSWData();
    data->device = static_cast<ID3D11Device*>(nullptr);
    data->context = static_cast<ID3D11DeviceContext*>(nullptr);

    codecCtx->opaque = data;
}

void initD3D11SWContext(AVCodecContext *codecCtx, ID3D11Device *device, ID3D11DeviceContext *context)
{
    if (!codecCtx)
        return;

    auto *data = reinterpret_cast<CodecContextSWData*>(codecCtx->opaque);
    if (!data) {
        data = new CodecContextSWData();
        codecCtx->opaque = data;
    }

    data->device = device;
    data->context = context;
}

std::shared_ptr<D3D11SWTexturePool> getD3D11SWTexturePoolForContext(AVCodecContext *codecCtx)
{
    Q_UNUSED(codecCtx);
    return nullptr;
}

bool isD3D11SWZeroCopyFrame(const AVFrame *frame)
{
    if (!frame || frame->format < 0)
        return false;

    return frame->buf[0] && frame->buf[0]->size == sizeof(void*);
}

D3D11SWTexturePool::D3D11SWTexturePool(ID3D11Device *device, ID3D11DeviceContext *context)
{
    Q_UNUSED(device);
    Q_UNUSED(context);
}

D3D11SWTexturePool::~D3D11SWTexturePool() = default;

bool D3D11SWTexturePool::isValid() const
{
    return true;
}

}

QT_END_NAMESPACE

#endif
