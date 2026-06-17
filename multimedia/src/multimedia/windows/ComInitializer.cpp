// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "ComInitializer_p.h"

#include <QtCore/private/qfunctions_win_p.h>
#include <memory>

namespace {

thread_local std::unique_ptr<QComHelper> s_comHelperRegistry;

}

void ensureComInitializedOnThisThread() {
    if (!s_comHelperRegistry)
        s_comHelperRegistry = std::make_unique<QComHelper>();
}

