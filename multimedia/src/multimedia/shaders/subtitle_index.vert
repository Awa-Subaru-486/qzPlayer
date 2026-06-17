// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

// 位图字幕顶点着色器
#version 440

layout(std140, binding = 0) uniform buf {
    mat4 matrix;
    int nbColors;          // 调色板颜色数
    float opacity;
} ubuf;

layout(location = 0) in vec4 qt_Vertex;
layout(location = 1) in vec2 qt_MultiTexCoord0;

layout(location = 0) out vec2 texCoord;

void main() {
    texCoord = qt_MultiTexCoord0;
    gl_Position = ubuf.matrix * qt_Vertex;
}
