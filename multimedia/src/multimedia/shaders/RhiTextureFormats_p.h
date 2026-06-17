// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_SHADERS_RHITEXTUREFORMATS_P_H
#define QT_SHADERS_RHITEXTUREFORMATS_P_H

const int RhiTextureFormat_RGBA8 = 1;
const int RhiTextureFormat_BGRA8 = 2;
const int RhiTextureFormat_R8 = 3;
const int RhiTextureFormat_RG8 = 4;
const int RhiTextureFormat_R16 = 5;
const int RhiTextureFormat_RG16 = 6;
const int RhiTextureFormat_RED_OR_ALPHA8 = 7;

#ifdef __cplusplus
#include <QtGui/rhi/qrhi.h>

static_assert(QRhiTexture::RGBA8 == RhiTextureFormat_RGBA8, "Incompatible RGBA8 value between c++ and shaders");
static_assert(QRhiTexture::BGRA8 == RhiTextureFormat_BGRA8, "Incompatible BGRA8 value between c++ and shaders");
static_assert(QRhiTexture::R8 == RhiTextureFormat_R8, "Incompatible R8 value between c++ and and shaders");
static_assert(QRhiTexture::RG8 == RhiTextureFormat_RG8, "Incompatible RG8 value between c++ and shaders");
static_assert(QRhiTexture::RG16 == RhiTextureFormat_RG16, "Incompatible RG16 value between c++ and shaders");
static_assert(QRhiTexture::RED_OR_ALPHA8 == RhiTextureFormat_RED_OR_ALPHA8, "Incompatible RED_OR_ALPHA value between c++ and shaders");

#endif

#endif
