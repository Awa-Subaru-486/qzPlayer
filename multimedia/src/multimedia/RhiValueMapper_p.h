// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_RHIVALUEMAPPER_P_H
#define QT_RHIVALUEMAPPER_P_H

#include <qzMultimedia/MultimediaGlobal.h>

#include <qreadwritelock.h>

#include <memory>
#include <map>

class QRhi;

class QZ_MULTIMEDIA_EXPORT RhiCallback : public std::enable_shared_from_this<RhiCallback>
{
public:
    class Manager;

    RhiCallback();
    virtual ~RhiCallback();

    void registerCallback(QRhi &rhi);

protected:
    virtual void onRhiCleanup(QRhi &rhi) = 0;

private:
    std::shared_ptr<Manager> m_manager;
};

template <typename Value>
class RhiValueMapper
{
    struct Data : RhiCallback
    {
        QReadWriteLock lock;

        std::map<QRhi *, Value> storage;

        void onRhiCleanup(QRhi &rhi) override
        {
            QWriteLocker locker(&lock);
            storage.erase(&rhi);
        }
    };

public:
    ~RhiValueMapper()
    {
        clear();
    }

    Q_DISABLE_COPY(RhiValueMapper);

    RhiValueMapper(RhiValueMapper&& ) noexcept = default;
    RhiValueMapper& operator = (RhiValueMapper&&) noexcept = default;

    RhiValueMapper() : m_data(std::make_shared<Data>()) { }

    template <typename V>
    std::pair<Value *, bool> tryMap(QRhi &rhi, V &&value)
    {
        QWriteLocker locker(&m_data->lock);

        auto [rhiIt, rhiAdded] = m_data->storage.try_emplace(&rhi, std::forward<V>(value));

        if (rhiAdded)
            m_data->registerCallback(rhi);

        return { &rhiIt->second, rhiAdded };
    }

    Value *get(QRhi *rhi) const
    {
        QReadLocker locker(&m_data->lock);
        auto rhiIt = m_data->storage.find(rhi);
        return rhiIt == m_data->storage.end() ? nullptr : &rhiIt->second;
    }

    void clear()
    {
        if (!m_data)
            return;
        QWriteLocker locker(&m_data->lock);
        m_data->storage.clear();
    }

    template <typename Predicate>
    QRhi *findRhi(Predicate &&p) const
    {
        QReadLocker locker(&m_data->lock);
        auto &storage = m_data->storage;

        auto it = std::find_if(storage.begin(), storage.end(),
                               [&p](auto &rhiItem) { return p(*rhiItem.first); });
        return it == storage.end() ? nullptr : it->first;
    }

private:
    std::shared_ptr<Data> m_data;
};

#endif
