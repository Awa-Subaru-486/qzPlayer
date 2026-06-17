// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "ThreadLocalRhi_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/qpa/qplatformintegration.h>

#if QT_CONFIG(vulkan)
#include <QVulkanInstance>
#include <rhi/qrhi.h>
#endif

import qzLog;
import qzVulkanContext;

namespace {

static thread_local QRhi::Implementation s_preferredBackend = QRhi::Null;

class ThreadLocalRhiHolder
{
public:
    ThreadLocalRhiHolder();
    ~ThreadLocalRhiHolder() { resetRhi(); }

    QRhi *ensureRhi(const QRhi *referenceRhi)
    {
        if (m_rhi || m_cpuOnly)
            return m_rhi.get();

        const QRhi::Implementation referenceBackend = referenceRhi ? referenceRhi->backend() : QRhi::Null;
        const QPlatformIntegration *qpa = QGuiApplicationPrivate::platformIntegration();

        if (qpa && qpa->hasCapability(QPlatformIntegration::RhiBasedRendering))
        {

#if defined(Q_OS_WIN)
            if (!m_rhi && canUseRhiImpl(QRhi::D3D11, referenceBackend))
            {
                QRhiD3D11InitParams params;
                m_rhi.reset(QRhi::create(QRhi::D3D11, &params));
            }
#endif

#if QT_CONFIG(vulkan)
            if (!m_rhi && canUseRhiImpl(QRhi::Vulkan, referenceBackend))
            {
                // Try to use qzVulkanContext's VkDevice for zero-copy video decode.
                // When the RHI uses the same VkDevice as FFmpeg's Vulkan decoder,
                // decoded frames can be directly sampled without cross-device copies.
                auto *vkContext = qzVulkanContext::instance();
                if (vkContext && vkContext->isInitialized()) {
                    if (!m_vulkanInstance) {
                        m_vulkanInstance = std::make_unique<QVulkanInstance>();
                        m_vulkanInstance->setVkInstance(vkContext->vkInstance());
                        if (!m_vulkanInstance->create()) {
                            qz::Log::warn("{}: Failed to create QVulkanInstance from qzVulkanContext", Q_FUNC_INFO);
                            m_vulkanInstance.reset();
                        }
                    }

                    if (m_vulkanInstance) {
                        QRhiVulkanInitParams params;
                        params.inst = m_vulkanInstance.get();
                        params.window = nullptr;

                        // Import qzVulkanContext's VkDevice so RHI shares the same device
                        // as the FFmpeg Vulkan decoder, enabling zero-copy rendering.
                        QRhiVulkanNativeHandles importHandles;
                        importHandles.physDev = vkContext->physicalDevice();
                        importHandles.dev = vkContext->device();

                        // Find graphics queue family index
                        for (const auto &family : vkContext->queueFamilies()) {
                            if (static_cast<VkQueueFlags>(family.flags) & VK_QUEUE_GRAPHICS_BIT) {
                                importHandles.gfxQueueFamilyIdx = family.familyIndex;
                                break;
                            }
                        }
                        importHandles.gfxQueueIdx = 0;

                        m_rhi.reset(QRhi::create(QRhi::Vulkan, &params, {}, &importHandles));
                        if (m_rhi) {
                            qz::Log::info("{}: Created Vulkan RHI with imported qzVulkanContext device (zero-copy enabled)", Q_FUNC_INFO);
                        } else {
                            qz::Log::warn("{}: Failed to create Vulkan RHI with imported device, falling back", Q_FUNC_INFO);
                        }
                    }
                }

                // Fallback: create independent Vulkan RHI if qzVulkanContext is not available
                if (!m_rhi) {
                    if (!m_vulkanInstance) {
                        m_vulkanInstance = std::make_unique<QVulkanInstance>();
                        m_vulkanInstance->setExtensions(QRhiVulkanInitParams::preferredInstanceExtensions());
                        if (!m_vulkanInstance->create()) {
                            qz::Log::warn("{}: Failed to create QVulkanInstance for thread-local RHI", Q_FUNC_INFO);
                            m_vulkanInstance.reset();
                        }
                    }

                    if (m_vulkanInstance) {
                        QRhiVulkanInitParams params;
                        params.inst = m_vulkanInstance.get();
                        params.window = nullptr;
                        m_rhi.reset(QRhi::create(QRhi::Vulkan, &params));
                    }
                }
            }
#endif
        }

        if (!m_rhi)
        {
            m_cpuOnly = true;
            qz::Log::warn("{}: No RHI backend. Using CPU conversion.", Q_FUNC_INFO);
        }

        return m_rhi.get();
    }

    void resetRhi()
    {
        m_rhi.reset();
        m_vulkanInstance.reset();
        m_cpuOnly = false;
    }

    bool canUseRhiImpl(const QRhi::Implementation implementation, const QRhi::Implementation reference)
    {
        if (reference != QRhi::Null)
            return implementation == reference;

        if (s_preferredBackend != QRhi::Null)
            return implementation == s_preferredBackend;

        return true;
    }

private:
    std::unique_ptr<QRhi> m_rhi;
#if QT_CONFIG(vulkan)
    std::unique_ptr<QVulkanInstance> m_vulkanInstance;
#endif
    bool m_cpuOnly = false;
};

Q_CONSTINIT thread_local std::optional<ThreadLocalRhiHolder> g_threadLocalRhiHolder;

ThreadLocalRhiHolder::ThreadLocalRhiHolder()
{
    if (QThread::isMainThread())
    {
        qAddPostRoutine([] { g_threadLocalRhiHolder.reset(); });
    }
}
}

QRhi *qEnsureThreadLocalRhi(QRhi *referenceRhi)
{
    if (!g_threadLocalRhiHolder)
        g_threadLocalRhiHolder.emplace();

    return g_threadLocalRhiHolder->ensureRhi(referenceRhi);
}

void qSetPreferredThreadLocalRhiBackend(QRhi::Implementation backend)
{
    s_preferredBackend = backend;
    if (g_threadLocalRhiHolder)
        g_threadLocalRhiHolder->resetRhi();
}

