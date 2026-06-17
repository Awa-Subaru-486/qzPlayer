#include "../FFmpegHwAccel_vulkan_utils_p.h"

#if QT_FFMPEG_HAS_VULKAN

#include "VkDeviceContext_p.h"
#include "VkDispatch_p.h"

#include <QtCore/qloggingcategory.h>
#include <rhi/qrhi.h>
#include <QVulkanInstance>

import qzVulkanContext;

QT_BEGIN_NAMESPACE

namespace ffmpeg {

std::shared_ptr<VkDeviceContext> VkDeviceContext::fromQRhi(QRhi *rhi)
{
    if (!rhi || rhi->backend() != QRhi::Vulkan)
        return nullptr;

    auto nativeHandles = static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles());
    if (!nativeHandles || !nativeHandles->inst)
        return nullptr;

    auto ctx = std::make_shared<VkDeviceContext>();

    ctx->m_instance = vk::Instance(nativeHandles->inst->vkInstance());
    ctx->m_physicalDevice = vk::PhysicalDevice(nativeHandles->physDev);
    ctx->m_device = vk::Device(nativeHandles->dev);
    ctx->m_graphicsQueue = vk::Queue(nativeHandles->gfxQueue);
    ctx->m_graphicsQueueFamilyIndex = nativeHandles->gfxQueueFamilyIdx;

    if (!ctx->m_instance || !ctx->m_physicalDevice ||
        !ctx->m_device || !ctx->m_graphicsQueue) {
        return nullptr;
    }

    auto *qzCtx = qzVulkanContext::instance();
    if (qzCtx && qzCtx->isInitialized() &&
        static_cast<VkDevice>(qzCtx->device()) == nativeHandles->dev) {
        ctx->m_dld = qzCtx->dld();
    } else {
        const auto vkInst = nativeHandles->inst->vkInstance();
        const auto getProcAddr =reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                nativeHandles->inst->getInstanceProcAddr("vkGetInstanceProcAddr"));

        if (!getProcAddr)
            return nullptr;

        ctx->m_dld.init(vkInst, getProcAddr);

        VkDispatch::initFromVkInstance(vkInst, getProcAddr);
    }

    ctx->m_valid = true;
    return ctx;
}

std::shared_ptr<VkDeviceContext> VkDeviceContext::fromQVulkanInstance(
    QVulkanInstance *vkInstance, VkDevice device,
    VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex)
{
    if (!vkInstance || device == VK_NULL_HANDLE || physicalDevice == VK_NULL_HANDLE)
        return nullptr;

    auto ctx = std::make_shared<VkDeviceContext>();

    ctx->m_instance = vk::Instance(vkInstance->vkInstance());
    ctx->m_physicalDevice = vk::PhysicalDevice(physicalDevice);
    ctx->m_device = vk::Device(device);
    ctx->m_graphicsQueueFamilyIndex = queueFamilyIndex;

    if (const auto *qzCtx = qzVulkanContext::instance(); qzCtx && qzCtx->isInitialized() &&
        static_cast<VkDevice>(qzCtx->device()) == device) {
        ctx->m_dld = qzCtx->dld();
    } else {
        const auto getProcAddr =
            reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                vkInstance->getInstanceProcAddr("vkGetInstanceProcAddr"));

        if (!getProcAddr)
            return nullptr;

        ctx->m_dld.init(vkInstance->vkInstance(), getProcAddr);

        VkDispatch::initFromQVulkanInstance(vkInstance);
    }

    try {
        ctx->m_graphicsQueue = ctx->m_device.getQueue(queueFamilyIndex, 0, ctx->m_dld);
    } catch (const vk::SystemError &) {
        return nullptr;
    }

    ctx->m_valid = true;
    return ctx;
}

std::shared_ptr<VkDeviceContext> VkDeviceContext::fromQzVulkanContext()
{
    auto *qzCtx = qzVulkanContext::instance();
    if (!qzCtx || !qzCtx->isInitialized())
        return nullptr;

    auto ctx = std::make_shared<VkDeviceContext>();

    ctx->m_instance = vk::Instance(qzCtx->vkInstance());
    ctx->m_physicalDevice = vk::PhysicalDevice(qzCtx->physicalDevice());
    ctx->m_device = vk::Device(qzCtx->device());
    ctx->m_dld = qzCtx->dld();

    // Find graphics queue family
    const auto &queueFamilies = qzCtx->queueFamilies();
    for (const auto &[familyIndex, queueCount, flags, videoOps] : queueFamilies) {
        if (static_cast<VkQueueFlags>(flags) & VK_QUEUE_GRAPHICS_BIT) {
            ctx->m_graphicsQueueFamilyIndex = static_cast<uint32_t>(familyIndex);
            try {
                ctx->m_graphicsQueue = ctx->m_device.getQueue(ctx->m_graphicsQueueFamilyIndex, 0, ctx->m_dld);
            } catch (const vk::SystemError &) {
                return nullptr;
            }
            break;
        }
    }

    if (!ctx->m_instance || !ctx->m_physicalDevice || !ctx->m_device || !ctx->m_graphicsQueue)
        return nullptr;

    ctx->m_valid = true;
    return ctx;
}

} // namespace ffmpeg

QT_END_NAMESPACE

#endif
