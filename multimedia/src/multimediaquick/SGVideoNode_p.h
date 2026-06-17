// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef SGVIDEONODE_P_H
#define SGVIDEONODE_P_H

#include <QtQuick/qsgnode.h>
#include <QtQuick/qsgmaterial.h>
#include "qzmultimediaquickexports.h"
#include "private/VideoTextureHelper_p.h"
#include "private/MultimediaUtils_p.h"
#include <SubtitleStyle.h>
#include <SGVideoTexture_p.h>

#include <qzMultimedia/VideoFrame.h>
#include <qzMultimedia/VideoFrameFormat.h>

QT_BEGIN_NAMESPACE

class QSGVideoMaterial;
class QuickVideoOutput;
class QSGInternalTextNode;
class QSGSimpleTextureNode;

class VideoFrameTexturePool;
using QVideoFrameTexturePoolPtr = std::shared_ptr<VideoFrameTexturePool>;

// 位图字幕 GPU 调色板查找着色器
class QSGSubtitleBitmapShader : public QSGMaterialShader
{
public:
    QSGSubtitleBitmapShader();

    bool updateUniformData(RenderState &state, QSGMaterial *newMaterial,
                           QSGMaterial *oldMaterial) override;
    void updateSampledImage(RenderState &state, int binding, QSGTexture **texture,
                            QSGMaterial *newMaterial, QSGMaterial *oldMaterial) override;
};

// 位图字幕 GPU 调色板查找材质
class QSGSubtitleBitmapMaterial : public QSGMaterial
{
public:
    QSGSubtitleBitmapMaterial(QRhi *rhi);

    QSGMaterialType *type() const override {
        static QSGMaterialType type;
        return &type;
    }

    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override {
        return new QSGSubtitleBitmapShader;
    }

    int compare(const QSGMaterial *other) const override {
        const auto *o = static_cast<const QSGSubtitleBitmapMaterial *>(other);
        qint64 diff = m_indexTexture.comparisonKey() - o->m_indexTexture.comparisonKey();
        return diff < 0 ? -1 : (diff > 0 ? 1 : 0);
    }

    // 设置待上传的字幕数据（在 UI 线程调用）
    void setPendingData(const SubtitleBitmapData &data);

    // 在渲染线程中上传纹理
    void uploadTextures(QRhi *rhi, QRhiResourceUpdateBatch *rub);

    QRhi *m_rhi = nullptr;
    QSGVideoTexture m_indexTexture;
    QSGVideoTexture m_paletteTexture;
    float m_opacity = 1.0f;
    int m_nbColors = 0;            // 调色板颜色数
    float m_texSizeW = 0.f;       // 合并纹理宽度（像素）
    float m_texSizeH = 0.f;       // 合并纹理高度（像素）
    bool m_dirty = true;
    bool m_hasData = false;
    // 跟踪上次上传的数据尺寸，用于判断是否需要重建纹理
    int m_lastIndexW = 0;
    int m_lastIndexH = 0;
    int m_lastPaletteCount = 0;
    // 每个 rect 在合并纹理中的区域
    QVector<QRect> m_rectRegions;
    // 待上传的数据（从 UI 线程传递到渲染线程）
    SubtitleBitmapData m_pendingData;
    bool m_textureUploadPending = false;
};

// 场景图视频节点：在 QSG 中渲染视频帧
class QSGVideoNode : public QSGGeometryNode
{
public:
    QSGVideoNode(QuickVideoOutput *parent, const VideoFrameFormat &videoFormat,
                 QRhi *rhi);
    ~QSGVideoNode() override;

    VideoFrameFormat::PixelFormat pixelFormat() const { return m_videoFormat.pixelFormat(); }
    // 设置当前帧
    void setCurrentFrame(const VideoFrame &frame);
    // 设置表面格式
    void setSurfaceFormat(const QRhiSwapChain::Format surfaceFormat);
    // 设置 HDR 信息
    void setHdrInfo(const QRhiSwapChainHdrInfo &hdrInfo);
    // 设置圆角矩形参数
    void setRoundedRectParams(float radius, const QRectF &renderedRect);

    // 设置带纹理的矩形几何
    void setTexturedRectGeometry(const QRectF &boundingRect, const QRectF &textureRect,
                                 VideoTransformation videoOutputTransformation);

    // 获取纹理池
    const QVideoFrameTexturePoolPtr &texturePool() const;

    // 更新字幕显示(文本或位图)
    void updateSubtitle(const VideoFrame &frame, const SubtitleStyle &style = {});

private:
    void updateSubtitleImage(const VideoFrame &frame);
    void updateSubtitleBitmap(const VideoFrame &frame);
    void setSubtitleGeometry();

    QuickVideoOutput *m_parent = nullptr;
    QRectF m_rect;
    QRectF m_textureRect;
    VideoTransformation m_videoOutputTransformation;
    VideoTransformation m_frameTransformation;

    VideoFrameFormat m_videoFormat;
    QSGVideoMaterial *m_material = nullptr;

    VideoTextureHelper::SubtitleLayout m_subtitleLayout;
    SubtitleStyle m_subtitleStyle;
    QSGInternalTextNode *m_subtitleTextNode = nullptr;
    QSGSimpleTextureNode *m_subtitleImageNode = nullptr;
    QImage m_lastSubtitleImage;

    // GPU 调色板查找字幕节点
    QSGGeometryNode *m_subtitleBitmapNode = nullptr;
    QSGSubtitleBitmapMaterial *m_subtitleBitmapMaterial = nullptr;
    SubtitleBitmapData m_lastSubtitleBitmapData;
};

QT_END_NAMESPACE

#endif
