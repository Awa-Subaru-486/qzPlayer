// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "private/VideoTransformation_p.h"
#include "qdebug.h"

QDebug operator<<(QDebug dbg, const VideoTransformation &transform)
{
    dbg << "[ rotation:" << transform.rotation
        << "; mirrored:" << transform.mirroredHorizontallyAfterRotation << "]";
    return dbg;
}

