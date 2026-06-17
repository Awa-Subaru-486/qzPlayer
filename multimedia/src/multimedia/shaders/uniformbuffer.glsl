// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

// 统一缓冲区定义：定义着色器共享的uniform数据结构（变换矩阵、颜色矩阵、HDR元数据、圆角遮罩参数等）
// Make sure to also modify externalsampler_gles.frag when modifying this

#ifndef UNIFORMBUFFER
#define UNIFORMBUFFER

layout(std140, binding = 0) uniform buf {
    mat4 matrix;
    mat4 colorMatrix;
    float opacity;
    float width;
    // HDR metadata required for tonemapping
    float masteringWhite; // in PQ or HLG values
    float maxLum; // in PQ or HLG values
    int redOrAlphaIndex; // index of the RED_OR_ALPHA component
    int plane1Format; // Rhi texture format of the 1st plane
    int plane2Format; // Rhi texture format of the 2nd plane
    int plane3Format; // Rhi texture format of the 3rd plane
    int colorRange; // 0: Unknown, 1: Video (Limited), 2: Full
    // Rounded rectangle mask
    float radius;    // corner radius in pixels, 0 = no rounding
    vec2 rectSize;   // rendered rectangle size in pixels
    vec2 rectOffset; // rendered rectangle offset in pixels
} ubuf;

#endif
