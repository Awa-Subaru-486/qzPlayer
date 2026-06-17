// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_VIDEO_HWVIDEOBUFFER_P_H
#define QT_VIDEO_HWVIDEOBUFFER_P_H

#include "AbstractVideoBuffer.h"
#include "VideoFrame.h"

#include <QtGui/qmatrix4x4.h>

class QRhi;
class QRhiTexture;
class VideoFrame;

// 视频帧纹理句柄接口
class QZ_MULTIMEDIA_EXPORT VideoFrameTexturesHandles
{
public:
    virtual ~VideoFrameTexturesHandles();

    // 获取纹理句柄
    virtual quint64 textureHandle(QRhi &, int ) { return 0; };
};
using VideoFrameTexturesHandlesUPtr = std::unique_ptr<VideoFrameTexturesHandles>;

// 视频帧纹理接口：用于 GPU 渲染
class QZ_MULTIMEDIA_EXPORT VideoFrameTextures
{
public:
    virtual ~VideoFrameTextures();
    // 获取指定平面的纹理
    virtual QRhiTexture *texture(uint plane) const = 0;

    // 帧结束回调
    virtual void onFrameEndInvoked() { }

    // 获取纹理句柄
    virtual VideoFrameTexturesHandlesUPtr takeHandles() { return nullptr; }

    void setSourceFrame(VideoFrame sourceFrame) { m_sourceFrame = std::move(sourceFrame); }

private:
    VideoFrame m_sourceFrame;
};
using VideoFrameTexturesUPtr = std::unique_ptr<VideoFrameTextures>;

// 硬件视频缓冲区：GPU 内存中的视频数据
class QZ_MULTIMEDIA_EXPORT HwVideoBuffer : public AbstractVideoBuffer,
                                           public VideoFrameTexturesHandles
{
public:
    HwVideoBuffer(VideoFrame::HandleType type, QRhi *rhi = nullptr);

    ~HwVideoBuffer() override;

    VideoFrame::HandleType handleType() const { return m_type; }
    virtual QRhi *rhi() const { return m_rhi; }

    VideoFrameFormat format() const override { return {}; }

    // 外部纹理矩阵
    virtual QMatrix4x4 externalTextureMatrix() const { return {}; }

    // 映射为纹理
    virtual VideoFrameTexturesUPtr mapTextures(QRhi &, VideoFrameTexturesUPtr& ) { return nullptr; };

    // 初始化纹理转换器
    virtual void initTextureConverter(QRhi &) { }

    // 是否为 DMA 缓冲区
    virtual bool isDmaBuf() const { return false; }

protected:
    VideoFrame::HandleType m_type;
    QRhi *m_rhi = nullptr;
};

#endif
