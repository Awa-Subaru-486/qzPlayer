// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.


#ifndef QT_WINDOWS_COMTASKRESOURCE_P_H
#define QT_WINDOWS_COMTASKRESOURCE_P_H
#include <QtCore/qassert.h>

#include <objbase.h>
#include <algorithm>
#include <type_traits>
#include <utility>

class EmptyDeleter final
{
public:
    template<typename T>
    void operator()(T ) const
    {
    }
};

class ComDeleter final
{
public:
    template<typename T>
    void operator()(T element) const
    {
        element->Release();
    }
};

template<typename T>
class ComTaskResourceBase
{
public:
    ComTaskResourceBase(const ComTaskResourceBase<T> &source) = delete;
    ComTaskResourceBase &operator=(const ComTaskResourceBase<T> &right) = delete;

    explicit operator bool() const { return m_resource != nullptr; }

    T *get() const { return m_resource; }

protected:
    ComTaskResourceBase() = default;
    explicit ComTaskResourceBase(T *const resource) : m_resource(resource) { }

    T *release() { return std::exchange(m_resource, nullptr); }

    void reset(T *const resource = nullptr)
    {
        if (m_resource != resource) {
            if (m_resource)
                CoTaskMemFree(m_resource);
            m_resource = resource;
        }
    }

    T *m_resource = nullptr;
};

template<typename T, typename TElementDeleter = EmptyDeleter>
class ComTaskResource final : public ComTaskResourceBase<T>
{
    using Base = ComTaskResourceBase<T>;

public:
    using Base::ComTaskResourceBase;

    ~ComTaskResource() { reset(); }

    T *operator->() const { return m_resource; }
    T &operator*() const { return *m_resource; }

    T **address()
    {
        Q_ASSERT(m_resource == nullptr);
        return &m_resource;
    }

    using Base::release;
    using Base::reset;

private:
    using Base::m_resource;
};

template<typename T, typename TElementDeleter>
class ComTaskResource<T[], TElementDeleter> final : public ComTaskResourceBase<T>
{
    using Base = ComTaskResourceBase<T>;

public:
    ComTaskResource() = default;
    explicit ComTaskResource(T *const resource, const std::size_t size)
        : Base(resource), m_size(size)
    {
    }

    ~ComTaskResource() { reset(); }

    T &operator[](const std::size_t index) const
    {
        Q_ASSERT(index < m_size);
        return m_resource[index];
    }

    T *release()
    {
        m_size = 0;

        return Base::release();
    }

    void reset() { reset(nullptr, 0); }

    void reset(T *const resource, const std::size_t size)
    {
        if (m_resource != resource) {
            resetElements();

            Base::reset(resource);

            m_size = size;
        }
    }

private:
    void resetElements()
    {
        if constexpr (!std::is_same_v<TElementDeleter, EmptyDeleter>) {
            std::for_each(m_resource, m_resource + m_size, TElementDeleter());
        }
    }

    std::size_t m_size = 0;

    using Base::m_resource;
};

#endif
