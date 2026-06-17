#include <qzFFmpegMediaPluginImpl/private/FFmpegHwAccel_MediaCodec_p.h>

#include <QtGui/rhi/qrhi.h>

#include "AndroidSurfaceTexture_p.h"

extern "C" {
#include <libavcodec/mediacodec.h>
#include <libavutil/hwcontext_mediacodec.h>
}

#if !defined(Q_OS_ANDROID)
#    error "Configuration error"
#endif

import qzLog;

QT_BEGIN_NAMESPACE

namespace ffmpeg {

namespace {

class MediaCodecTextureHandles : public VideoFrameTexturesHandles
{
public:
    MediaCodecTextureHandles(TextureConverterBackendPtr &&converterBackend, quint64 textureHandle)
        : m_parentConverterBackend(std::move(converterBackend)), m_handle(textureHandle)
    {
    }

    quint64 textureHandle(QRhi &, int plane) override { return (plane == 0) ? m_handle : 0; }

private:
    TextureConverterBackendPtr m_parentConverterBackend; // ensures the backend is kept alive
    quint64 m_handle;
};

void deleteSurface(AVHWDeviceContext *ctx)
{
    AndroidSurfaceTexture *s = reinterpret_cast<AndroidSurfaceTexture *>(ctx->user_opaque);
    delete s;
}

AndroidSurfaceTexture *getTextureSurface(AVFrame *frame)
{
    if (!frame || !frame->hw_frames_ctx)
        return nullptr;

    auto *frameContext = reinterpret_cast<AVHWFramesContext *>(frame->hw_frames_ctx->data);

    if (!frameContext || !frameContext->device_ctx)
        return nullptr;

    AVHWDeviceContext *deviceContext = frameContext->device_ctx;

    return reinterpret_cast<AndroidSurfaceTexture *>(deviceContext->user_opaque);
}

} // namespace

void MediaCodecTextureConverter::setupDecoderSurface(AVCodecContext *avCodecContext)
{
    std::unique_ptr<AndroidSurfaceTexture> androidSurfaceTexture(new AndroidSurfaceTexture(0));
    if (!androidSurfaceTexture->isValid()) {
        qz::Log::warn("Failed to create AndroidSurfaceTexture for MediaCodec");
        return;
    }

    AVMediaCodecContext *mediacodecContext = av_mediacodec_alloc_context();
    if (!mediacodecContext) {
        qz::Log::warn("Failed to allocate AVMediaCodecContext");
        return;
    }

    int ret = av_mediacodec_default_init(avCodecContext, mediacodecContext,
                                          androidSurfaceTexture->surface());
    if (ret < 0) {
        qz::Log::warn("av_mediacodec_default_init failed: {} {}", ret, strerror(-ret));
        return;
    }

    if (!avCodecContext->hw_device_ctx || !avCodecContext->hw_device_ctx->data) {
        qz::Log::warn("hw_device_ctx is not set after av_mediacodec_default_init");
        return;
    }

    AVHWDeviceContext *deviceContext =
            reinterpret_cast<AVHWDeviceContext *>(avCodecContext->hw_device_ctx->data);

    if (!deviceContext->hwctx)
        return;

    AVMediaCodecDeviceContext *mediaDeviceContext =
            reinterpret_cast<AVMediaCodecDeviceContext *>(deviceContext->hwctx);

    if (!mediaDeviceContext)
        return;

    mediaDeviceContext->surface = androidSurfaceTexture->surface();

    Q_ASSERT(deviceContext->user_opaque == nullptr);
    deviceContext->user_opaque = androidSurfaceTexture.release();
    deviceContext->free = deleteSurface;
}

VideoFrameTexturesUPtr
MediaCodecTextureConverter::createTextures(AVFrame * /*frame*/,
                                           VideoFrameTexturesUPtr & /*oldTextures*/)
{
    // Most likely, this method should be used instead of createTextureHandles, QRhiTexture is
    // already created, and we don't need to convert it to handle and back. This work should be done
    // in the scope of QTBUG-132174
    return nullptr;
}

VideoFrameTexturesHandlesUPtr
MediaCodecTextureConverter::createTextureHandles(AVFrame *frame,
                                                 VideoFrameTexturesHandlesUPtr /*oldHandles*/)
{
    // TODO: reuse oldHandles: the underlying QRhiTexture must be taken from it instead of
    // keeping it in AndroidTextureConverter. See QTBUG-132174
    AndroidSurfaceTexture *androidSurfaceTexture = getTextureSurface(frame);

    if (!androidSurfaceTexture || !androidSurfaceTexture->isValid()) {
        qz::Log::warn("MediaCodec: no valid AndroidSurfaceTexture for frame");
        return {};
    }

    if (!externalTexture || m_currentSurfaceIndex != androidSurfaceTexture->index()) {
        m_currentSurfaceIndex = androidSurfaceTexture->index();
        androidSurfaceTexture->detachFromGLContext();
        externalTexture = std::unique_ptr<QRhiTexture>(
                rhi->newTexture(QRhiTexture::Format::RGBA8, { frame->width, frame->height }, 1,
                                QRhiTexture::ExternalOES));

        if (!externalTexture->create()) {
            qz::Log::warn("Failed to create the external texture!");
            return {};
        }

        quint64 textureHandle = externalTexture->nativeTexture().object;
        androidSurfaceTexture->attachToGLContext(textureHandle);
    }

    // release a MediaCodec buffer and render it to the surface
    AVMediaCodecBuffer *buffer = (AVMediaCodecBuffer *)frame->data[3];

    if (!buffer) {
        qz::Log::warn("Received a frame without AVMediaCodecBuffer.");
    } else if (av_mediacodec_release_buffer(buffer, 1) < 0) {
        qz::Log::warn("Failed to render buffer to surface.");
        return {};
    }

    androidSurfaceTexture->updateTexImage();

    return std::make_unique<MediaCodecTextureHandles>(shared_from_this(),
                                                      externalTexture->nativeTexture().object);
}

} // namespace ffmpeg

QT_END_NAMESPACE
