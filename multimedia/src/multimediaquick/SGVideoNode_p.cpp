// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "SGVideoNode_p.h"
#include <QtQuick/qsgmaterial.h>
#include <QtQuick/qsgsimpletexturenode.h>
#include <QtQuick/qquickwindow.h>
#include "SGVideoTexture_p.h"
#include <qzMultimedia/private/VideoTextureHelper_p.h>
#include <private/qsginternaltextnode_p.h>
#include <private/qsgadaptationlayer_p.h>
#include <private/qquickitem_p.h>
#include "QuickVideoOutput_p.h"
#include <private/HwVideoBuffer_p.h>
#include <private/VideoFrameTexturePool_p.h>
import qzLog;

static qz::Log::LogCategory qLcSGVideoNode("qz.multimedia.quick.sgvideonode");

QT_BEGIN_NAMESPACE

static inline void qSetGeom(QSGGeometry::TexturedPoint2D *v, const QPointF &p)
{
    v->x = p.x();
    v->y = p.y();
}

static inline void qSetTex(QSGGeometry::TexturedPoint2D *v, const QPointF &p)
{
    v->tx = p.x();
    v->ty = p.y();
}

static inline void qSwapTex(QSGGeometry::TexturedPoint2D *v0, QSGGeometry::TexturedPoint2D *v1)
{
    auto tvx = v0->tx;
    auto tvy = v0->ty;
    v0->tx = v1->tx;
    v0->ty = v1->ty;
    v1->tx = tvx;
    v1->ty = tvy;
}

class QSGVideoMaterial;

class QSGVideoMaterialRhiShader : public QSGMaterialShader
{
public:
    QSGVideoMaterialRhiShader(const VideoFrameFormat &videoFormat,
                              const QRhiSwapChain::Format surfaceFormat,
                              const QRhiSwapChainHdrInfo &hdrInfo,
                              QRhi *rhi)
        : m_videoFormat(videoFormat)
        , m_surfaceFormat(surfaceFormat)
        , m_hdrInfo(hdrInfo)
    {
        setShaderFileName(VertexStage, VideoTextureHelper::vertexShaderFileName(m_videoFormat));
        setShaderFileName(FragmentStage, VideoTextureHelper::fragmentShaderFileName(
                                                 m_videoFormat, rhi, m_surfaceFormat));

        computeFormatUniforms(rhi);
    }

    bool updateUniformData(RenderState &state, QSGMaterial *newMaterial,
                           QSGMaterial *oldMaterial) override;

    void updateSampledImage(RenderState &state, int binding, QSGTexture **texture,
                            QSGMaterial *newMaterial, QSGMaterial *oldMaterial) override;

protected:
    void computeFormatUniforms(QRhi *rhi);

    VideoFrameFormat m_videoFormat;
    QRhiSwapChain::Format m_surfaceFormat;
    QRhiSwapChainHdrInfo m_hdrInfo;

    float m_cachedColorMatrix[4][4] = {};
    int m_cachedRedOrAlphaIndex = 0;
    int m_cachedPlaneFormats[3] = {};
    int m_cachedColorRange = 0;
    float m_cachedWidth = 0.f;
    float m_cachedMasteringWhite = 0.f;
};

class QSGVideoMaterial : public QSGMaterial
{
public:
    QSGVideoMaterial(const VideoFrameFormat &videoFormat, QRhi *rhi);

    [[nodiscard]] QSGMaterialType *type() const override {
        static constexpr int NFormats = QRhiSwapChain::HDRExtendedDisplayP3Linear + 1;
        static QSGMaterialType type[VideoFrameFormat::NPixelFormats][NFormats];
        return &type[m_videoFormat.pixelFormat()][m_surfaceFormat];
    }

    [[nodiscard]] QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override {
        return new QSGVideoMaterialRhiShader(m_videoFormat, m_surfaceFormat, m_hdrInfo, m_rhi);
    }

    int compare(const QSGMaterial *other) const override {
        const QSGVideoMaterial *m = static_cast<const QSGVideoMaterial *>(other);

        qint64 diff = m_textures[0].comparisonKey() - m->m_textures[0].comparisonKey();
        if (!diff)
            diff = m_textures[1].comparisonKey() - m->m_textures[1].comparisonKey();
        if (!diff)
            diff = m_textures[2].comparisonKey() - m->m_textures[2].comparisonKey();

        return diff < 0 ? -1 : (diff > 0 ? 1 : 0);
    }

    void updateBlending() {
        setFlag(Blending, !qFuzzyCompare(m_opacity, float(1.0)) || m_radius > 0.f);
    }

    void setSurfaceFormat(const QRhiSwapChain::Format surfaceFormat)
    {
        m_surfaceFormat = surfaceFormat;
    }

    void setHdrInfo(const QRhiSwapChainHdrInfo &hdrInfo)
    {
        m_hdrInfo = hdrInfo;
    }

    void updateTextures(QRhi *rhi, QRhiResourceUpdateBatch *resourceUpdates);

    VideoFrameFormat m_videoFormat;
    QRhiSwapChain::Format m_surfaceFormat = QRhiSwapChain::SDR;
    float m_opacity = 1.0f;
    QRhiSwapChainHdrInfo m_hdrInfo;

    QVideoFrameTexturePoolPtr m_texturePool = std::make_shared<VideoFrameTexturePool>();
    std::array<QSGVideoTexture, 3> m_textures;

    QRhi *m_rhi;

    float m_radius = 0.f;
    float m_rectSize[2] = { 0.f, 0.f };
    float m_rectOffset[2] = { 0.f, 0.f };
    bool m_roundedRectDirty = false;

    void setRoundedRectParams(float radius, const QRectF &renderedRect)
    {
        m_radius = radius;
        m_rectSize[0] = renderedRect.width();
        m_rectSize[1] = renderedRect.height();
        m_rectOffset[0] = renderedRect.x();
        m_rectOffset[1] = renderedRect.y();
        m_roundedRectDirty = true;
        updateBlending();
    }
};

void QSGVideoMaterial::updateTextures(QRhi *rhi, QRhiResourceUpdateBatch *resourceUpdates)
{
    if (!m_texturePool->texturesDirty())
        return;

    VideoFrameTextures *textures = m_texturePool->updateTextures(*rhi, *resourceUpdates);
    if (!textures)
        return;

    for (int plane = 0; plane < 3; ++plane)
        m_textures[plane].setRhiTexture(textures->texture(plane));
}

void QSGVideoMaterialRhiShader::computeFormatUniforms(QRhi *rhi)
{
    auto cache = VideoTextureHelper::computeFormatUniformCache(m_videoFormat, rhi);
    memcpy(m_cachedColorMatrix, cache.colorMatrix, sizeof(m_cachedColorMatrix));
    m_cachedRedOrAlphaIndex = cache.redOrAlphaIndex;
    memcpy(m_cachedPlaneFormats, cache.planeFormats, sizeof(m_cachedPlaneFormats));
    m_cachedColorRange = cache.colorRange;
    m_cachedWidth = cache.width;
    m_cachedMasteringWhite = cache.masteringWhite;
}

bool QSGVideoMaterialRhiShader::updateUniformData(RenderState &state, QSGMaterial *newMaterial,
                                                      QSGMaterial *oldMaterial)
{
    Q_UNUSED(oldMaterial);

    auto m = static_cast<QSGVideoMaterial *>(newMaterial);

    if (!state.isMatrixDirty() && !state.isOpacityDirty() && !m->m_roundedRectDirty)
        return false;

    if (state.isOpacityDirty()) {
        m->m_opacity = state.opacity();
        m->updateBlending();
    }

    m->updateTextures(state.rhi(), state.resourceUpdateBatch());

    float maxNits = 100;
    if (m_surfaceFormat == QRhiSwapChain::HDRExtendedSrgbLinear) {
        if (m_hdrInfo.limitsType == QRhiSwapChainHdrInfo::ColorComponentValue)
            maxNits = 100 * m_hdrInfo.limits.colorComponentValue.maxColorComponentValue;
        else
            maxNits = m_hdrInfo.limits.luminanceInNits.maxLuminance;
    }

    auto &uniformData = *state.uniformData();
    if (uniformData.size() < static_cast<int>(sizeof(VideoTextureHelper::UniformData)))
        uniformData.resize(sizeof(VideoTextureHelper::UniformData));

    auto ud = reinterpret_cast<VideoTextureHelper::UniformData *>(uniformData.data());
    memcpy(ud->transformMatrix, state.combinedMatrix().constData(), sizeof(ud->transformMatrix));
    memcpy(ud->colorMatrix, m_cachedColorMatrix, sizeof(ud->colorMatrix));
    ud->opacity = state.opacity();
    ud->width = m_cachedWidth;
    ud->masteringWhite = m_cachedMasteringWhite;

    ud->maxLum = VideoTextureHelper::computeMaxLum(float(maxNits), m_videoFormat);

    ud->redOrAlphaIndex = m_cachedRedOrAlphaIndex;
    memcpy(ud->planeFormats, m_cachedPlaneFormats, sizeof(ud->planeFormats));
    ud->colorRange = m_cachedColorRange;
    ud->radius = m->m_radius;
    ud->rectSize[0] = m->m_rectSize[0];
    ud->rectSize[1] = m->m_rectSize[1];
    ud->rectOffset[0] = m->m_rectOffset[0];
    ud->rectOffset[1] = m->m_rectOffset[1];

    m->m_roundedRectDirty = false;

    return true;
}

void QSGVideoMaterialRhiShader::updateSampledImage(RenderState &state, int binding, QSGTexture **texture,
                                                       QSGMaterial *newMaterial, QSGMaterial *oldMaterial)
{
    Q_UNUSED(state);
    Q_UNUSED(oldMaterial);
    if (binding < 1 || binding > 3)
        return;

    auto m = static_cast<QSGVideoMaterial *>(newMaterial);
    *texture = &m->m_textures[binding - 1];
}

QSGVideoMaterial::QSGVideoMaterial(const VideoFrameFormat &videoFormat, QRhi *rhi)
    : m_videoFormat(videoFormat),
    m_rhi(rhi)
{
    setFlag(Blending, false);
}

QSGVideoNode::QSGVideoNode(QuickVideoOutput *parent, const VideoFrameFormat &videoFormat,
                           QRhi *rhi)
    : m_parent(parent), m_videoFormat(videoFormat)
{
    setFlag(QSGNode::OwnsMaterial);
    setFlag(QSGNode::OwnsGeometry);
    m_material = new QSGVideoMaterial(videoFormat, rhi);
    setMaterial(m_material);
}

QSGVideoNode::~QSGVideoNode()
{
    delete m_subtitleTextNode;
    delete m_subtitleImageNode;
    delete m_subtitleBitmapNode;
}

void QSGVideoNode::setCurrentFrame(const VideoFrame &frame)
{
    texturePool()->setCurrentFrame(frame);
    markDirty(DirtyMaterial);
    updateSubtitle(frame, m_subtitleStyle);
}

void QSGVideoNode::setSurfaceFormat(const QRhiSwapChain::Format surfaceFormat)
{
    m_material->setSurfaceFormat(surfaceFormat);
    markDirty(DirtyMaterial);
}

void QSGVideoNode::setHdrInfo(const QRhiSwapChainHdrInfo &hdrInfo)
{
    m_material->setHdrInfo(hdrInfo);
    markDirty(DirtyMaterial);
}

void QSGVideoNode::setRoundedRectParams(float radius, const QRectF &renderedRect)
{
    m_material->setRoundedRectParams(radius, renderedRect);
    markDirty(DirtyMaterial);
}

void QSGVideoNode::updateSubtitle(const VideoFrame &frame, const SubtitleStyle &style)
{
    m_subtitleStyle = style;

    QSize subtitleFrameSize = m_rect.size().toSize();
    if (subtitleFrameSize.isEmpty())
        return;

    subtitleFrameSize = qRotatedFrameSize(subtitleFrameSize, m_videoOutputTransformation.rotation);

    // 优先使用 GPU 调色板查找路径
    if (frame.hasSubtitleBitmapData()) {
        updateSubtitleBitmap(frame);
        return;
    }

    // 清除 GPU 调色板查找节点
    delete m_subtitleBitmapNode;
    m_subtitleBitmapNode = nullptr;
    m_subtitleBitmapMaterial = nullptr;
    m_lastSubtitleBitmapData = {};

    if (!frame.subtitleImage().isNull()) {
        updateSubtitleImage(frame);
        return;
    }

    delete m_subtitleImageNode;
    m_subtitleImageNode = nullptr;
    m_lastSubtitleImage = {};

    if (frame.subtitleText().isEmpty())
        return;

    if (!m_subtitleLayout.update(subtitleFrameSize, frame.subtitleText(), m_subtitleStyle))
        return;

    delete m_subtitleTextNode;
    m_subtitleTextNode = nullptr;
    if (frame.subtitleText().isEmpty())
        return;

    QQuickItemPrivate *parent_d = QQuickItemPrivate::get(m_parent);

    m_subtitleTextNode = parent_d->sceneGraphContext()->createInternalTextNode(parent_d->sceneGraphRenderContext());
    m_subtitleTextNode->setColor(m_subtitleStyle.fontColor());
    QColor bgColor = m_subtitleStyle.backgroundColor();
    bgColor.setAlphaF(m_subtitleStyle.backgroundOpacity());
    QSGInternalRectangleNode *rectNode = parent_d->sceneGraphContext()->createInternalRectangleNode();
    rectNode->setRect(m_subtitleLayout.bounds);
    rectNode->setColor(bgColor);
    rectNode->setRadius(m_subtitleLayout.cornerRadius);
    rectNode->setAntialiasing(true);
    rectNode->update();
    m_subtitleTextNode->appendChildNode(rectNode);
    m_subtitleTextNode->addTextLayout(m_subtitleLayout.layout.position(), &m_subtitleLayout.layout);
    appendChildNode(m_subtitleTextNode);
    setSubtitleGeometry();
}

void QSGVideoNode::updateSubtitleImage(const VideoFrame &frame)
{
    // 清除 GPU 调色板查找节点
    delete m_subtitleBitmapNode;
    m_subtitleBitmapNode = nullptr;
    m_subtitleBitmapMaterial = nullptr;
    m_lastSubtitleBitmapData = {};

    const QImage &img = frame.subtitleImage();
    if (img.isNull()) {
        qz::Log::cat_debug(qLcSGVideoNode, "updateSubtitleImage: image is null, removing node");
        delete m_subtitleImageNode;
        m_subtitleImageNode = nullptr;
        m_lastSubtitleImage = {};
        return;
    }

    // Skip update if the image data pointer hasn't changed (same frame)
    if (img.constBits() == m_lastSubtitleImage.constBits())
        return;

    qz::Log::cat_debug(qLcSGVideoNode, "updateSubtitleImage: imageSize={}x{} subRect={}",
                        img.width(), img.height(), frame.subtitleRect());

    m_lastSubtitleImage = img;

    delete m_subtitleTextNode;
    m_subtitleTextNode = nullptr;

    QQuickWindow *window = m_parent ? m_parent->window() : nullptr;
    if (!window)
        return;

    // Reuse existing node if possible (same size), only update texture
    const bool sizeChanged = !m_subtitleImageNode
        || m_subtitleImageNode->rect().size() != QSizeF(img.width(), img.height());

    if (sizeChanged) {
        delete m_subtitleImageNode;
        QSGTexture *texture = window->createTextureFromImage(img);
        if (!texture)
            return;

        m_subtitleImageNode = new QSGSimpleTextureNode;
        m_subtitleImageNode->setTexture(texture);
        m_subtitleImageNode->setOwnsTexture(true);
        m_subtitleImageNode->setFiltering(QSGTexture::Linear);
        appendChildNode(m_subtitleImageNode);
    } else {
        // Same size: just update the texture without recreating the node
        QSGTexture *texture = window->createTextureFromImage(img);
        if (!texture)
            return;

        m_subtitleImageNode->setTexture(texture);
        m_subtitleImageNode->setOwnsTexture(true);
    }

    const QRect &subRect = frame.subtitleRect();
    QSizeF videoSize = qRotatedFramePresentationSize(frame);
    double scaleX = m_rect.width() / videoSize.width();
    double scaleY = m_rect.height() / videoSize.height();

    QRectF drawRect(m_rect.x() + subRect.x() * scaleX,
                    m_rect.y() + subRect.y() * scaleY,
                    subRect.width() * scaleX,
                    subRect.height() * scaleY);

    qz::Log::cat_debug(qLcSGVideoNode, "updateSubtitleImage: videoSize={}x{} renderRect={} scale={}x{} drawRect={}",
                        static_cast<int>(videoSize.width()), static_cast<int>(videoSize.height()),
                        m_rect, scaleX, scaleY, drawRect);

    m_subtitleImageNode->setRect(drawRect);
    m_subtitleImageNode->markDirty(QSGNode::DirtyMaterial);
    setSubtitleGeometry();
}

void QSGVideoNode::setSubtitleGeometry()
{
    if (m_subtitleTextNode) {
        if (m_material)
            updateSubtitle(texturePool()->currentFrame(), m_subtitleStyle);

        float rotate = -1.f * qToUnderlying(m_videoOutputTransformation.rotation);
        float yTranslate = 0;
        float xTranslate = 0;
        if (m_videoOutputTransformation.rotation == QtVideo::Rotation::Clockwise90) {
            yTranslate = m_rect.height();
        } else if (m_videoOutputTransformation.rotation == QtVideo::Rotation::Clockwise180) {
            yTranslate = m_rect.height();
            xTranslate = m_rect.width();
        } else if (m_videoOutputTransformation.rotation == QtVideo::Rotation::Clockwise270) {
            xTranslate = m_rect.width();
        }

        QMatrix4x4 transform;
        transform.translate(m_rect.x() + xTranslate, m_rect.y() + yTranslate);
        transform.rotate(rotate, 0, 0, 1);

        m_subtitleTextNode->setMatrix(transform);
        m_subtitleTextNode->markDirty(DirtyGeometry);
    }

    // 更新位图字幕节点的几何（窗口大小变化时顶点位置需要更新）
    if (m_subtitleBitmapNode && m_subtitleBitmapMaterial) {
        const auto &lastData = m_lastSubtitleBitmapData;
        if (lastData.isValid()) {
            const auto &frame = texturePool()->currentFrame();
            QSizeF videoSize = qRotatedFramePresentationSize(frame);
            if (videoSize.width() > 0 && videoSize.height() > 0) {
                double scaleX = m_rect.width() / videoSize.width();
                double scaleY = m_rect.height() / videoSize.height();

                QSGGeometry::TexturedPoint2D *v =
                        m_subtitleBitmapNode->geometry()->vertexDataAsTexturedPoint2D();
                const auto &regions = m_subtitleBitmapMaterial->m_rectRegions;
                const float texW = m_subtitleBitmapMaterial->m_texSizeW;
                const float texH = m_subtitleBitmapMaterial->m_texSizeH;

                if (regions.size() < lastData.rectCount())
                    return;

                for (int i = 0; i < lastData.rectCount(); ++i) {
                    const auto &rect = lastData.rects[i];
                    const QRectF &region = regions[i];

                    QRectF drawRect(m_rect.x() + rect.x * scaleX,
                                    m_rect.y() + rect.y * scaleY,
                                    rect.w * scaleX,
                                    rect.h * scaleY);

                    float u0 = region.x() / texW;
                    float v0 = region.y() / texH;
                    float u1 = (region.x() + region.width()) / texW;
                    float v1 = (region.y() + region.height()) / texH;

                    int base = i * 6;
                    v[base + 0].set(drawRect.x(), drawRect.y(), u0, v0);
                    v[base + 1].set(drawRect.x() + drawRect.width(), drawRect.y() + drawRect.height(), u1, v1);
                    v[base + 2].set(drawRect.x(), drawRect.y() + drawRect.height(), u0, v1);
                    v[base + 3].set(drawRect.x(), drawRect.y(), u0, v0);
                    v[base + 4].set(drawRect.x() + drawRect.width(), drawRect.y(), u1, v0);
                    v[base + 5].set(drawRect.x() + drawRect.width(), drawRect.y() + drawRect.height(), u1, v1);
                }
                m_subtitleBitmapNode->markDirty(QSGNode::DirtyGeometry);
            }
        }
    }
}

void QSGVideoNode::setTexturedRectGeometry(const QRectF &rect, const QRectF &textureRect,
                                           VideoTransformation videoOutputTransformation)
{
    const VideoTransformation currentFrameTransformation = qNormalizedFrameTransformation(
            m_material ? texturePool()->currentFrame() : VideoFrame{}, videoOutputTransformation);

    if (rect == m_rect && textureRect == m_textureRect
        && videoOutputTransformation == m_videoOutputTransformation
        && currentFrameTransformation == m_frameTransformation)
        return;

    m_rect = rect;
    m_textureRect = textureRect;
    m_videoOutputTransformation = videoOutputTransformation;
    m_frameTransformation = currentFrameTransformation;

    QSGGeometry *g = geometry();

    if (g == nullptr)
        g = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4);

    QSGGeometry::TexturedPoint2D *v = g->vertexDataAsTexturedPoint2D();

    qSetGeom(v + 0, rect.topLeft());
    qSetGeom(v + 1, rect.bottomLeft());
    qSetGeom(v + 2, rect.topRight());
    qSetGeom(v + 3, rect.bottomRight());

    switch (currentFrameTransformation.rotation) {
    default:

        qSetTex(v + 0, textureRect.topLeft());
        qSetTex(v + 1, textureRect.bottomLeft());
        qSetTex(v + 2, textureRect.topRight());
        qSetTex(v + 3, textureRect.bottomRight());
        break;

    case QtVideo::Rotation::Clockwise90:

        qSetTex(v + 0, textureRect.bottomLeft());
        qSetTex(v + 1, textureRect.bottomRight());
        qSetTex(v + 2, textureRect.topLeft());
        qSetTex(v + 3, textureRect.topRight());
        break;

    case QtVideo::Rotation::Clockwise180:

        qSetTex(v + 0, textureRect.bottomRight());
        qSetTex(v + 1, textureRect.topRight());
        qSetTex(v + 2, textureRect.bottomLeft());
        qSetTex(v + 3, textureRect.topLeft());
        break;

    case QtVideo::Rotation::Clockwise270:

        qSetTex(v + 0, textureRect.topRight());
        qSetTex(v + 1, textureRect.topLeft());
        qSetTex(v + 2, textureRect.bottomRight());
        qSetTex(v + 3, textureRect.bottomLeft());
        break;
    }

    if (m_frameTransformation.mirroredHorizontallyAfterRotation) {
        qSwapTex(v + 0, v + 2);
        qSwapTex(v + 1, v + 3);
    }

    if (!geometry())
        setGeometry(g);

    markDirty(DirtyGeometry);

    setSubtitleGeometry();
}

const QVideoFrameTexturePoolPtr &QSGVideoNode::texturePool() const
{
    return m_material->m_texturePool;
}

// ============================================================================
// QSGSubtitleBitmapMaterial / QSGSubtitleBitmapShader 实现
// ============================================================================

QSGSubtitleBitmapMaterial::QSGSubtitleBitmapMaterial(QRhi *rhi)
    : m_rhi(rhi)
{
    setFlag(Blending, true); // 字幕需要 Alpha 混合
    setFlag(RequiresFullMatrixExceptTranslate, true);
}

void QSGSubtitleBitmapMaterial::setPendingData(const SubtitleBitmapData &data)
{
    m_pendingData = data;

    // 在 UI 线程预计算 rect 区域（供顶点计算使用）
    m_rectRegions.clear();
    int totalHeight = 0;
    for (const auto &rect : data.rects) {
        m_rectRegions.append(QRect(0, totalHeight, rect.w, rect.h));
        totalHeight += rect.h;
    }
    m_texSizeW = static_cast<float>(data.rects.isEmpty() ? 0 : [&]() {
        int maxW = 0;
        for (const auto &r : data.rects) maxW = qMax(maxW, r.w);
        return maxW;
    }());
    m_texSizeH = static_cast<float>(totalHeight);

    m_textureUploadPending = true;
    m_dirty = true;
}

void QSGSubtitleBitmapMaterial::uploadTextures(QRhi *rhi, QRhiResourceUpdateBatch *rub)
{
    if (!rhi || !rub || !m_textureUploadPending)
        return;

    const auto &data = m_pendingData;
    if (data.isEmpty()) {
        m_textureUploadPending = false;
        return;
    }

    // 合并所有 rect 的索引数据到一张纹理（垂直堆叠）
    int totalWidth = 0;
    int totalHeight = 0;
    m_rectRegions.clear();

    for (const auto &rect : data.rects) {
        totalWidth = qMax(totalWidth, rect.w);
        m_rectRegions.append(QRect(0, totalHeight, rect.w, rect.h));
        totalHeight += rect.h;
    }

    // 索引纹理：R8 格式，尺寸 totalWidth x totalHeight
    {
        const bool needRecreate = (m_lastIndexW != totalWidth || m_lastIndexH != totalHeight);
        QRhiTexture *tex = m_indexTexture.rhiTexture();

        if (needRecreate || !tex) {
            if (tex)
                delete tex;

            tex = rhi->newTexture(QRhiTexture::R8, QSize(totalWidth, totalHeight));
            if (!tex->create()) {
                qz::Log::cat_warn(qLcSGVideoNode, "Failed to create index texture {}x{}", totalWidth, totalHeight);
                delete tex;
                return;
            }
            m_indexTexture.setRhiTexture(tex);
            m_indexTexture.setFiltering(QSGTexture::Nearest);
            m_indexTexture.setHorizontalWrapMode(QSGTexture::ClampToEdge);
            m_indexTexture.setVerticalWrapMode(QSGTexture::ClampToEdge);
            m_lastIndexW = totalWidth;
            m_lastIndexH = totalHeight;
        }

        // 上传合并后的索引数据
        QByteArray mergedIndex(totalWidth * totalHeight, 0);
        for (int i = 0; i < data.rects.size(); ++i) {
            const auto &rect = data.rects[i];
            const auto &region = m_rectRegions[i];
            const auto &idx = *rect.indexData;
            for (int row = 0; row < rect.h; ++row) {
                memcpy(mergedIndex.data() + (region.y() + row) * totalWidth + region.x(),
                       idx.constData() + row * rect.w, rect.w);
            }
        }

        QRhiTextureSubresourceUploadDescription subres;
        subres.setData(mergedIndex);
        subres.setDataStride(totalWidth);
        QRhiTextureUploadEntry entry(0, 0, subres);
        QRhiTextureUploadDescription desc({ entry });
        rub->uploadTexture(tex, desc);
    }

    // 调色板纹理：BGRA8 格式，尺寸 nbColors x 1
    {
        const bool needRecreate = (m_lastPaletteCount != data.nbColors);
        QRhiTexture *tex = m_paletteTexture.rhiTexture();

        if (needRecreate || !tex) {
            if (tex)
                delete tex;

            const int palWidth = qMax(data.nbColors, 1);
            tex = rhi->newTexture(QRhiTexture::BGRA8, QSize(palWidth, 1));
            if (!tex->create()) {
                qz::Log::cat_warn(qLcSGVideoNode, "Failed to create palette texture size={}", palWidth);
                delete tex;
                return;
            }
            m_paletteTexture.setRhiTexture(tex);
            m_paletteTexture.setFiltering(QSGTexture::Nearest);
            m_paletteTexture.setHorizontalWrapMode(QSGTexture::ClampToEdge);
            m_paletteTexture.setVerticalWrapMode(QSGTexture::ClampToEdge);
            m_lastPaletteCount = data.nbColors;
        }

        const auto &pal = *data.palette;
        QRhiTextureSubresourceUploadDescription subres;
        QByteArray paletteBytes(reinterpret_cast<const char *>(pal.constData()),
                                pal.size() * sizeof(uint32_t));
        subres.setData(paletteBytes);
        subres.setDataStride(data.nbColors * sizeof(uint32_t));
        QRhiTextureUploadEntry entry(0, 0, subres);
        QRhiTextureUploadDescription desc({ entry });
        rub->uploadTexture(tex, desc);
    }

    m_texSizeW = static_cast<float>(totalWidth);
    m_texSizeH = static_cast<float>(totalHeight);
    m_hasData = true;
    m_textureUploadPending = false;
}

QSGSubtitleBitmapShader::QSGSubtitleBitmapShader()
{
    setShaderFileName(VertexStage, QStringLiteral(":/qz/multimedia/shaders/subtitle_index.vert.qsb"));
    setShaderFileName(FragmentStage, QStringLiteral(":/qz/multimedia/shaders/subtitle_index.frag.qsb"));
}

bool QSGSubtitleBitmapShader::updateUniformData(RenderState &state, QSGMaterial *newMaterial,
                                                  QSGMaterial *oldMaterial)
{
    auto *m = static_cast<QSGSubtitleBitmapMaterial *>(newMaterial);

    bool dirty = false;
    if (state.isOpacityDirty()) {
        m->m_opacity = state.opacity();
        dirty = true;
    }

    if (state.isMatrixDirty())
        dirty = true;

    if (m->m_dirty)
        dirty = true;

    if (!dirty)
        return false;

    // 在渲染线程中上传纹理
    if (m->m_textureUploadPending) {
        m->uploadTextures(state.rhi(), state.resourceUpdateBatch());
    }

    // Uniform 数据结构：与着色器中的 buf 一致（std140 布局）
    // mat4 matrix (64 bytes, 偏移 0)
    // int nbColors (4 bytes, 偏移 64)
    // float opacity (4 bytes, 偏移 68)
    // vec2 texSize (8 bytes, 偏移 72) — 需要 8 字节对齐，72 是 8 的倍数
    // padding (4 bytes, 偏移 80) — 总大小需为 16 的倍数 → 96 字节
    struct SubtitleUniformData {
        float transformMatrix[4][4];
        qint32 nbColors;
        float opacity;
        float texSize[2];           // 索引纹理尺寸 (宽, 高)
        qint32 _padding[2];         // 填充到 96 字节
    };

    auto &uniformData = *state.uniformData();
    if (uniformData.size() < static_cast<int>(sizeof(SubtitleUniformData)))
        uniformData.resize(sizeof(SubtitleUniformData));

    auto *ud = reinterpret_cast<SubtitleUniformData *>(uniformData.data());
    memcpy(ud->transformMatrix, state.combinedMatrix().constData(), sizeof(ud->transformMatrix));
    ud->nbColors = m->m_nbColors;
    ud->opacity = m->m_opacity;
    ud->texSize[0] = m->m_texSizeW;
    ud->texSize[1] = m->m_texSizeH;

    m->m_dirty = false;
    return true;
}

void QSGSubtitleBitmapShader::updateSampledImage(RenderState &state, int binding,
                                                   QSGTexture **texture,
                                                   QSGMaterial *newMaterial,
                                                   QSGMaterial *oldMaterial)
{
    Q_UNUSED(state);
    Q_UNUSED(oldMaterial);
    auto *m = static_cast<QSGSubtitleBitmapMaterial *>(newMaterial);

    if (binding == 1)
        *texture = &m->m_indexTexture;
    else if (binding == 2)
        *texture = &m->m_paletteTexture;
}

// ============================================================================
// QSGVideoNode::updateSubtitleBitmap 实现
// ============================================================================

void QSGVideoNode::updateSubtitleBitmap(const VideoFrame &frame)
{
    const auto &data = frame.subtitleBitmapData();

    // 清除其他字幕节点
    delete m_subtitleImageNode;
    m_subtitleImageNode = nullptr;
    m_lastSubtitleImage = {};
    delete m_subtitleTextNode;
    m_subtitleTextNode = nullptr;

    // 空数据：清除位图字幕节点
    if (data.isEmpty()) {
        delete m_subtitleBitmapNode;
        m_subtitleBitmapNode = nullptr;
        m_subtitleBitmapMaterial = nullptr;
        m_lastSubtitleBitmapData = {};
        return;
    }

    // 去重检查：shared_ptr 指针相同即为相同数据（零拷贝保证）
    if (data.isSameData(m_lastSubtitleBitmapData))
        return;

    m_lastSubtitleBitmapData = data;

    // 计算字幕在渲染区域中的位置
    QSizeF videoSize = qRotatedFramePresentationSize(frame);
    double scaleX = m_rect.width() / videoSize.width();
    double scaleY = m_rect.height() / videoSize.height();

    // 创建/更新位图字幕节点
    if (!m_subtitleBitmapNode) {
        m_subtitleBitmapNode = new QSGGeometryNode;
        m_subtitleBitmapMaterial = new QSGSubtitleBitmapMaterial(
                m_material ? m_material->m_rhi : nullptr);
        m_subtitleBitmapNode->setMaterial(m_subtitleBitmapMaterial);
        m_subtitleBitmapNode->setFlag(QSGNode::OwnsMaterial);

        // 使用 DrawTriangles 支持多 rect
        const int vertexCount = data.rectCount() * 6; // 每个 rect 2 个三角形
        QSGGeometry *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), vertexCount);
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        m_subtitleBitmapNode->setGeometry(geometry);
        m_subtitleBitmapNode->setFlag(QSGNode::OwnsGeometry);

        appendChildNode(m_subtitleBitmapNode);
    } else {
        // 重建几何（rect 数量可能变化）
        const int vertexCount = data.rectCount() * 6;
        QSGGeometry *geometry = m_subtitleBitmapNode->geometry();
        if (geometry->vertexCount() != vertexCount) {
            geometry->allocate(vertexCount);
            geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        }
    }

    // 更新材质参数
    m_subtitleBitmapMaterial->m_nbColors = data.nbColors;
    m_subtitleBitmapMaterial->setPendingData(data);

    // 更新顶点数据：每个 rect 6 个顶点（2 个三角形）
    QSGGeometry::TexturedPoint2D *v = m_subtitleBitmapNode->geometry()->vertexDataAsTexturedPoint2D();
    const auto &regions = m_subtitleBitmapMaterial->m_rectRegions;
    const float texW = m_subtitleBitmapMaterial->m_texSizeW;
    const float texH = m_subtitleBitmapMaterial->m_texSizeH;

    if (regions.size() < data.rectCount())
        return;

    for (int i = 0; i < data.rectCount(); ++i) {
        const auto &rect = data.rects[i];
        const QRectF &region = regions[i];

        // 屏幕坐标
        QRectF drawRect(m_rect.x() + rect.x * scaleX,
                        m_rect.y() + rect.y * scaleY,
                        rect.w * scaleX,
                        rect.h * scaleY);

        // 纹理坐标（在合并纹理中的位置）
        float u0 = region.x() / texW;
        float v0 = region.y() / texH;
        float u1 = (region.x() + region.width()) / texW;
        float v1 = (region.y() + region.height()) / texH;

        // 三角形 1: 左上-右下-左下
        int base = i * 6;
        v[base + 0].set(drawRect.x(), drawRect.y(), u0, v0);
        v[base + 1].set(drawRect.x() + drawRect.width(), drawRect.y() + drawRect.height(), u1, v1);
        v[base + 2].set(drawRect.x(), drawRect.y() + drawRect.height(), u0, v1);
        // 三角形 2: 左上-右上-右下
        v[base + 3].set(drawRect.x(), drawRect.y(), u0, v0);
        v[base + 4].set(drawRect.x() + drawRect.width(), drawRect.y(), u1, v0);
        v[base + 5].set(drawRect.x() + drawRect.width(), drawRect.y() + drawRect.height(), u1, v1);
    }

    m_subtitleBitmapNode->markDirty(QSGNode::DirtyMaterial | QSGNode::DirtyGeometry);
}

QT_END_NAMESPACE
