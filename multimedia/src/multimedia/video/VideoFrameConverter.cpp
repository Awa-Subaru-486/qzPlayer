// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.


#include "VideoFrameConverter_p.h"
#include "VideoFrameConversionHelper_p.h"
#include "VideoFrameFormat.h"
#include "VideoFrame_p.h"
#include "MultimediaUtils_p.h"
#include "ThreadLocalRhi_p.h"
#include "CachedValue_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qsize.h>
#include <QtCore/qhash.h>
#include <QtCore/qfile.h>
#include <QtGui/qimage.h>
import qzLog;

#include <qzMultimedia/private/MultimediaRanges_p.h>
#include <qzMultimedia/private/VideoTextureHelper_p.h>

#include <rhi/qrhi.h>

static qz::Log::LogCategory qLcVideoFrameConverter("qz.multimedia.video.frameconverter");

static constexpr float g_quad[] = {

    1.f, -1.f,   1.f, 1.f,
    1.f,  1.f,   1.f, 0.f,
   -1.f, -1.f,   0.f, 1.f,
   -1.f,  1.f,   0.f, 0.f,

    1.f, -1.f,   1.f, 0.f,
    1.f,  1.f,   0.f, 0.f,
   -1.f, -1.f,   1.f, 1.f,
   -1.f,  1.f,   0.f, 1.f,

    1.f, -1.f,   0.f, 0.f,
    1.f,  1.f,   0.f, 1.f,
   -1.f, -1.f,   1.f, 0.f,
   -1.f,  1.f,   1.f, 1.f,

    1.f, -1.f,  0.f, 1.f,
    1.f,  1.f,  1.f, 1.f,
   -1.f, -1.f,  0.f, 0.f,
   -1.f,  1.f,  1.f, 0.f,
};

static bool pixelFormatHasAlpha(VideoFrameFormat::PixelFormat format)
{
    switch (format) {
    case  VideoFrameFormat::Format_ARGB8888:
    case  VideoFrameFormat::Format_ARGB8888_Premultiplied:
    case  VideoFrameFormat::Format_BGRA8888:
    case  VideoFrameFormat::Format_BGRA8888_Premultiplied:
    case  VideoFrameFormat::Format_ABGR8888:
    case  VideoFrameFormat::Format_RGBA8888:
    case  VideoFrameFormat::Format_AYUV:
    case  VideoFrameFormat::Format_AYUV_Premultiplied:
        return true;
    default:
        return false;
    }
};

static QShader ensureShader(const QString &name)
{
    static CachedValueMap<QString, QShader> shaderCache;

    return shaderCache.ensure(name, [&name]() {
        QFile f(name);
        return f.open(QIODevice::ReadOnly) ? QShader::fromSerialized(f.readAll()) : QShader();
    });
}

static void rasterTransform(QImage &image, VideoTransformation transformation)
{
    QTransform t;
    if (transformation.rotation != QtVideo::Rotation::None)
        t.rotate(qreal(transformation.rotation));
    if (transformation.mirroredHorizontallyAfterRotation)
        t.scale(-1., 1);
    if (!t.isIdentity())
        image = image.transformed(t);
}

static void imageCleanupHandler(void *info)
{
    QByteArray *imageData = reinterpret_cast<QByteArray *>(info);
    delete imageData;
}

static bool updateTextures(QRhi *rhi,
                           std::unique_ptr<QRhiBuffer> &uniformBuffer,
                           std::unique_ptr<QRhiSampler> &textureSampler,
                           std::unique_ptr<QRhiShaderResourceBindings> &shaderResourceBindings,
                           std::unique_ptr<QRhiGraphicsPipeline> &graphicsPipeline,
                           std::unique_ptr<QRhiRenderPassDescriptor> &renderPass,
                           VideoFrame &frame,
                           const VideoFrameTexturesUPtr &videoFrameTextures)
{
    auto format = frame.surfaceFormat();
    auto pixelFormat = format.pixelFormat();

    auto textureDesc = VideoTextureHelper::textureDescription(pixelFormat);

    QRhiShaderResourceBinding bindings[4];
    auto *b = bindings;
    *b++ = QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                                                    uniformBuffer.get());
    for (int i = 0; i < textureDesc->nplanes; ++i)
        *b++ = QRhiShaderResourceBinding::sampledTexture(i + 1, QRhiShaderResourceBinding::FragmentStage,
                                                         videoFrameTextures->texture(i), textureSampler.get());
    shaderResourceBindings->setBindings(bindings, b);
    if (!shaderResourceBindings->create()) {
        qz::Log::cat_debug(qLcVideoFrameConverter, "{}: failed to create shader resource bindings", Q_FUNC_INFO);
        return false;
    }

    graphicsPipeline.reset(rhi->newGraphicsPipeline());
    graphicsPipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);

    QShader vs = ensureShader(VideoTextureHelper::vertexShaderFileName(format));
    if (!vs.isValid())
        return false;

    QShader fs = ensureShader(VideoTextureHelper::fragmentShaderFileName(format, rhi));
    if (!fs.isValid())
        return false;

    graphicsPipeline->setShaderStages({
        { QRhiShaderStage::Vertex, vs },
        { QRhiShaderStage::Fragment, fs }
    });

    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { 4 * sizeof(float) }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float) }
    });

    graphicsPipeline->setVertexInputLayout(inputLayout);
    graphicsPipeline->setShaderResourceBindings(shaderResourceBindings.get());
    graphicsPipeline->setRenderPassDescriptor(renderPass.get());
    if (!graphicsPipeline->create()) {
        qz::Log::cat_debug(qLcVideoFrameConverter, "{}: failed to create graphics pipeline", Q_FUNC_INFO);
        return false;
    }

    return true;
}

static QImage convertJPEG(const VideoFrame &frame, const VideoTransformation &transform)
{
    VideoFrame varFrame = frame;
    if (!varFrame.map(VideoFrame::ReadOnly)) {
        qz::Log::cat_debug(qLcVideoFrameConverter, "{}: frame mapping failed", Q_FUNC_INFO);
        return {};
    }

    auto unmap = std::optional(QScopeGuard([&] {
        varFrame.unmap();
    }));

    std::span<uchar> jpegData{
        varFrame.bits(0),
        static_cast<size_t>(varFrame.mappedBytes(0)),
    };

    constexpr std::array<uchar, 2> soiMarker{ uchar(0xff), uchar(0xd8) };
    if (!QtMultimediaPrivate::ranges::equal(jpegData.first(2), soiMarker, std::equal_to<void>{})) {
        qz::Log::cat_debug(qLcVideoFrameConverter, "{}: JPEG data does not start with SOI marker", Q_FUNC_INFO);
        return QImage{};
    }

    constexpr std::array<uchar, 2> eoiMarker{ uchar(0xff), uchar(0xd9) };

    if (!QtMultimediaPrivate::ranges::equal(jpegData.last(2), eoiMarker, std::equal_to<void>{})) {
        qz::Log::cat_debug(qLcVideoFrameConverter, "{}: JPEG data does not end with EOI marker", Q_FUNC_INFO);

        auto eoi_it = std::find_end(jpegData.begin(), jpegData.end(), std::begin(eoiMarker),
                                    std::end(eoiMarker));
        if (eoi_it == jpegData.end()) {
            qz::Log::cat_warn(qLcVideoFrameConverter, "{}: JPEG data does not contain EOI marker", Q_FUNC_INFO);
            return QImage{};
        };

        const size_t newSize = std::distance(jpegData.begin(), eoi_it) + std::size(eoiMarker);
        jpegData = jpegData.first(newSize);
    }

    QImage image = QImage::fromData(jpegData, "JPG");
    unmap = std::nullopt;
    rasterTransform(image, transform);
    return image;
}

static QImage convertCPU(const VideoFrame &frame, const VideoTransformation &transform)
{
    VideoFrameConvertFunc convert = qConverterForFormat(frame.pixelFormat());
    if (!convert) {
        qz::Log::cat_debug(qLcVideoFrameConverter, "{}: unsupported pixel format {}", Q_FUNC_INFO, static_cast<int>(frame.pixelFormat()));
        return {};
    } else {
        VideoFrame varFrame = frame;
        if (!varFrame.map(VideoFrame::ReadOnly)) {
            qz::Log::cat_debug(qLcVideoFrameConverter, "{}: frame mapping failed", Q_FUNC_INFO);
            return {};
        }
        auto format = pixelFormatHasAlpha(varFrame.pixelFormat()) ? QImage::Format_ARGB32_Premultiplied : QImage::Format_RGB32;
        QImage image = QImage(varFrame.width(), varFrame.height(), format);
        convert(varFrame, image.bits());
        varFrame.unmap();
        rasterTransform(image, transform);
        return image;
    }
}

QImage qImageFromVideoFrame(const VideoFrame &frame, bool forceCpu)
{

    return qImageFromVideoFrame(frame, qNormalizedSurfaceTransformation(frame.surfaceFormat()),
                                forceCpu);
}

QImage qImageFromVideoFrame(const VideoFrame &frame, const VideoTransformation &transformation,
                            bool forceCpu)
{
    std::unique_ptr<QRhiRenderPassDescriptor> renderPass;
    std::unique_ptr<QRhiBuffer> vertexBuffer;
    std::unique_ptr<QRhiBuffer> uniformBuffer;
    std::unique_ptr<QRhiTexture> targetTexture;
    std::unique_ptr<QRhiTextureRenderTarget> renderTarget;
    std::unique_ptr<QRhiSampler> textureSampler;
    std::unique_ptr<QRhiShaderResourceBindings> shaderResourceBindings;
    std::unique_ptr<QRhiGraphicsPipeline> graphicsPipeline;

    if (frame.size().isEmpty() || frame.pixelFormat() == VideoFrameFormat::Format_Invalid)
        return {};

    if (frame.pixelFormat() == VideoFrameFormat::Format_Jpeg)
        return convertJPEG(frame, transformation);

    if (forceCpu)
        return convertCPU(frame, transformation);

    QRhi *rhi = nullptr;

    if (HwVideoBuffer *buffer = VideoFramePrivate::hwBuffer(frame))
        rhi = buffer->rhi();

    if (!rhi || !rhi->thread()->isCurrentThread())
        rhi = qEnsureThreadLocalRhi(rhi);

    if (!rhi || rhi->isRecordingFrame())
        return convertCPU(frame, transformation);

    const QSize frameSize = qRotatedFrameSize(frame.size(), frame.surfaceFormat().rotation());

    vertexBuffer.reset(rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(g_quad)));
    if (!vertexBuffer->create()) {
        qz::Log::cat_debug(qLcVideoFrameConverter, "Failed to create vertex buffer. Using CPU conversion.");
        return convertCPU(frame, transformation);
    }

    uniformBuffer.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(VideoTextureHelper::UniformData)));
    if (!uniformBuffer->create()) {
        qz::Log::cat_debug(qLcVideoFrameConverter, "Failed to create uniform buffer. Using CPU conversion.");
        return convertCPU(frame, transformation);
    }

    textureSampler.reset(rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                         QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
    if (!textureSampler->create()) {
        qz::Log::cat_debug(qLcVideoFrameConverter, "Failed to create texture sampler. Using CPU conversion.");
        return convertCPU(frame, transformation);
    }

    shaderResourceBindings.reset(rhi->newShaderResourceBindings());

    targetTexture.reset(rhi->newTexture(QRhiTexture::RGBA8, frameSize, 1, QRhiTexture::RenderTarget));
    if (!targetTexture->create()) {
        qz::Log::cat_debug(qLcVideoFrameConverter, "Failed to create target texture. Using CPU conversion.");
        return convertCPU(frame, transformation);
    }

    renderTarget.reset(rhi->newTextureRenderTarget({ { targetTexture.get() } }));
    renderPass.reset(renderTarget->newCompatibleRenderPassDescriptor());
    renderTarget->setRenderPassDescriptor(renderPass.get());
    if (!renderTarget->create()) {
        qz::Log::cat_debug(qLcVideoFrameConverter, "Failed to create render target. Using CPU conversion.");
        return convertCPU(frame, transformation);
    }

    QRhiCommandBuffer *cb = nullptr;
    QRhi::FrameOpResult r = rhi->beginOffscreenFrame(&cb);
    if (r != QRhi::FrameOpSuccess) {
        qz::Log::cat_debug(qLcVideoFrameConverter, "Failed to set up offscreen frame. Using CPU conversion.");
        return convertCPU(frame, transformation);
    }

    QRhiResourceUpdateBatch *rub = rhi->nextResourceUpdateBatch();
    Q_ASSERT(rub);

    rub->uploadStaticBuffer(vertexBuffer.get(), g_quad);

    VideoFrame frameTmp = frame;
    VideoFrameTexturesUPtr texturesTmp;
    auto videoFrameTextures = VideoTextureHelper::createTextures(frameTmp, *rhi, *rub, texturesTmp);
    if (!videoFrameTextures) {
        qz::Log::cat_debug(qLcVideoFrameConverter, "Failed obtain textures. Using CPU conversion.");
        return convertCPU(frame, transformation);
    }

    if (!updateTextures(rhi, uniformBuffer, textureSampler, shaderResourceBindings,
                        graphicsPipeline, renderPass, frameTmp, videoFrameTextures)) {
        qz::Log::cat_debug(qLcVideoFrameConverter, "Failed to update textures. Using CPU conversion.");
        return convertCPU(frame, transformation);
    }

    float xScale = transformation.mirroredHorizontallyAfterRotation ? -1.0 : 1.0;
    float yScale = 1.f;

    if (rhi->isYUpInFramebuffer())
        yScale = -yScale;

    QMatrix4x4 transform;
    transform.scale(xScale, yScale);

    QByteArray uniformData(sizeof(VideoTextureHelper::UniformData), Qt::Uninitialized);
    VideoTextureHelper::updateUniformData(&uniformData, rhi, frame.surfaceFormat(), frame,
                                           transform, 1.f);
    rub->updateDynamicBuffer(uniformBuffer.get(), 0, uniformData.size(), uniformData.constData());

    cb->beginPass(renderTarget.get(), Qt::black, { 1.0f, 0 }, rub);
    cb->setGraphicsPipeline(graphicsPipeline.get());

    cb->setViewport({ 0, 0, static_cast<float>(frameSize.width()), static_cast<float>(frameSize.height()) });
    cb->setShaderResources(shaderResourceBindings.get());

    const quint32 vertexOffset = static_cast<quint32>(sizeof(float)) * 16 * transformation.rotationIndex();
    const QRhiCommandBuffer::VertexInput vbufBinding(vertexBuffer.get(), vertexOffset);
    cb->setVertexInput(0, 1, &vbufBinding);
    cb->draw(4);

    QRhiReadbackDescription readDesc(targetTexture.get());
    QRhiReadbackResult readResult;
    bool readCompleted = false;

    readResult.completed = [&readCompleted] { readCompleted = true; };

    rub = rhi->nextResourceUpdateBatch();
    rub->readBackTexture(readDesc, &readResult);

    cb->endPass(rub);

    rhi->endOffscreenFrame();

    if (!readCompleted) {
        qz::Log::cat_debug(qLcVideoFrameConverter, "Failed to read back texture. Using CPU conversion.");
        return convertCPU(frame, transformation);
    }

    auto *imageData = new QByteArray(readResult.data);

    return {reinterpret_cast<const uchar *>(imageData->constData()),
        readResult.pixelSize.width(), readResult.pixelSize.height(),
                  QImage::Format_RGBA8888_Premultiplied, imageCleanupHandler, imageData};
}

QImage videoFramePlaneAsImage(VideoFrame &frame, int plane, QImage::Format targetFormat,
                              QSize targetSize)
{
    if (plane >= frame.planeCount())
        return {};

    if (!frame.map(VideoFrame::ReadOnly)) {
        qWarning() << "Cannot map a video frame in ReadOnly mode!";
        return {};
    }

    auto frameHandle = VideoFramePrivate::handle(frame);

    frameHandle->ref.ref();

    auto imageCleanupFunction = [](void *data) {
        VideoFrame video_frame = static_cast<VideoFramePrivate *>(data)->adoptThisByVideoFrame();
        Q_ASSERT(video_frame.isMapped());
        video_frame.unmap();
    };

    const auto bytesPerLine = frame.bytesPerLine(plane);
    const auto height =
            bytesPerLine ? qMin(targetSize.height(), frame.mappedBytes(plane) / bytesPerLine) : 0;

    return QImage(reinterpret_cast<const uchar *>(frame.bits(plane)), targetSize.width(), height,
                  bytesPerLine, targetFormat, imageCleanupFunction, frameHandle);
}

