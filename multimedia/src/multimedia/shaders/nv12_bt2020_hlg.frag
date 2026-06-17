// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

// NV12 BT.2020 HLG格式片段着色器：支持HLG(混合对数伽马)传输函数的HDR视频渲染，含色调映射和Rec.2020到sRGB颜色空间转换
#version 440
#extension GL_GOOGLE_include_directive : enable

#include "uniformbuffer.glsl"
#include "colortransfer.glsl"
#include "colorconvert.glsl"
#include "hdrtonemapper.glsl"
#include "texturecomponent.glsl"
#include "roundedrect.glsl"

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec2 pixelCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D plane1Texture;
layout(binding = 2) uniform sampler2D plane2Texture;

// This implements support HDR video using the HLG transfer functions, see also
//   https://en.wikipedia.org/wiki/Hybrid_log–gamma
//   https://www.itu.int/dms_pub/itu-r/opb/rep/R-REP-BT.2390-6-2019-PDF-E.pdf
//   https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.2100-2-201807-I!!PDF-E.pdf
//
// Tonemapping is done using the same algorithm as for the PQ transfer function, but we
// operate in HLG space here.
void main()
{
    float Y = getR16(plane1Texture, texCoord, ubuf.plane1Format, 1);
    vec2 UV = getRG16(plane2Texture, texCoord, ubuf.plane2Format);
    // map to Rec.2020 color space
    fragColor = vec4(Y, UV.x, UV.y, 1.);
    fragColor = ubuf.colorMatrix * fragColor;

    // tonemap
    // colorRange: 0=Unknown, 1=Video(Limited), 2=Full
    float y = Y;
    if (ubuf.colorRange != 2) {
        // Limited/Video range (16...235) or Unknown (assume Limited)
        y = (Y - 16./256.)*256./219.;
    }
    float scale = tonemapScaleForLuminosity(y, ubuf.masteringWhite, ubuf.maxLum);
    fragColor *= scale;

    fragColor = convertHLGToLinear(fragColor, ubuf.maxLum);
    fragColor = convertRec2020ToSRGB(fragColor);
    fragColor *= ubuf.opacity;

    fragColor *= roundedRectAlpha(pixelCoord, ubuf.rectSize, ubuf.radius);

#ifndef QMM_OUTPUTSURFACE_LINEAR
    fragColor = convertSRGBFromLinear(fragColor);
#endif
}
