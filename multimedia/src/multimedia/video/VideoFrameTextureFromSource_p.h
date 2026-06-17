// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_VIDEO_VIDEOFRAMETEXTUREFROMSOURCE_P_H
#define QT_VIDEO_VIDEOFRAMETEXTUREFROMSOURCE_P_H

#include "private/HwVideoBuffer_p.h"
#include "VideoTextureHelper_p.h"

#include <rhi/qrhi.h>

namespace VideoTextureHelper {

using RhiTextureArray = std::array<std::unique_ptr<QRhiTexture>, TextureDescription::maxPlanes>;

class VideoFrameTexturesFromRhiTextureArray : public VideoFrameTextures
{
public:
    VideoFrameTexturesFromRhiTextureArray(RhiTextureArray &&rhiTextures = {});

    ~VideoFrameTexturesFromRhiTextureArray() override;

    QRhiTexture *texture(uint plane) const override;

    RhiTextureArray &textureArray() { return m_rhiTextures; }

private:
    RhiTextureArray m_rhiTextures;
};

class VideoFrameTexturesFromMemory : public VideoFrameTexturesFromRhiTextureArray
{
public:
    using VideoFrameTexturesFromRhiTextureArray::VideoFrameTexturesFromRhiTextureArray;

    void setMappedFrame(VideoFrame mappedFrame);

    ~VideoFrameTexturesFromMemory() override;

    void onFrameEndInvoked() override;

private:
    VideoFrame m_mappedFrame;
};

class VideoFrameTexturesFromHandlesSet : public VideoFrameTexturesFromRhiTextureArray
{
public:
    VideoFrameTexturesFromHandlesSet(RhiTextureArray &&rhiTextures,
                                      VideoFrameTexturesHandlesUPtr handles);

    ~VideoFrameTexturesFromHandlesSet() override;

    VideoFrameTexturesHandlesUPtr takeHandles() override { return std::move(m_textureHandles); }

private:
    VideoFrameTexturesHandlesUPtr m_textureHandles;
};

}

#endif
