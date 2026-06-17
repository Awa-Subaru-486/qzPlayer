// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_VIDEO_MEMORYVIDEOBUFFER_P_H
#define QT_VIDEO_MEMORYVIDEOBUFFER_P_H
#include "AbstractVideoBuffer.h"

// 内存视频缓冲区：CPU 内存中的视频数据
class QZ_MULTIMEDIA_EXPORT MemoryVideoBuffer : public AbstractVideoBuffer
{
public:
    MemoryVideoBuffer(QByteArray data, int bytesPerLine);
    ~MemoryVideoBuffer() override;

    MapData map(VideoFrame::MapMode mode) override;

    VideoFrameFormat format() const override { return {}; }

private:
    int m_bytesPerLine = 0;
    QByteArray m_data;
};

#endif
