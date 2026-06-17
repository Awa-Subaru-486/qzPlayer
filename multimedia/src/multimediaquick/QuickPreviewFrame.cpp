// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

/**
 * @file QuickPreviewFrame.cpp
 * @brief 预览帧显示组件实现
 * @details 使用自定义 QSGMaterial + YUV/RGB 着色器，支持圆角和 PreserveAspectCrop
 *          通过 QMutex 保护 m_frameData，避免数据复制，同时消除数据竞争
 */

#include "QuickPreviewFrame_p.h"

#include <QSGMaterial>
#include <QSGGeometryNode>
#include <QSGTexture>
#include <QQuickWindow>
#include <QMutexLocker>
#include <cstring>

#include <qzMultimedia/PreviewFrameProvider.h>

QT_BEGIN_NAMESPACE

// ============================================================
// 自定义 Material / Shader / Node
// 支持 YUV420P（3 纹理）、NV12（2 纹理）、RGBA（1 纹理）
// ============================================================

/**
 * @brief 预览帧材质，持有纹理和 uniform 数据
 */
struct PreviewFrameMaterial : public QSGMaterial
{
    PreviewFrameMaterial()
    {
        setFlag(Blending);
    }

    QSGMaterialType *type() const override
    {
        static QSGMaterialType t;
        return &t;
    }

    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override;

    int compare(const QSGMaterial *other) const override
    {
        const auto *o = static_cast<const PreviewFrameMaterial *>(other);
        const auto k1 = texture0 ? texture0->comparisonKey() : 0;
        const auto k2 = o->texture0 ? o->texture0->comparisonKey() : 0;
        return k1 < k2 ? -1 : (k1 > k2 ? 1 : 0);
    }

    QSGTexture *texture0 = nullptr;  // Y 或 RGBA
    QSGTexture *texture1 = nullptr;  // U 或 UV
    QSGTexture *texture2 = nullptr;  // V（YUV420P only）

    int format = 0;  // 0=RGBA, 1=YUV420P, 2=NV12

    float radii[4] = {};          // tl, tr, bl, br
    float itemSize[2] = {};       // width, height
    float fillTransform[4] = {};  // sx, sy, ox, oy (PreserveAspectCrop)
};

/**
 * @brief 预览帧着色器
 *
 * Uniform buffer layout (std140):
 *   offset  0: mat4 qt_Matrix     (64 bytes)
 *   offset 64: float qt_Opacity   (4 bytes)
 *   offset 72: vec2 size          (8 bytes, 8-aligned)
 *   offset 80: vec4 radii         (16 bytes)
 *   offset 96: vec4 u_transform   (16 bytes)
 *   offset 112: int u_format      (4 bytes)
 *   offset 116: int _pad          (4 bytes, padding to 16-byte boundary)
 *   Total: 128 bytes
 */
struct PreviewFrameShader : public QSGMaterialShader
{
    PreviewFrameShader()
    {
        setShaderFileName(VertexStage, QString::fromLatin1(":/qz/multimedia/shaders/PreviewFrame.vert.qsb"));
        setShaderFileName(FragmentStage, QString::fromLatin1(":/qz/multimedia/shaders/PreviewFrame.frag.qsb"));
    }

    bool updateUniformData(RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial) override
    {
        Q_UNUSED(oldMaterial);
        auto *mat = static_cast<PreviewFrameMaterial *>(newMaterial);
        QByteArray *buf = state.uniformData();
        if (buf->size() < 112)
            return false;

        bool changed = false;

        if (state.isMatrixDirty())
        {
            const auto matrix = state.combinedMatrix();
            std::memcpy(buf->data(), matrix.constData(), 64);
            changed = true;
        }

        if (state.isOpacityDirty())
        {
            const float opacity = state.opacity();
            std::memcpy(buf->data() + 64, &opacity, 4);
            changed = true;
        }

        std::memcpy(buf->data() + 72, mat->itemSize, 8);
        std::memcpy(buf->data() + 80, mat->radii, 16);
        std::memcpy(buf->data() + 96, mat->fillTransform, 16);
        std::memcpy(buf->data() + 112, &mat->format, 4);

        return true;
    }

    void updateSampledImage(RenderState &state, int binding, QSGTexture **texture,
                            QSGMaterial *newMaterial, QSGMaterial *oldMaterial) override
    {
        Q_UNUSED(oldMaterial);
        auto *mat = static_cast<PreviewFrameMaterial *>(newMaterial);

        QSGTexture *tex = nullptr;
        if (binding == 1)
            tex = mat->texture0;
        else if (binding == 2)
            tex = mat->texture1;
        else if (binding == 3)
            tex = mat->texture2;

        *texture = tex;
        if (tex)
        {
            tex->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
        }
    }
};

QSGMaterialShader *PreviewFrameMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new PreviewFrameShader;
}

/**
 * @brief 预览帧渲染节点
 * @details sync() 在渲染线程执行，通过 item 的 frameMutex 保护对 m_frameData 的访问
 *          不复制帧数据，直接引用 item 的 shared_ptr（零拷贝）
 */
struct PreviewFrameNode : public QSGGeometryNode
{
    PreviewFrameNode()
        : m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
    {
        m_geometry.setDrawingMode(QSGGeometry::DrawTriangleStrip);
        setGeometry(&m_geometry);
        setMaterial(&m_material);
    }

    ~PreviewFrameNode() override
    {
        delete m_texture0;
        delete m_texture1;
        delete m_texture2;
    }

    /**
     * @brief 同步数据到渲染节点（渲染线程）
     * @param item QuickPreviewFrame，通过其 frameMutex 保护数据访问
     *
     * 通过 mutex 保护对 item->frameData() 的访问，避免与 GUI 线程的 onFrameReady 竞争。
     * 纹理创建使用 shared_ptr 引用的原始数据，不增加引用计数（零拷贝），
     * 但 mutex 保证在 sync 期间 shared_ptr 不会被重置。
     */
    void sync(const QuickPreviewFrame *item)
    {
        QQuickWindow *window = item->window();
        const qreal dpr = window ? window->devicePixelRatio() : 1.0;
        const qreal w = item->width();
        const qreal h = item->height();
        const qreal radius = item->radiusValue();
        const QRectF rect = item->boundingRect();

        // 持有锁期间读取 frameData 和 generation
        // shared_ptr 引用的数据在此期间不会被释放
        const QMutexLocker locker(&item->frameMutex());
        const PreviewFrameData &frameData = item->frameData();
        const int gen = item->generation();

        // 仅在帧数据变更时重建纹理
        if (gen != m_lastGen && frameData.isValid() && window)
        {
            delete m_texture0;
            delete m_texture1;
            delete m_texture2;
            m_texture0 = m_texture1 = m_texture2 = nullptr;

            if (frameData.format == PreviewFrameData::Format::RGBA)
            {
                QImage img(frameData.planes[0].data.get(),
                           frameData.width, frameData.height,
                           frameData.planes[0].linesize,
                           QImage::Format_RGBA8888);
                m_texture0 = window->createTextureFromImage(img);
                m_material.format = 0;
            }
            else if (frameData.format == PreviewFrameData::Format::YUV420P)
            {
                m_texture0 = createLuminanceTexture(window,
                                                    frameData.planes[0],
                                                    frameData.width,
                                                    frameData.height);
                m_texture1 = createLuminanceTexture(window,
                                                    frameData.planes[1],
                                                    (frameData.width + 1) / 2,
                                                    (frameData.height + 1) / 2);
                m_texture2 = createLuminanceTexture(window,
                                                    frameData.planes[2],
                                                    (frameData.width + 1) / 2,
                                                    (frameData.height + 1) / 2);
                m_material.format = 1;
            }
            else if (frameData.format == PreviewFrameData::Format::NV12)
            {
                m_texture0 = createLuminanceTexture(window,
                                                    frameData.planes[0],
                                                    frameData.width,
                                                    frameData.height);
                m_texture1 = createLuminanceTexture(window,
                                                    frameData.planes[1],
                                                    (frameData.width + 1) / 2,
                                                    (frameData.height + 1) / 2);
                m_texture2 = nullptr;
                m_material.format = 2;
            }

            m_lastGen = gen;
        }

        m_material.texture0 = m_texture0;
        m_material.texture1 = m_texture1;
        m_material.texture2 = m_texture2;

        // 圆角半径（考虑 DPR 缩放）
        const float r = static_cast<float>(radius * dpr);
        m_material.radii[0] = r;
        m_material.radii[1] = r;
        m_material.radii[2] = r;
        m_material.radii[3] = r;

        // Item 尺寸
        m_material.itemSize[0] = static_cast<float>(w);
        m_material.itemSize[1] = static_cast<float>(h);

        // PreserveAspectCrop 填充变换（此时 frameData 引用仍有效，锁未释放）
        float sx = 1.0f, sy = 1.0f, ox = 0.0f, oy = 0.0f;
        if (frameData.isValid())
        {
            const auto srcW = static_cast<float>(frameData.width);
            const auto srcH = static_cast<float>(frameData.height);
            const auto dstW = static_cast<float>(w);
            const auto dstH = static_cast<float>(h);
            if (srcW > 0 && srcH > 0 && dstW > 0 && dstH > 0)
            {
                const float srcRatio = srcW / srcH;
                const float dstRatio = dstW / dstH;
                if (dstRatio > srcRatio)
                {
                    sy = dstRatio / srcRatio;
                    oy = (1.0f - sy) / 2.0f;
                }
                else
                {
                    sx = srcRatio / dstRatio;
                    ox = (1.0f - sx) / 2.0f;
                }
            }
        }
        m_material.fillTransform[0] = sx;
        m_material.fillTransform[1] = sy;
        m_material.fillTransform[2] = ox;
        m_material.fillTransform[3] = oy;

        // 更新几何体
        QSGGeometry::updateTexturedRectGeometry(&m_geometry, rect, QRectF(0, 0, 1, 1));
        markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
    }

private:
    QSGTexture *createLuminanceTexture(QQuickWindow *window,
                                       const PreviewFrameData::Plane &plane,
                                       int width, int height)
    {
        if (!window || !plane.data || width <= 0 || height <= 0)
            return nullptr;

        QImage img(plane.data.get(), width, height, plane.linesize, QImage::Format_Grayscale8);
        return window->createTextureFromImage(img);
    }

    PreviewFrameMaterial m_material;
    QSGGeometry m_geometry;
    QSGTexture *m_texture0 = nullptr;
    QSGTexture *m_texture1 = nullptr;
    QSGTexture *m_texture2 = nullptr;
    int m_lastGen = -1;
};

// ============================================================
// QuickPreviewFrame 实现
// ============================================================

QuickPreviewFrame::QuickPreviewFrame(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);

    m_provider = new PreviewFrameProvider(this);

    connect(&m_frameWatcher, &QFutureWatcher<PreviewFrameData>::finished,
            this, &QuickPreviewFrame::onFrameReady);

    // 切换源后延迟 500ms 再请求帧，避免与播放器状态变化冲突
    m_sourceDebounceTimer.setSingleShot(true);
    m_sourceDebounceTimer.setInterval(500);
    connect(&m_sourceDebounceTimer, &QTimer::timeout, this, [this]() {
        // 延迟结束后，仅当源仍非空时请求帧
        if (!m_source.isEmpty())
            requestFrame();
    });
}

QuickPreviewFrame::~QuickPreviewFrame()
{
    // 停止延迟定时器
    m_sourceDebounceTimer.stop();

    // 等待所有异步任务完成，避免后端访问已释放的资源
    if (m_frameWatcher.isRunning())
    {
        m_frameWatcher.cancel();
        m_frameWatcher.waitForFinished();
    }
}

QUrl QuickPreviewFrame::source() const
{
    return m_source;
}

void QuickPreviewFrame::setSource(const QUrl &source)
{
    if (m_source == source)
        return;

    // 切换源时彻底清理所有缓存，显示黑色空帧，暂时不解析
    //
    // 设计：切换 URL 时不立即请求新帧，仅显示黑色空帧
    // 后续 position 变化或手动调用才会触发解析

    // 步骤 1：停止延迟定时器
    m_sourceDebounceTimer.stop();

    // 步骤 2：取消后端 token 和 future watcher（非阻塞）
    m_provider->cancel();
    if (m_frameWatcher.isRunning())
    {
        m_frameWatcher.cancel();
    }

    // 步骤 3：设置新源（后端完全异步，不阻塞）
    m_source = source;
    m_provider->setSource(source);

    // 步骤 4：构造黑色空帧（RGBA 格式，2x2 全黑）
    // 让渲染节点显示黑色，而不是空白
    PreviewFrameData blackFrame;
    blackFrame.format = PreviewFrameData::Format::RGBA;
    blackFrame.width = 2;
    blackFrame.height = 2;
    {
        // 分配 2x2 RGBA buffer，全 0（黑色不透明）
        // 使用 shared_ptr 管理生命周期
        const int bufSize = 2 * 2 * 4;  // width * height * 4 (RGBA)
        uchar *buf = new uchar[bufSize]();  // 值初始化为 0（黑色，alpha=0 透明）
        // 设置 alpha=255 使其不透明
        for (int i = 3; i < bufSize; i += 4)
            buf[i] = 255;
        blackFrame.planes[0].data = std::shared_ptr<const uchar>(buf, [](const uchar *p) { delete[] p; });
        blackFrame.planes[0].linesize = 2 * 4;  // width * 4
        blackFrame.planes[0].width = 2;
        blackFrame.planes[0].height = 2;
    }

    // 步骤 5：替换帧数据为黑色空帧（持锁）
    {
        const QMutexLocker locker(&m_frameMutex);
        m_frameData = blackFrame;
    }

    // 步骤 6：递增 generation，让渲染节点重建纹理显示黑色
    ++m_generation;

    // 标记源刚切换，下一次 setPosition 时跳过请求
    m_sourceJustChanged = true;

    emit sourceChanged();

    // 不请求新帧，等待 position 变化或外部触发
}

qint64 QuickPreviewFrame::position() const
{
    return m_position;
}

void QuickPreviewFrame::setPosition(qint64 position)
{
    if (m_position == position)
        return;
    m_position = position;
    emit positionChanged();

    // 源刚切换后第一次 setPosition 跳过请求，避免立即解析导致崩溃
    if (m_sourceJustChanged)
    {
        m_sourceJustChanged = false;
        return;
    }

    requestFrame();
}

qreal QuickPreviewFrame::radius() const
{
    return m_radius;
}

void QuickPreviewFrame::setRadius(qreal r)
{
    if (qFuzzyCompare(m_radius, r))
        return;
    m_radius = r;
    emit radiusChanged();
    update();
}

void QuickPreviewFrame::requestFrame()
{
    if (m_source.isEmpty())
        return;

    // 异步取消旧请求（不阻塞 GUI 线程）
    // 后端 requestFrame 内部会 cancel + waitForFinished 旧任务
    // onFrameReady 会检查 isCanceled 跳过旧帧
    if (m_frameWatcher.isRunning())
    {
        m_provider->cancel();
        m_frameWatcher.cancel();
    }

    auto future = m_provider->requestFrame(m_position);
    m_frameWatcher.setFuture(future);
}

void QuickPreviewFrame::onFrameReady()
{
    // canceled 的 future 不应取结果
    if (m_frameWatcher.isCanceled())
        return;

    // 持有锁期间替换 m_frameData
    // 渲染线程若正在 sync，会等待锁释放
    {
        const QMutexLocker locker(&m_frameMutex);
        m_frameData = m_frameWatcher.result();
    }
    ++m_generation;
    update();
}

QSGNode *QuickPreviewFrame::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    // updatePaintNode 在 GUI 线程执行
    // sync() 内部会通过 frameMutex 保护对 m_frameData 的访问

    // 先快速检查是否有有效帧（无锁，仅作提前退出优化）
    if (m_generation.load(std::memory_order_acquire) == 0 && m_frameData.format == PreviewFrameData::Format::Invalid)
    {
        delete oldNode;
        return nullptr;
    }

    auto *node = static_cast<PreviewFrameNode *>(oldNode);
    if (!node)
    {
        node = new PreviewFrameNode();
    }

    node->sync(this);
    return node;
}

QT_END_NAMESPACE
