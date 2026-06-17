// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_MEDIAINPUTENCODERINTERFACE_P_H
#define QT_MEDIAINPUTENCODERINTERFACE_P_H

#include <qzMultimedia/MultimediaGlobal.h>

class MediaInputEncoderInterface
{
public:
    virtual ~MediaInputEncoderInterface() = default;
    virtual bool canPushFrame() const = 0;
};

#endif
