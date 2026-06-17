// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_VIDEO_IMAGEVIDEOBUFFER_P_H
#define QT_VIDEO_IMAGEVIDEOBUFFER_P_H
#include <AbstractVideoBuffer.h>
#include <qimage.h>

class QZ_MULTIMEDIA_EXPORT ImageVideoBuffer : public AbstractVideoBuffer
{
public:
    ImageVideoBuffer(QImage image);

    MapData map(VideoFrame::MapMode mode) override;

    VideoFrameFormat format() const override { return {}; }

    QImage underlyingImage() const;

private:
    QImage m_image;
};

#endif
