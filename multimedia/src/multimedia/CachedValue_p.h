// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_CACHEDVALUE_P_H
#define QT_CACHEDVALUE_P_H

#include <QReadWriteLock>

#include <optional>
#include <unordered_map>

// 缓存值：线程安全的延迟初始化缓存
template <typename T>
class CachedValue
{
public:
    CachedValue() = default;

    Q_DISABLE_COPY(CachedValue)

    // 确保值已初始化，必要时调用 creator
    template <typename Creator>
    T ensure(Creator &&creator)
    {
        {
            QReadLocker locker(&m_lock);
            if (m_cached)
                return *m_cached;
        }

        {
            QWriteLocker locker(&m_lock);
            if (!m_cached)
                m_cached = creator();
            return *m_cached;
        }
    }

    // 更新缓存值
    bool update(T value)
    {
        QWriteLocker locker(&m_lock);
        if (value == m_cached)
            return false;
        auto temp = std::exchange(m_cached, std::move(value));
        locker.unlock();
        return true;
    }

    // 重置缓存
    void reset()
    {
        QWriteLocker locker(&m_lock);
        auto temp = std::exchange(m_cached, std::nullopt);
        locker.unlock();
    }

private:
    QReadWriteLock m_lock;
    std::optional<T> m_cached;
};

// 缓存值映射：线程安全的键值缓存
template <typename Key, typename Value>
class CachedValueMap
{
public:
    CachedValueMap() = default;

    Q_DISABLE_COPY(CachedValueMap)

    // 确保值已初始化，必要时调用 creator
    template <typename Creator>
    Value ensure(const Key &key, Creator &&creator)
    {
        {
            QReadLocker locker(&m_lock);
            auto it = m_map.find(key);
            if (it != m_map.end())
                return it->second;
        }

        {
            QWriteLocker locker(&m_lock);
            auto emplaceRes = m_map.try_emplace(key, creator());
            return emplaceRes.first->second;
        }
    }

private:
    QReadWriteLock m_lock;
    std::unordered_map<Key, Value> m_map;
};

#endif
