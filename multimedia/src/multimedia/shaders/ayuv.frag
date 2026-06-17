// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

// AYUV像素格式片段着色器：从单平面中提取YUV和Alpha分量，通过颜色矩阵转换为RGB
#version 440
#extension GL_GOOGLE_include_directive : enable

#include "uniformbuffer.glsl"
#include "colortransfer.glsl"
#include "roundedrect.glsl"

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec2 pixelCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D plane1Texture;

void main()
{
    vec3 YUV = texture(plane1Texture, texCoord).gba;
    float A = texture(plane1Texture, texCoord).r;
    fragColor = ubuf.colorMatrix * vec4(YUV, 1.0) * A * ubuf.opacity;

#ifdef QMM_OUTPUTSURFACE_LINEAR
    fragColor = convertSRGBToLinear(fragColor);
#endif

    // Clamp output to valid range to account for out-of-range
    // input values and numerical inaccuracies in YUV->RGB conversion
    fragColor = clamp(fragColor, 0.0, 1.0);

    fragColor *= roundedRectAlpha(pixelCoord, ubuf.rectSize, ubuf.radius);
}
