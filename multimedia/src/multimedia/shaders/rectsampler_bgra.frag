// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

// 矩形采样BGRA片段着色器：使用sampler2DRect采样BGRA纹理（非归一化坐标），应用透明度和圆角遮罩
#version 440
#extension GL_GOOGLE_include_directive : enable

#include "uniformbuffer.glsl"
#include "colortransfer.glsl"
#include "roundedrect.glsl"

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec2 pixelCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2DRect rgbTexture;

void main()
{
    fragColor = texture(rgbTexture, texCoord).rgba * ubuf.opacity;

    fragColor *= roundedRectAlpha(pixelCoord, ubuf.rectSize, ubuf.radius);

#ifdef QMM_OUTPUTSURFACE_LINEAR
    fragColor = convertSRGBToLinear(fragColor);
#endif
}
