// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "RhiValueMapper_p.h"

#include <qmutex.h>
#include <rhi/qrhi.h>
#include <q20vector.h>

#include <vector>

class RhiCallback::Manager : public std::enable_shared_from_this<Manager>
{
    using CallbackList = std::vector<std::weak_ptr<RhiCallback>>;
    struct CallbacksItem
    {
        CallbackList callbacks;
        size_t lastValidCallbackCount = 1u;

        void addCallback(const std::weak_ptr<RhiCallback> &cb)
        {
            Q_ASSERT(!cb.expired());

            if (callbacks.size() > lastValidCallbackCount * 2) {
                q20::erase_if(callbacks, [](const auto &cb) {
                    return cb.expired();
                });
                lastValidCallbackCount = callbacks.size() + 1;
            }

            callbacks.push_back(cb);
        }
    };

public:
    void registerCallback(QRhi &rhi, const std::weak_ptr<RhiCallback> &cb)
    {
        QMutexLocker locker(&m_mutex);
        auto [rhiIt, added] = m_rhiToCallbackItems.try_emplace(&rhi, CallbacksItem{});
        if (added)
            rhi.addCleanupCallback([instance = shared_from_this()](QRhi *rhi) {
                for (auto &weakCb : instance->extractCallbacks(rhi))
                    if (auto cb = weakCb.lock())
                        cb->onRhiCleanup(*rhi);
            });

        rhiIt->second.addCallback(cb);
    }

private:
    CallbackList extractCallbacks(QRhi *rhi)
    {
        QMutexLocker locker(&m_mutex);
        auto it = m_rhiToCallbackItems.find(rhi);
        Q_ASSERT(it != m_rhiToCallbackItems.end());

        CallbackList result = std::move(it->second.callbacks);
        m_rhiToCallbackItems.erase(it);
        return result;
    }

private:
    std::unordered_map<QRhi *, CallbacksItem> m_rhiToCallbackItems;
    QBasicMutex m_mutex;
};

Q_GLOBAL_STATIC(std::shared_ptr<RhiCallback::Manager>, rhiCallbacksStorage,
                std::make_shared<RhiCallback::Manager>());

RhiCallback::RhiCallback() : m_manager(*rhiCallbacksStorage) { }

RhiCallback::~RhiCallback() = default;

void RhiCallback::registerCallback(QRhi &rhi)
{
    m_manager->registerCallback(rhi, weak_from_this());
}

