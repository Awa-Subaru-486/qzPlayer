// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_VIDEOTRANSFORMATION_P_H
#define QT_VIDEOTRANSFORMATION_P_H
#include <QtVideo.h>
#include <optional>

// 视频变换：描述视频的旋转和镜像状态
struct VideoTransformation
{
    QtVideo::Rotation rotation = QtVideo::Rotation::None;
    bool mirroredHorizontallyAfterRotation = false;

    // 旋转
    void rotate(QtVideo::Rotation rotation)
    {
        if (rotation != QtVideo::Rotation::None) {
            int angle = qToUnderlying(rotation);
            if (mirroredHorizontallyAfterRotation && angle % 180 != 0)
                angle += 180;

            appendRotation(angle);
        }
    }

    // 水平镜像
    void mirrorHorizontally(bool mirror = true) { mirroredHorizontallyAfterRotation ^= mirror; }

    // 垂直镜像
    void mirrorVertically(bool mirror = true)
    {
        if (mirror) {
            mirroredHorizontallyAfterRotation ^= true;
            appendRotation(180);
        }
    }

    int rotationIndex() const { return qToUnderlying(rotation) / 90; }

private:
    void appendRotation(quint32 angle)
    {
        rotation = QtVideo::Rotation((angle + qToUnderlying(rotation)) % 360);
    }
};

using VideoTransformationOpt = std::optional<VideoTransformation>;

inline bool operator==(const VideoTransformation &lhs, const VideoTransformation &rhs)
{
    return lhs.rotation == rhs.rotation
            && lhs.mirroredHorizontallyAfterRotation == rhs.mirroredHorizontallyAfterRotation;
}

inline bool operator!=(const VideoTransformation &lhs, const VideoTransformation &rhs)
{
    return !(lhs == rhs);
}

QZ_MULTIMEDIA_EXPORT QDebug operator<<(QDebug dbg, const VideoTransformation &transform);

#endif
