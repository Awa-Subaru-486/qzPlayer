// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "MemoryVideoBuffer_p.h"

MemoryVideoBuffer::MemoryVideoBuffer(QByteArray data, int bytesPerLine)
    : m_bytesPerLine(bytesPerLine), m_data(std::move(data))
{
}

MemoryVideoBuffer::~MemoryVideoBuffer() = default;

AbstractVideoBuffer::MapData MemoryVideoBuffer::map(VideoFrame::MapMode mode)
{
    MapData mapData;

    if (!m_data.isEmpty()) {
        mapData.planeCount = 1;
        mapData.bytesPerLine[0] = m_bytesPerLine;

        if (mode == VideoFrame::ReadOnly)
            mapData.data[0] = reinterpret_cast<uchar *>(const_cast<char *>(m_data.constData()));
        else
            mapData.data[0] = reinterpret_cast<uchar *>(m_data.data());
        mapData.dataSize[0] = m_data.size();
    }

    return mapData;
}

