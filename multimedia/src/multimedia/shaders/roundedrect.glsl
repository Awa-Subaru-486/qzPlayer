// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

// 圆角矩形库：提供圆角矩形的SDF（有符号距离场）计算和Alpha遮罩生成函数
#ifndef ROUNDEDRECT
#define ROUNDEDRECT

float roundedRectSDF(vec2 pixelCoord, vec2 size, float r)
{
    vec2 halfSize = size * 0.5;
    vec2 d = abs(pixelCoord - halfSize) - halfSize + r;
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - r;
}

float roundedRectAlpha(vec2 pixelCoord, vec2 size, float r)
{
    if (r <= 0.0)
        return 1.0;
    float dist = roundedRectSDF(pixelCoord, size, r);
    return 1.0 - smoothstep(-1.0, 1.0, dist);
}

#endif
