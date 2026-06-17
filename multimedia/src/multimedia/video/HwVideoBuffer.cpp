// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "HwVideoBuffer_p.h"

VideoFrameTextures::~VideoFrameTextures() = default;

VideoFrameTexturesHandles::~VideoFrameTexturesHandles() = default;

HwVideoBuffer::HwVideoBuffer(VideoFrame::HandleType type, QRhi *rhi) : m_type(type), m_rhi(rhi)
{
}

HwVideoBuffer::~HwVideoBuffer() = default;

