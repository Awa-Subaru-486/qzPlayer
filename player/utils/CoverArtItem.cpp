/**
 * @file CoverArtItem.cpp
 * @brief 封面图像显示组件实现
 * @details 使用自定义 QSGMaterial + 圆角着色器，零拷贝纹理上传
 */

#include "CoverArtItem.hpp"

#include <QSGMaterial>
#include <QSGGeometryNode>
#include <QSGTexture>
#include <QQuickWindow>
#include <QtConcurrent>
#include <cstring>

#include "ThemeConfig.hpp"

#ifdef Q_OS_ANDROID
#include <QSysInfo>
#endif

#include <qzMultimedia/MediaPlayer.h>

import KmeansColors;
import qzLog;
import qzTheme;

namespace qz {

// ============================================================
// 自定义 Material / Shader / Node — 圆角着色器 + 零拷贝
// ============================================================

/**
 * @brief 圆角封面材质，持有纹理和 uniform 数据
 */
struct CoverArtMaterial : public QSGMaterial
{
    CoverArtMaterial()
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
        const auto *o = static_cast<const CoverArtMaterial *>(other);
        if (!texture || !o->texture) return texture ? 1 : -1;
        const auto k1 = texture->comparisonKey();
        const auto k2 = o->texture->comparisonKey();
        return k1 < k2 ? -1 : (k1 > k2 ? 1 : 0);
    }

    QSGTexture *texture = nullptr;
    float radii[4] = {};          // tl, tr, bl, br
    float itemSize[2] = {};       // width, height
    float fillTransform[4] = {};  // sx, sy, ox, oy (PreserveAspectCrop)
    float imgScale = 1.0f;
};

/**
 * @brief 圆角封面着色器，加载预编译 .qsb 着色器
 *
 * Uniform buffer layout (std140):
 *   offset  0: mat4 qt_Matrix     (64 bytes)
 *   offset 64: float qt_Opacity   (4 bytes)
 *   offset 72: vec2 size          (8 bytes, 8-aligned)
 *   offset 80: vec4 radii         (16 bytes)
 *   offset 96: vec4 u_transform   (16 bytes)
 *   offset 112: float u_imgScale  (4 bytes)
 *   Total: 128 bytes (padded to 16-byte boundary)
 */
struct CoverArtShader : public QSGMaterialShader
{
    CoverArtShader()
    {
        setShaderFileName(VertexStage, ":/qz/player/shaders/RoundedImage.vert.qsb");
        setShaderFileName(FragmentStage, ":/qz/player/shaders/RoundedImage.frag.qsb");
    }

    bool updateUniformData(RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial) override
    {
        Q_UNUSED(oldMaterial);
        auto *mat = static_cast<CoverArtMaterial *>(newMaterial);
        QByteArray *buf = state.uniformData();
        Q_ASSERT(buf->size() >= 128);

        bool changed = false;

        // mat4 qt_Matrix @ 0
        if (state.isMatrixDirty())
        {
            const auto matrix = state.combinedMatrix();
            std::memcpy(buf->data(), matrix.constData(), 64);
            changed = true;
        }

        // float qt_Opacity @ 64
        if (state.isOpacityDirty())
        {
            const float opacity = state.opacity();
            std::memcpy(buf->data() + 64, &opacity, 4);
            changed = true;
        }

        // vec2 size @ 72
        std::memcpy(buf->data() + 72, mat->itemSize, 8);
        // vec4 radii @ 80
        std::memcpy(buf->data() + 80, mat->radii, 16);
        // vec4 u_transform @ 96
        std::memcpy(buf->data() + 96, mat->fillTransform, 16);
        // float u_imgScale @ 112
        std::memcpy(buf->data() + 112, &mat->imgScale, 4);

        return true;
    }

    void updateSampledImage(RenderState &state, int binding, QSGTexture **texture,
                            QSGMaterial *newMaterial, QSGMaterial *oldMaterial) override
    {
        Q_UNUSED(oldMaterial);
        if (binding == 1)
        {
            auto *mat = static_cast<CoverArtMaterial *>(newMaterial);
            *texture = mat->texture;
            if (mat->texture)
            {
                mat->texture->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
            }
        }
    }
};

QSGMaterialShader *CoverArtMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new CoverArtShader;
}

/**
 * @brief 圆角封面渲染节点，管理纹理生命周期
 */
struct CoverArtNode : public QSGGeometryNode
{
    CoverArtNode()
        : m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
    {
        m_geometry.setDrawingMode(QSGGeometry::DrawTriangleStrip);
        setGeometry(&m_geometry);
        setMaterial(&m_material);
    }

    ~CoverArtNode() override
    {
        delete m_texture;
    }

    void sync(const CoverArtItem *item)
    {
        const int gen = item->generation();

        // 仅在图像变更时重建纹理（零拷贝：直接上传原始图像，GPU 负责缩放）
        if (gen != m_lastGen && !item->coverImage().isNull() && item->window())
        {
            delete m_texture;
            m_texture = item->window()->createTextureFromImage(item->coverImage());
            m_lastGen = gen;
        }

        m_material.texture = m_texture;

        // 圆角半径（考虑 DPR 缩放）
        const qreal dpr = item->window() ? item->window()->devicePixelRatio() : 1.0;
        const float r = static_cast<float>(item->radius() * dpr);
        m_material.radii[0] = r;
        m_material.radii[1] = r;
        m_material.radii[2] = r;
        m_material.radii[3] = r;

        // Item 尺寸
        m_material.itemSize[0] = static_cast<float>(item->width());
        m_material.itemSize[1] = static_cast<float>(item->height());

        // PreserveAspectCrop 填充变换
        float sx = 1.0f, sy = 1.0f, ox = 0.0f, oy = 0.0f;
        const QImage &cover = item->coverImage();
        if (!cover.isNull())
        {
            const auto srcW = static_cast<float>(cover.width());
            const auto srcH = static_cast<float>(cover.height());
            const auto dstW = static_cast<float>(item->width());
            const auto dstH = static_cast<float>(item->height());
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
        m_material.imgScale = 1.0f;

        // 更新几何体
        QSGGeometry::updateTexturedRectGeometry(&m_geometry, item->boundingRect(), QRectF(0, 0, 1, 1));
        markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
    }

private:
    CoverArtMaterial m_material;
    QSGGeometry m_geometry;
    QSGTexture *m_texture = nullptr;
    int m_lastGen = -1;
};

// ============================================================
// CoverArtItem 实现
// ============================================================

CoverArtItem::CoverArtItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);

    connect(&m_coverWatcher, &QFutureWatcher<QImage>::finished,
            this, &CoverArtItem::onCoverFinished);
    connect(&m_colorWatcher, &QFutureWatcher<std::optional<QColor>>::finished,
            this, &CoverArtItem::onColorExtracted);
}

CoverArtItem::~CoverArtItem()
{
    disconnect(&m_coverWatcher, &QFutureWatcher<QImage>::finished,
               this, &CoverArtItem::onCoverFinished);
    disconnect(&m_colorWatcher, &QFutureWatcher<std::optional<QColor>>::finished,
               this, &CoverArtItem::onColorExtracted);

    m_coverWatcher.cancel();
    m_coverWatcher.waitForFinished();
    m_colorWatcher.cancel();
    m_colorWatcher.waitForFinished();
}

QObject* CoverArtItem::mediaPlayer() const
{
    return m_mediaPlayer;
}

void CoverArtItem::set_mediaPlayer(QObject *player)
{
    if (m_mediaPlayer == player)
        return;

    if (m_mediaPlayer)
    {
        disconnect(m_mediaPlayer, nullptr, this, nullptr);
    }

    m_mediaPlayer = player;

    if (m_mediaPlayer)
    {
        auto *mp = qobject_cast<MediaPlayer*>(m_mediaPlayer);
        if (mp)
        {
            connect(mp, &MediaPlayer::sourceChanged, this, [this]() {
                m_coverNeeded = true;
            });
            connect(mp, &MediaPlayer::mediaStatusChanged, this, [this](MediaPlayer::MediaStatus status) {
                if (m_coverNeeded && status == MediaPlayer::LoadedMedia)
                {
                    m_coverNeeded = false;
                    requestCover();
                }
            });
            if (!mp->source().isEmpty())
            {
                m_coverNeeded = true;
            }
        }
    }
    else
    {
        const bool wasEmpty = m_coverImage.isNull();
        m_coverImage = QImage();
        update();
        if (!wasEmpty)
            emit isEmptyChanged();
    }

    emit mediaPlayerChanged();
}

qreal CoverArtItem::radius() const
{
    return m_radius;
}

void CoverArtItem::set_radius(qreal r)
{
    if (qFuzzyCompare(m_radius, r))
        return;
    m_radius = r;
    emit radiusChanged();
    update();
}

bool CoverArtItem::isEmpty() const
{
    return m_coverImage.isNull();
}

qreal CoverArtItem::coverAspectRatio() const
{
    return m_coverAspectRatio;
}

qreal CoverArtItem::maxDim() const
{
    return m_maxDim;
}

void CoverArtItem::set_maxDim(qreal dim)
{
    if (qFuzzyCompare(m_maxDim, dim) || dim <= 0)
        return;
    m_maxDim = dim;
    emit maxDimChanged();
    updateImplicitSize();
}

const QImage &CoverArtItem::coverImage() const
{
    return m_coverImage;
}

int CoverArtItem::generation() const
{
    return m_generation.load(std::memory_order_acquire);
}

void CoverArtItem::requestCover()
{
    if (!m_mediaPlayer)
        return;

    ++m_generation;

    auto *player = qobject_cast<MediaPlayer*>(m_mediaPlayer);
    if (!player)
        return;

    auto future = QtConcurrent::run([player]() -> QImage {
        return player->getMediaCover();
    });

    m_coverWatcher.setFuture(future);
}

void CoverArtItem::onCoverFinished()
{
    if (m_coverWatcher.isCanceled())
        return;

    const QImage cover = m_coverWatcher.result();
    const bool wasEmpty = m_coverImage.isNull();

    if (cover.isNull())
    {
        m_coverImage = QImage();
        updateImplicitSize();
        update();
        if (!wasEmpty)
            emit isEmptyChanged();
        return;
    }

    m_coverImage = cover;
    updateImplicitSize();
    update();
    if (wasEmpty)
        emit isEmptyChanged();

    extractColor(cover);
}

void CoverArtItem::extractColor(const QImage &image)
{
#ifdef Q_OS_ANDROID
    Q_UNUSED(image);
    // Android 下不进行类聚算法提取颜色，避免性能开销
    return;
#else
    auto future = extract_dominant_color(image);
    m_colorWatcher.setFuture(future);
#endif
}

void CoverArtItem::updateImplicitSize()
{
    if (m_coverImage.isNull())
    {
        if (m_coverAspectRatio != 0.0)
        {
            m_coverAspectRatio = 0.0;
            setImplicitWidth(0);
            setImplicitHeight(0);
            emit coverAspectRatioChanged();
        }
        return;
    }

    const qreal w = m_coverImage.width();
    const qreal h = m_coverImage.height();
    if (w <= 0 || h <= 0)
        return;

    const qreal aspect = w / h;
    const qreal oldAspect = m_coverAspectRatio;
    m_coverAspectRatio = aspect;

    if (aspect >= 1.0)
    {
        // 宽屏或正方形：宽度 = maxDim，高度 = maxDim / aspect
        setImplicitWidth(m_maxDim);
        setImplicitHeight(m_maxDim / aspect);
    }
    else
    {
        // 竖屏：高度 = maxDim，宽度 = maxDim * aspect
        setImplicitWidth(m_maxDim * aspect);
        setImplicitHeight(m_maxDim);
    }

    if (!qFuzzyCompare(oldAspect, aspect))
        emit coverAspectRatioChanged();
}

void CoverArtItem::onColorExtracted()
{
    if (m_colorWatcher.isCanceled())
        return;

    const auto result = m_colorWatcher.result();
    if (!result.has_value())
    {
        Log::debug("CoverArtItem: no dominant color extracted");
        return;
    }

    qzTheme::ThemeBackend::instance().set_accentColor(result.value());
}

QSGNode *CoverArtItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto *node = static_cast<CoverArtNode *>(oldNode);

    if (m_coverImage.isNull())
    {
        delete node;
        return nullptr;
    }

    if (!node)
    {
        node = new CoverArtNode();
    }

    node->sync(this);
    return node;
}

} // namespace qz
