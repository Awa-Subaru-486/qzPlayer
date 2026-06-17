// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

// 基础顶点着色器：处理顶点位置变换和纹理坐标传递
#version 440
#extension GL_GOOGLE_include_directive : enable

#include "uniformbuffer.glsl"

layout(location = 0) in vec4 vertexPosition;
layout(location = 1) in vec2 vertexTexCoord;

layout(location = 0) out vec2 texCoord;
layout(location = 1) out vec2 pixelCoord;

out gl_PerVertex { vec4 gl_Position; };

void main() {
    texCoord = vertexTexCoord;
    pixelCoord = vertexPosition.xy - ubuf.rectOffset;
    gl_Position = ubuf.matrix * vertexPosition;
}
