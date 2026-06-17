// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "VideoFrameTextureFromSource_p.h"

using namespace VideoTextureHelper;

VideoFrameTexturesFromRhiTextureArray::VideoFrameTexturesFromRhiTextureArray(RhiTextureArray &&rhiTextures)
    : m_rhiTextures(std::move(rhiTextures))
{
}

VideoFrameTexturesFromRhiTextureArray::~VideoFrameTexturesFromRhiTextureArray() = default;

QRhiTexture *VideoFrameTexturesFromRhiTextureArray::texture(uint plane) const
{
    return plane < m_rhiTextures.size() ? m_rhiTextures[plane].get() : nullptr;
}

void VideoFrameTexturesFromMemory::setMappedFrame(VideoFrame mappedFrame) {
    Q_ASSERT(!mappedFrame.isValid() || mappedFrame.isReadable());
    m_mappedFrame.unmap();
    m_mappedFrame = std::move(mappedFrame);
}

VideoFrameTexturesFromMemory::~VideoFrameTexturesFromMemory()
{
    m_mappedFrame.unmap();
}

void VideoFrameTexturesFromMemory::onFrameEndInvoked()
{

    setMappedFrame({});
    setSourceFrame({});
}

VideoFrameTexturesFromHandlesSet::VideoFrameTexturesFromHandlesSet(
        RhiTextureArray &&rhiTextures, VideoFrameTexturesHandlesUPtr handles)
    : VideoFrameTexturesFromRhiTextureArray(std::move(rhiTextures)),
      m_textureHandles(std::move(handles))
{
}

VideoFrameTexturesFromHandlesSet::~VideoFrameTexturesFromHandlesSet() = default;

