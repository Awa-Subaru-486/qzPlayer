// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_VIDEO_VIDEOFRAMETEXTUREPOOL_P_H
#define QT_VIDEO_VIDEOFRAMETEXTUREPOOL_P_H

#include "VideoFrame.h"
#include "HwVideoBuffer_p.h"

#include <array>
#include <optional>

class QRhi;
class QRhiResourceUpdateBatch;

// 视频帧纹理池：管理视频帧到 GPU 纹理的上传和复用
class QZ_MULTIMEDIA_EXPORT VideoFrameTexturePool {
    static constexpr size_t MaxSlotsCount = 4;
public:

    // 纹理是否需要更新
    bool texturesDirty() const { return m_texturesDirty; }

    // 当前帧
    const VideoFrame& currentFrame() const { return m_currentFrame; }

    void setCurrentFrame(VideoFrame frame);

    // 更新纹理
    VideoFrameTextures* updateTextures(QRhi &rhi, QRhiResourceUpdateBatch &rub);

    // 帧结束回调
    void onFrameEndInvoked();

    // 清除纹理
    void clearTextures();

private:
    VideoFrame m_currentFrame;
    bool m_texturesDirty = false;
    std::array<VideoFrameTexturesUPtr, MaxSlotsCount> m_textureSlots;
    std::optional<int> m_currentSlot;
    VideoFrameTexturesUPtr m_oldTextures;
};

#endif
