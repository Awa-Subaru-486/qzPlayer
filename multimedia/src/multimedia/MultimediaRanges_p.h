// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_MULTIMEDIARANGES_P_H
#define QT_MULTIMEDIARANGES_P_H

#include <QtCore/qtconfigmacros.h>

#ifdef __cpp_lib_ranges
#  include <ranges>
#endif

#include <algorithm>

namespace QtMultimediaPrivate::ranges {

#ifdef __cpp_lib_ranges
using std::ranges::equal;
using std::ranges::fill;

#else

constexpr auto equal = [](const auto &lhs, const auto &rhs, auto &&predicate) {
    return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), predicate);
};

constexpr auto fill = [](auto &range, auto &&value) {
    return std::fill(std::begin(range), std::end(range), value);
};

#endif

}

#endif
