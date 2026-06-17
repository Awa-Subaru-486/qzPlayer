// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "SubtitleStyle.h"

static void registerSubtitleStyle()
{
    qRegisterMetaType<SubtitleStyle>();
}

Q_CONSTRUCTOR_FUNCTION(registerSubtitleStyle)
