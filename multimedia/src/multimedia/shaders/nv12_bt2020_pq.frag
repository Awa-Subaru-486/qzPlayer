// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

// NV12 BT.2020 PQ格式片段着色器：支持PQ(感知量化器)传输函数的HDR视频渲染，含色调映射和Rec.2020到sRGB颜色空间转换
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

// This uses the PQ transfer function, see also https://en.wikipedia.org/wiki/Perceptual_quantizer
// or https://ieeexplore.ieee.org/document/7291452
//
// Tonemapping into the RGB range supported by the output is done using
// https://www.itu.int/dms_pub/itu-r/opb/rep/R-REP-BT.2390-6-2019-PDF-E.pdf, section 5.4
//
// masteringWhite in PQ values, not in linear. maxOutLum as defined in the doc above
// we assume that masteringBlack == 0, and minLum == 0 to simplify the calculations
//
// The calculation calculates a new luminosity in non linear space and scales the UV
// components before linearizing. This corresponds to option (2) at the end of section 5.4.
// This option was chosen as it keeps the colors correct while as well as being computationally
// cheapest.
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

    fragColor = convertPQToLinear(fragColor);
    fragColor = convertRec2020ToSRGB(fragColor);
    fragColor *= ubuf.opacity;

    fragColor *= roundedRectAlpha(pixelCoord, ubuf.rectSize, ubuf.radius);

#ifndef QMM_OUTPUTSURFACE_LINEAR
    fragColor = convertSRGBFromLinear(fragColor);
#endif
}
