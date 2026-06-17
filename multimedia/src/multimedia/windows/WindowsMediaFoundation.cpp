// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "WindowsMediaFoundation_p.h"
#include <QtCore/qdebug.h>

namespace {

Q_GLOBAL_STATIC(WindowsMediaFoundation, s_wmf);

template <typename T>
bool setProcAddress(QSystemLibrary &lib, T &f, const char name[])
{
    f = reinterpret_cast<T>(lib.resolve(name));
    return static_cast<bool>(f);
}

}

WindowsMediaFoundation *WindowsMediaFoundation::instance()
{
    if (s_wmf->valid())
        return s_wmf;

    return nullptr;
}

WindowsMediaFoundation::WindowsMediaFoundation()
{
    if (!m_mfplat.load(false))
        return;

    if (!m_mf.load(false))
        return;

    if (!m_mfreadwrite.load(false))
        return;

    m_valid = setProcAddress(m_mfplat, mfStartup, "MFStartup")
            && setProcAddress(m_mfplat, mfShutdown, "MFShutdown")
            && setProcAddress(m_mfplat, mfCreateMediaType, "MFCreateMediaType")
            && setProcAddress(m_mfplat, mfCreateMemoryBuffer, "MFCreateMemoryBuffer")
            && setProcAddress(m_mfplat, mfCreateSample, "MFCreateSample")
            && setProcAddress(m_mfplat, mfCreateAttributes, "MFCreateAttributes")
            && setProcAddress(m_mf, mfEnumDeviceSources, "MFEnumDeviceSources")
            && setProcAddress(m_mfreadwrite, mfCreateSourceReaderFromMediaSource,
                              "MFCreateSourceReaderFromMediaSource");

    Q_ASSERT(m_valid);
}

WindowsMediaFoundation::~WindowsMediaFoundation() = default;

bool WindowsMediaFoundation::valid() const
{
    return m_valid;
}

MFRuntimeInit::MFRuntimeInit(WindowsMediaFoundation *wmf)
    : m_wmf{ wmf }, m_initResult{ wmf ? m_wmf->mfStartup(MF_VERSION, MFSTARTUP_FULL) : E_FAIL }
{
    if (m_initResult != S_OK)
        qErrnoWarning(m_initResult, "Failed to initialize Windows Media Foundation");
}

MFRuntimeInit::~MFRuntimeInit()
{

    if (FAILED(m_initResult))
        return;

    const HRESULT hr = m_wmf->mfShutdown();
    if (hr != S_OK)
        qErrnoWarning(hr, "Failed to shut down Windows Media Foundation");
}

