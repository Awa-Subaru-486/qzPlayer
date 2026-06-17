// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "WindowsPlatformSpecificInterface_p.h"
#include "WindowsSmtcManager_p.h"

WindowsPlatformSpecificInterface::WindowsPlatformSpecificInterface()
    : m_smtcManager(std::make_unique<WindowsSmtcManager>())
{
}

WindowsPlatformSpecificInterface::~WindowsPlatformSpecificInterface() = default;
