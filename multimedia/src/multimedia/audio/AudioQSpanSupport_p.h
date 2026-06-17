// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_AUDIOQSPANSUPPORT_P_H
#define QT_AUDIO_AUDIOQSPANSUPPORT_P_H

#include <span>
#include <cstddef>

namespace QtMultimediaPrivate {

template <typename U>
inline std::span<U> drop(std::span<U> span, size_t n)
{
    if (n < span.size())
        return span.subspan(n);
    else
        return {};
}

template <typename U>
inline std::span<U> take(std::span<U> span, size_t n)
{
    if (n > span.size())
        return span;
    else
        return span.first(n);
}

}

#endif
