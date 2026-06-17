#include "../FFmpegHwAccel_vulkan_utils_p.h"

#if QT_FFMPEG_HAS_VULKAN

#include "VkDispatch_p.h"

#include <QtCore/qloggingcategory.h>
#include <QVulkanInstance>

import qzVulkanContext;

QT_BEGIN_NAMESPACE

namespace ffmpeg {

VkDispatch &VkDispatch::instance()
{
    static VkDispatch s_dispatch;
    return s_dispatch;
}

bool VkDispatch::initFromQVulkanInstance(QVulkanInstance *vkInstance)
{
    auto &self = instance();
    if (self.m_initialized)
        return true;

    if (!vkInstance)
        return false;

    auto vkInst = vkInstance->vkInstance();
    if (vkInst == VK_NULL_HANDLE)
        return false;

    PFN_vkGetInstanceProcAddr getProcAddr =
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(
            vkInstance->getInstanceProcAddr("vkGetInstanceProcAddr"));

    if (!getProcAddr)
        return false;

    self.m_dld.init(vkInst, getProcAddr);
    self.m_initialized = true;
    return true;
}

bool VkDispatch::initFromVkInstance(VkInstance vkInstance, PFN_vkGetInstanceProcAddr getProcAddr)
{
    auto &self = instance();
    if (self.m_initialized)
        return true;

    if (!getProcAddr || vkInstance == VK_NULL_HANDLE)
        return false;

    self.m_dld.init(vkInstance, getProcAddr);
    self.m_initialized = true;
    return true;
}

const vk::detail::DispatchLoaderDynamic &VkDispatch::dld()
{
    auto *ctx = qzVulkanContext::instance();
    if (ctx && ctx->isInitialized())
        return ctx->dld();

    return instance().m_dld;
}

bool VkDispatch::isInitialized()
{
    auto *ctx = qzVulkanContext::instance();
    if (ctx && ctx->isInitialized())
        return true;

    return instance().m_initialized;
}

} // namespace ffmpeg

QT_END_NAMESPACE

#endif
