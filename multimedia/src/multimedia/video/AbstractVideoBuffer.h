// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_VIDEO_ABSTRACTVIDEOBUFFER_H
#define QT_VIDEO_ABSTRACTVIDEOBUFFER_H
#include <qzMultimedia/qtmultimediaexports.h>
#include <qzMultimedia/VideoFrame.h>
#include <qzMultimedia/VideoFrameFormat.h>
#include <qzMultimedia/QtVideo.h>

// 抽象视频缓冲区：视频帧数据的底层存储抽象
class QZ_MULTIMEDIA_EXPORT AbstractVideoBuffer
{
public:
    // 映射数据结构：包含平面数、每行字节数、数据指针等
    struct MapData
    {
        int planeCount = 0;
        int bytesPerLine[4] = {};
        uchar *data[4] = {};
        int dataSize[4] = {};
    };

    virtual ~AbstractVideoBuffer();
    // 映射缓冲区到内存
    virtual MapData map(VideoFrame::MapMode mode) = 0;
    // 取消映射
    virtual void unmap();
    // 获取视频格式
    virtual VideoFrameFormat format() const = 0;
};

#endif
