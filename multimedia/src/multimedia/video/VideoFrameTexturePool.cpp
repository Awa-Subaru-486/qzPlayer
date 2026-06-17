// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "VideoFrameTexturePool_p.h"
#include "VideoTextureHelper_p.h"

#include <rhi/qrhi.h>
import qzLog;

static qz::Log::LogCategory qLcVideoFrameTexturePool("qz.multimedia.video.texturepool");

void VideoFrameTexturePool::setCurrentFrame(VideoFrame frame) {
    m_texturesDirty = true;
    m_currentFrame = std::move(frame);
}

VideoFrameTextures* VideoFrameTexturePool::updateTextures(QRhi &rhi, QRhiResourceUpdateBatch &rub) {
    const int currentSlot = rhi.currentFrameSlot();
    Q_ASSERT(static_cast<size_t>(currentSlot) < MaxSlotsCount);

    m_texturesDirty = false;
    VideoFrameTexturesUPtr &textures = m_textureSlots[currentSlot];
    Q_ASSERT(!m_oldTextures);
    m_oldTextures = std::move(textures);
    textures = VideoTextureHelper::createTextures(m_currentFrame, rhi, rub, m_oldTextures);

    m_currentSlot = textures ? currentSlot : std::optional<int>{};

    if (textures) {
        qz::Log::cat_debug(qLcVideoFrameTexturePool,
            "Textures created/updated: pixelFormat={} size={}x{}",
            static_cast<int>(m_currentFrame.pixelFormat()),
            m_currentFrame.width(), m_currentFrame.height());
    }

    return textures.get();
}

void VideoFrameTexturePool::onFrameEndInvoked()
{
    m_oldTextures.reset();
    if (m_currentSlot && m_textureSlots[*m_currentSlot])
        m_textureSlots[*m_currentSlot]->onFrameEndInvoked();
}

void VideoFrameTexturePool::clearTextures()
{
    std::ranges::fill(m_textureSlots, nullptr);
    m_currentSlot.reset();
    m_texturesDirty = m_currentFrame.isValid();
}

