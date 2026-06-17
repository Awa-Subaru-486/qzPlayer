// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef WINDOWSPLATFORMSPECIFICINTERFACE_P_H
#define WINDOWSPLATFORMSPECIFICINTERFACE_P_H

#include <qzMultimedia/private/PlatformMediaIntegration_p.h>

class WindowsSmtcManager;

class WindowsPlatformSpecificInterface : public AbstractPlatformSpecificInterface
{
public:
    WindowsPlatformSpecificInterface();
    ~WindowsPlatformSpecificInterface() override;

    WindowsSmtcManager *smtcManager() const { return m_smtcManager.get(); }

private:
    std::unique_ptr<WindowsSmtcManager> m_smtcManager;
};

#endif
