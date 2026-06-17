// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "MemoryResourceTlsf_p.h"

#include <QtCore/qassert.h>

#include <memory_resource>
#include <cstdlib>
#include <new>

#include <tlsf.h>

namespace QtMultimediaPrivate {

TlsfMemoryResource::TlsfMemoryResource(std::size_t preallocatedBytes, memory_resource *upstream):
    m_allocationSize {
        preallocatedBytes + 8192,
    },
    m_upstream{
        upstream,
    }
{
#ifdef Q_OS_ANDROID
    // Android NDK 不支持 C++17 对齐 new，使用 std::aligned_alloc
    m_buffer = static_cast<std::byte *>(std::aligned_alloc(poolAlignment, m_allocationSize));
#else
    // Windows/Linux 使用 C++17 对齐 new
    m_buffer = reinterpret_cast<std::byte *>(operator new(m_allocationSize, poolAlignment));
#endif

    m_tlsf = QtPrivate::tlsf_create_with_pool(m_buffer, m_allocationSize);
    Q_ASSERT(m_tlsf);
}

TlsfMemoryResource::~TlsfMemoryResource()
{
    QtPrivate::tlsf_destroy(m_tlsf);
#ifdef Q_OS_ANDROID
    std::free(m_buffer);
#else
    ::operator delete(m_buffer, poolAlignment);
#endif
}

void *TlsfMemoryResource::do_allocate(size_t bytes, size_t alignment)
{
    void *ret = QtPrivate::tlsf_memalign(m_tlsf, alignment, bytes);
    if (Q_UNLIKELY(ret == nullptr))
        ret = m_upstream->allocate(bytes, alignment);

    return ret;
}

void TlsfMemoryResource::do_deallocate(void *p, size_t bytes, size_t alignment)
{
    if (Q_LIKELY(p >= m_buffer && p < m_buffer + m_allocationSize))
        QtPrivate::tlsf_free(m_tlsf, p);
    else
        m_upstream->deallocate(p, bytes, alignment);
}

bool TlsfMemoryResource::do_is_equal(const memory_resource &other) const noexcept
{
    return this == &other;
}

}

