// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

// 位图字幕片段着色器：GPU端调色板查找 + 双线性插值抗锯齿 + 预乘Alpha输出
// 使用 texture() 采样4个相邻纹素（兼容 GLSL ES 100），
// 每个索引独立查找调色板，按双线性权重混合颜色
#version 440

layout(std140, binding = 0) uniform buf {
    mat4 matrix;
    int nbColors;          // 调色板颜色数
    float opacity;
    vec2 texSize;          // 索引纹理尺寸 (像素)
} ubuf;

layout(binding = 1) uniform sampler2D indexTexture;   // R8 索引纹理 (Nearest)
layout(binding = 2) uniform sampler2D paletteTexture;  // BGRA8 调色板纹理 (Nearest)

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;

vec4 lookupPalette(int index) {
    if (index == 0)
        return vec4(0.0);
    float palU = (float(index) + 0.5) / float(max(ubuf.nbColors, 1));
    return texture(paletteTexture, vec2(palU, 0.5));
}

void main() {
    vec2 pixel = texCoord * ubuf.texSize;
    vec2 f = fract(pixel - 0.5);
    vec2 base = floor(pixel - 0.5) + 0.5;

    vec2 invSize = 1.0 / ubuf.texSize;

    vec2 tc00 = base * invSize;
    vec2 tc10 = (base + vec2(1.0, 0.0)) * invSize;
    vec2 tc01 = (base + vec2(0.0, 1.0)) * invSize;
    vec2 tc11 = (base + vec2(1.0, 1.0)) * invSize;

    int idx00 = int(texture(indexTexture, tc00).r * 255.0 + 0.5);
    int idx10 = int(texture(indexTexture, tc10).r * 255.0 + 0.5);
    int idx01 = int(texture(indexTexture, tc01).r * 255.0 + 0.5);
    int idx11 = int(texture(indexTexture, tc11).r * 255.0 + 0.5);

    vec4 c00 = lookupPalette(idx00);
    vec4 c10 = lookupPalette(idx10);
    vec4 c01 = lookupPalette(idx01);
    vec4 c11 = lookupPalette(idx11);

    vec4 color = mix(mix(c00, c10, f.x), mix(c01, c11, f.x), f.y);

    if (color.a < 0.004)
        discard;

    color.a *= ubuf.opacity;

    fragColor = vec4(color.rgb * color.a, color.a);
}
