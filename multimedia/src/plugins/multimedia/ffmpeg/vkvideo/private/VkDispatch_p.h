#ifndef VKDISPATCH_P_H
#define VKDISPATCH_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegDefs_p.h>

#if QT_FFMPEG_HAS_VULKAN

#include <vulkan/vulkan.hpp>
#include <memory>

QT_BEGIN_NAMESPACE

class QVulkanInstance;

QT_END_NAMESPACE

QT_BEGIN_NAMESPACE

namespace ffmpeg {

// Vulkan 调度器单例，管理 Vulkan 动态加载器
class VkDispatch
{
public:
    static bool initFromQVulkanInstance(QVulkanInstance *vkInstance);
    static bool initFromVkInstance(VkInstance vkInstance, PFN_vkGetInstanceProcAddr getProcAddr);

    static const vk::detail::DispatchLoaderDynamic &dld();
    static bool isInitialized();

private:
    VkDispatch() = default;

    static VkDispatch &instance();

    vk::detail::DispatchLoaderDynamic m_dld;
    bool m_initialized = false;
};

} // namespace ffmpeg

QT_END_NAMESPACE

#endif
#endif
