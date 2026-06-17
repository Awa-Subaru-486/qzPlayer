// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_AUDIOALIGNMENTSUPPORT_P_H
#define QT_AUDIO_AUDIOALIGNMENTSUPPORT_P_H

#include <QtCore/qglobal.h>

namespace QtMultimediaPrivate {

template <typename IntType>
inline constexpr bool isPowerOfTwo(IntType arg)
{
    return arg > 0 && (arg & (arg - 1)) == 0;
}

template <typename Type>
constexpr Type alignUp(Type arg, size_t alignment)
{
    if constexpr (std::is_pointer_v<Type>) {
        return Type(alignUp(std::intptr_t(arg), alignment));
    } else {
        Q_ASSERT(isPowerOfTwo(alignment));
        return Type((arg + (Type(alignment) - 1)) & ~Type(alignment - 1));
    }
}

template <typename Type>
constexpr Type alignDown(Type arg, size_t alignment)
{
    if constexpr (std::is_pointer_v<Type>) {
        return Type(alignDown(std::intptr_t(arg), alignment));
    } else {
        Q_ASSERT(isPowerOfTwo(alignment));
        return arg & ~Type(alignment - 1);
    }
}

template <typename IntType>
constexpr bool isAligned(IntType arg, size_t alignment)
{
    if constexpr (std::is_pointer_v<IntType>) {
        return isAligned(std::intptr_t(arg), alignment);
    } else {
        Q_ASSERT(isPowerOfTwo(alignment));
        return (arg & (IntType(alignment) - 1)) == 0;
    }
}

}

#endif
