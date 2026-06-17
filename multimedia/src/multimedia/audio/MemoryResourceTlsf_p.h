// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_MEMORYRESOURCETLSF_P_H
#define QT_AUDIO_MEMORYRESOURCETLSF_P_H

#include <QtCore/qtclasshelpermacros.h>
#include <qzMultimedia/private/PmrEmulation_p.h>

#include <tlsf.h>

#ifdef Q_OS_ANDROID
#include <cstddef>
#else
#include <new>
#endif

namespace QtMultimediaPrivate {

struct TlsfMemoryResource final : pmr::memory_resource
{
    explicit TlsfMemoryResource(std::size_t preallocatedBytes,
                                 pmr::memory_resource *upstream = pmr::get_default_resource());

    Q_DISABLE_COPY_MOVE(TlsfMemoryResource)

    ~TlsfMemoryResource() override;

private:
    void *do_allocate(size_t bytes, size_t alignment) override;
    void do_deallocate(void *p, size_t bytes, size_t alignment) override;
    bool do_is_equal(const memory_resource &other) const noexcept override;

#ifdef Q_OS_ANDROID
    static constexpr size_t poolAlignment = 128;
#else
    static constexpr auto poolAlignment = std::align_val_t{ 128 };
#endif

    QtPrivate::tlsf_t m_tlsf;
    const size_t m_allocationSize;
    std::byte *m_buffer = nullptr;
    pmr::memory_resource *m_upstream{};
};

}

#endif
