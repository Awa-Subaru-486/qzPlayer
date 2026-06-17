// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 1) in vec2 coord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 size;
    vec4 radii;
    vec4 u_transform;
    int u_format;
    int _pad;
};

// binding 1: Y (YUV) 或 RGBA
layout(binding = 1) uniform sampler2D tex0;
// binding 2: U (YUV420P) 或 UV (NV12)
layout(binding = 2) uniform sampler2D tex1;
// binding 3: V (YUV420P only)
layout(binding = 3) uniform sampler2D tex2;

// 圆角 SDF
float roundedBoxSDF(vec2 centerPos, vec2 halfSize, float r) {
    return length(max(abs(centerPos) - halfSize + r, 0.0)) - r;
}

// BT.601 YUV→RGB 转换（标准范围）
vec3 yuvToRgb(float y, float u, float v) {
    u -= 0.5;
    v -= 0.5;
    float r = y + 1.402 * v;
    float g = y - 0.344 * u - 0.714 * v;
    float b = y + 1.772 * u;
    return vec3(r, g, b);
}

void main() {
    // 应用 PreserveAspectCrop 填充变换
    vec2 uv = qt_TexCoord0;
    uv = uv * u_transform.xy + u_transform.zw;

    vec4 texColor;

    if (u_format == 0) {
        // RGBA
        texColor = texture(tex0, uv);
    } else if (u_format == 1) {
        // YUV420P: 3 个独立平面
        float y = texture(tex0, uv).r;
        float u = texture(tex1, uv).r;
        float v = texture(tex2, uv).r;
        texColor = vec4(yuvToRgb(y, u, v), 1.0);
    } else {
        // NV12: Y + 交织 UV
        float y = texture(tex0, uv).r;
        vec2 uv_interleaved = texture(tex1, uv).rg;
        texColor = vec4(yuvToRgb(y, uv_interleaved.r, uv_interleaved.g), 1.0);
    }

    // 圆角处理
    vec2 halfSize = size * 0.5;
    vec2 centerPos = coord - halfSize;

    float tl = radii.x;
    float tr = radii.y;
    float bl = radii.z;
    float br = radii.w;

    // 根据当前象限选择圆角半径
    float r;
    if (centerPos.x < 0.0) {
        r = centerPos.y < 0.0 ? bl : tl;
    } else {
        r = centerPos.y < 0.0 ? br : tr;
    }

    float dist = roundedBoxSDF(centerPos, halfSize, r);
    float aa = fwidth(dist);
    float alpha = 1.0 - smoothstep(-aa, aa, dist);

    fragColor = texColor * alpha * qt_Opacity;
}
