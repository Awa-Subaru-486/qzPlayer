// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

// 颜色空间转换库：提供Rec.2020到sRGB的颜色空间转换函数
#ifndef COLORCONVERT
#define COLORCONVERT

// Convert the Rec2020 RGB colorspace to sRGB using:
// https://www.itu.int/dms_pub/itu-r/opb/rep/R-REP-BT.2407-2017-PDF-E.pdf
// We can use the simple matrix defined there for a conversion between BT2020
// and sRGB. Conversion has to be done in linear color space.
vec4 convertRec2020ToSRGB(vec4 rgba)
{
    const mat4 mat =
            mat4(1.6605f, -0.5876f, -0.0728f, 0.0000f,
               -0.1246f,  1.1329f, -0.0083f, 0.0000f,
               -0.0182f, -0.1006f,  1.1187f, 0.0000f,
                0.0000f,  0.0000f,  0.0000f, 1.0000f);

    return rgba * mat;
}

#endif
