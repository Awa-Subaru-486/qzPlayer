// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "SymbolsResolveUtils_p.h"

import qzLog;

static qz::Log::LogCategory qLcSymbolsResolver("qz.multimedia.symbolsresolver");

bool SymbolsResolver::isLazyLoadEnabled()
{
    static const bool lazyLoad =
            !static_cast<bool>(qEnvironmentVariableIntValue("QT_INSTANT_LOAD_FFMPEG_STUBS"));
    return lazyLoad;
}

SymbolsResolver::SymbolsResolver(const char *libLoggingName, LibraryLoader loader)
    : m_libLoggingName(libLoggingName)
{
    Q_ASSERT(libLoggingName);
    Q_ASSERT(loader);

    auto library = loader();
    if (library && library->isLoaded())
        m_library = std::move(library);
    else
        qz::Log::cat_warn(qLcSymbolsResolver, "Couldn't load {} library", m_libLoggingName);
}

SymbolsResolver::SymbolsResolver(const char *libName, const char *version,
                                 const char *libLoggingName)
    : m_libLoggingName(libLoggingName ? libLoggingName : libName)
{
    Q_ASSERT(libName);
    Q_ASSERT(version);

    auto library = std::make_unique<QLibrary>(QString::fromLocal8Bit(libName),
                                              QString::fromLocal8Bit(version));
    if (library->load())
        m_library = std::move(library);
    else
        qz::Log::cat_warn(qLcSymbolsResolver, "Couldn't load {} library", m_libLoggingName);
}

SymbolsResolver::~SymbolsResolver()
{
    if (m_library)
        m_library->unload();
}

QFunctionPointer SymbolsResolver::initOptionalFunction(const char *funcName)
{
    return m_library ? m_library->resolve(funcName) : nullptr;
}

QFunctionPointer SymbolsResolver::initFunction(const char *funcName)
{
    QFunctionPointer func = initOptionalFunction(funcName);

    if (!func && m_library)
    {
        qz::Log::cat_warn(qLcSymbolsResolver, "Couldn't resolve {} symbol {}", m_libLoggingName, funcName);
        m_library->unload();
        m_library.reset();
    }

    return func;
}

void SymbolsResolver::checkLibrariesLoaded(SymbolsMarker *begin, SymbolsMarker *end)
{
    if (m_library) {
        qz::Log::cat_debug(qLcSymbolsResolver, "{} symbols resolved", m_libLoggingName);
    } else {
        const auto size = reinterpret_cast<char *>(end) - reinterpret_cast<char *>(begin);
        memset(begin, 0, size);
        qz::Log::cat_warn(qLcSymbolsResolver, "Couldn't resolve {} symbols", m_libLoggingName);
    }
}

