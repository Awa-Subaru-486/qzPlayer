// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

// GLES版外部纹理采样片段着色器：使用OES外部纹理扩展(samplerExternalOES)采样Android外部纹理
#extension GL_OES_EGL_image_external : require
precision highp float;
precision highp int;

struct buf
{
    mat4 matrix;
    mat4 colorMatrix;
    float opacity;
    float width;
    float masteringWhite;
    float maxLum;
    int redOrAlphaIndex;
    int plane1Format;
    int plane2Format;
    int plane3Format;
    int colorRange;
    float radius;
    vec2 rectSize;
    vec2 rectOffset;
};

uniform buf ubuf;

uniform samplerExternalOES plane1Texture;

varying vec2 texCoord;
varying vec2 pixelCoord;

float roundedRectSDF(vec2 pc, vec2 size, float r)
{
    vec2 halfSize = size * 0.5;
    vec2 d = abs(pc - halfSize) - halfSize + r;
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - r;
}

float roundedRectAlpha(vec2 pc, vec2 size, float r)
{
    if (r <= 0.0)
        return 1.0;
    float dist = roundedRectSDF(pc, size, r);
    return 1.0 - smoothstep(-1.0, 1.0, dist);
}

void main()
{
    gl_FragColor = texture2D(plane1Texture, texCoord).rgba * ubuf.opacity;

    gl_FragColor *= roundedRectAlpha(pixelCoord, ubuf.rectSize, ubuf.radius);

#ifdef QMM_OUTPUTSURFACE_LINEAR
    gl_FragColor = pow(gl_FragColor, vec4(2.2));
#endif
}
