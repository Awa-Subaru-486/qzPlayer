// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#version 440

layout(location = 0) in vec4 qt_Vertex;
layout(location = 1) in vec2 qt_MultiTexCoord0;
layout(location = 0) out vec2 qt_TexCoord0;
layout(location = 1) out vec2 coord;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 size;
    vec4 radii;
    vec4 u_transform;
    int u_format;
    int _pad;
};

void main() {
    qt_TexCoord0 = qt_MultiTexCoord0;
    coord = qt_MultiTexCoord0 * size;
    vec4 pos = qt_Vertex;
    gl_Position = qt_Matrix * pos;
}
