// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_WINDOWS_WINDOWSMEDIAFOUNDATION_P_H
#define QT_WINDOWS_WINDOWSMEDIAFOUNDATION_P_H

#include <private/MultimediaGlobal_p.h>
#include <QtCore/private/qsystemlibrary_p.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

// Windows Media Foundation 运行时：动态加载 MF 库并管理函数指针
class WindowsMediaFoundation
{
public:
    static WindowsMediaFoundation *instance();

    WindowsMediaFoundation();
    ~WindowsMediaFoundation();

    // 是否有效
    bool valid() const;

    // MF 函数指针
    decltype(&::MFStartup) mfStartup = nullptr;
    decltype(&::MFShutdown) mfShutdown = nullptr;
    decltype(&::MFCreateMediaType) mfCreateMediaType = nullptr;
    decltype(&::MFCreateMemoryBuffer) mfCreateMemoryBuffer = nullptr;
    decltype(&::MFCreateSample) mfCreateSample = nullptr;
    decltype(&::MFCreateAttributes) mfCreateAttributes = nullptr;
    decltype(&::MFEnumDeviceSources) mfEnumDeviceSources = nullptr;
    decltype(&::MFCreateSourceReaderFromMediaSource) mfCreateSourceReaderFromMediaSource = nullptr;

private:
    QSystemLibrary m_mfplat{ QStringLiteral("Mfplat.dll") };
    QSystemLibrary m_mf{ QStringLiteral("Mf.dll") };
    QSystemLibrary m_mfreadwrite{ QStringLiteral("Mfreadwrite.dll") };
    bool m_valid = false;
};

// MF 运行时初始化辅助类
class MFRuntimeInit
{
    Q_DISABLE_COPY_MOVE(MFRuntimeInit)
public:
    MFRuntimeInit(WindowsMediaFoundation *wmf);
    ~MFRuntimeInit();

private:
    WindowsMediaFoundation *m_wmf;
    HRESULT m_initResult;
};

#endif
