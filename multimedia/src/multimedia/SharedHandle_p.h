// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_SHAREDHANDLE_P_H
#define QT_SHAREDHANDLE_P_H
#include <QtCore/private/quniquehandle_p.h>
#include <QtCore/qtconfigmacros.h>
#include <QtCore/qcompare.h>

#if __cpp_lib_concepts
#  include <concepts>
#endif

namespace QtPrivate {

#if __cpp_lib_concepts

template <typename T>
concept SharedHandleTraitsConcept = requires
{
    typename T::Type;

    { T::invalidValue() } noexcept -> std::same_as<typename T::Type>;
    { T::ref(std::declval<typename T::Type>()) } -> std::same_as<typename T::Type>;
    { T::unref(std::declval<typename T::Type>()) } -> std::same_as<bool>;
};

#endif

#if __cpp_lib_concepts
template <SharedHandleTraitsConcept SharedHandleTraits>
#else
template <typename SharedHandleTraits>
#endif
struct UniqueHandleTraitsFromSharedHandleTraits
{
    using Type = typename SharedHandleTraits::Type;

    [[nodiscard]] static Type invalidValue() noexcept(noexcept(SharedHandleTraits::invalidValue()))
    {
        return SharedHandleTraits::invalidValue();
    }

    [[nodiscard]] static bool
    close(Type handle) noexcept(noexcept(SharedHandleTraits::unref(handle)))
    {
        return SharedHandleTraits::unref(handle);
    }
};

#if __cpp_lib_concepts
template <SharedHandleTraitsConcept HandleTraits>
#else
template <typename HandleTraits>
#endif
struct SharedHandle : private QUniqueHandle<UniqueHandleTraitsFromSharedHandleTraits<HandleTraits>>
{
private:
    using BaseClass = QUniqueHandle<UniqueHandleTraitsFromSharedHandleTraits<HandleTraits>>;

    static constexpr bool BaseResetIsNoexcept =
            noexcept(std::declval<BaseClass>().reset(std::declval<typename HandleTraits::Type>()));

    static constexpr bool RefIsNoexcept =
            noexcept(HandleTraits::ref(std::declval<typename HandleTraits::Type>()));

    static constexpr bool BaseMoveIsNoexcept =
            noexcept(std::declval<BaseClass>().operator=(std::move(std::declval<BaseClass>())));

public:
    using typename BaseClass::Type;

    enum RefMode : uint8_t
    {
        HasRef,
        NeedsRef,

        AddRef = NeedsRef,
        NoAddRef = HasRef,
    };

    SharedHandle() = default;

    explicit SharedHandle(typename HandleTraits::Type object, RefMode mode)
        : BaseClass{ mode == NeedsRef ? HandleTraits::ref(object) : object }
    {
    }

    SharedHandle(const SharedHandle &o)
        : BaseClass{
              HandleTraits::ref(o.get()),
          }
    {
    }

    SharedHandle(SharedHandle &&) noexcept = default;

    SharedHandle &operator=(const SharedHandle &o) noexcept(RefIsNoexcept && BaseResetIsNoexcept)
    {
        if (BaseClass::get() != o.get())
            BaseClass::reset(HandleTraits::ref(o.get()));
        return *this;
    };

    SharedHandle &operator=(SharedHandle &&o) noexcept(BaseMoveIsNoexcept)
    {
        BaseClass::operator=(std::forward<SharedHandle>(o));
        return *this;
    }

    void reset(typename HandleTraits::Type o,
               RefMode mode) noexcept(RefIsNoexcept && BaseResetIsNoexcept)
    {
        if (mode == NeedsRef)
            BaseClass::reset(HandleTraits::ref(o));
        else
            BaseClass::reset(o);
    }

    void reset() noexcept(BaseResetIsNoexcept) { BaseClass::reset(); }

    Q_DECLARE_STRONGLY_ORDERED(SharedHandle);

    using BaseClass::get;
    using BaseClass::isValid;
    using BaseClass::operator bool;
    using BaseClass::release;
    using BaseClass::operator&;

    void swap(SharedHandle &other) noexcept(noexcept(std::declval<BaseClass>().swap(other)))
    {
        BaseClass::swap(other);
    }
};

template <typename Trait>
void swap(SharedHandle<Trait> &lhs, SharedHandle<Trait> &rhs) noexcept(noexcept(lhs.swap(rhs)))
{
    lhs.swap(rhs);
}

}

#endif
