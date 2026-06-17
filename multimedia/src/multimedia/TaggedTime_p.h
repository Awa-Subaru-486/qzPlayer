// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_TAGGEDTIME_P_H
#define QT_TAGGEDTIME_P_H

#include "qcompare.h"

template <typename ValueType, typename ThisType, typename AmendedType = ThisType>
class BaseTime
{
public:
    constexpr ValueType get() const noexcept { return m_value; }

    constexpr BaseTime(ValueType value) noexcept : m_value(value) { }
    constexpr BaseTime(const ThisType &other) noexcept : m_value(other.m_value) { }

    constexpr ThisType &operator=(const ThisType &other) noexcept
    {
        m_value = other.m_value;
        return static_cast<ThisType &>(*this);
    }

    friend bool comparesEqual(const ThisType &lhs, const ThisType &rhs) noexcept
    {
        return lhs.m_value == rhs.m_value;
    }

    friend constexpr Qt::strong_ordering compareThreeWay(const ThisType &lhs,
                                                         const ThisType &rhs) noexcept
    {
        return qCompareThreeWay(lhs.m_value, rhs.m_value);
    }

    Q_DECLARE_STRONGLY_ORDERED(ThisType);

    constexpr ThisType operator-() const { return ThisType(-m_value); }

    friend constexpr ThisType operator+(const ThisType &lhs, const AmendedType &rhs) noexcept
    {
        return ThisType(lhs.m_value + rhs.get());
    }

    template <typename T = ThisType, std::enable_if_t<!std::is_same_v<AmendedType, T>> * = nullptr>
    friend constexpr ThisType operator+(const AmendedType &lhs, const ThisType &rhs) noexcept
    {
        return ThisType(lhs.get() + rhs.m_value);
    }

    friend constexpr ThisType operator-(const ThisType &lhs, const AmendedType &rhs) noexcept
    {
        return ThisType(lhs.m_value - rhs.get());
    }

    friend constexpr ThisType &operator+=(ThisType &lhs, const AmendedType &rhs) noexcept
    {
        lhs.m_value += rhs.get();
        return lhs;
    }

    friend constexpr ThisType &operator-=(ThisType &lhs, const AmendedType &rhs) noexcept
    {
        lhs.m_value -= rhs.get();
        return lhs;
    }

private:
    ValueType m_value;
};

template <typename ValueType, typename Tag>
class TaggedTimePoint;

template <typename ValueType, typename Tag>
class TaggedDuration : public BaseTime<ValueType, TaggedDuration<ValueType, Tag>>
{
public:
    using TimePoint = TaggedTimePoint<ValueType, Tag>;
    using BaseTime<ValueType, TaggedDuration>::BaseTime;

    constexpr TimePoint asTimePoint() const noexcept { return TimePoint(this->get()); }
};

template <typename ValueType, typename Tag>
class TaggedTimePoint
    : public BaseTime<ValueType, TaggedTimePoint<ValueType, Tag>, TaggedDuration<ValueType, Tag>>
{
public:
    using Duration = TaggedDuration<ValueType, Tag>;
    using Base = BaseTime<ValueType, TaggedTimePoint, Duration>;
    using Base::Base;

    friend constexpr Duration operator-(const TaggedTimePoint &lhs,
                                        const TaggedTimePoint &rhs) noexcept
    {
        return Duration(lhs.get() - rhs.get());
    }

    constexpr Duration asDuration() const noexcept { return Duration(this->get()); }
};

#endif
