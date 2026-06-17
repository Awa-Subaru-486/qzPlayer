#ifndef VKMEMORY_P_H
#define VKMEMORY_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegDefs_p.h>

#if QT_FFMPEG_HAS_VULKAN

#include <vulkan/vulkan.hpp>

QT_BEGIN_NAMESPACE

namespace ffmpeg {

// Vulkan 内存工具类，查找合适的内存类型
class VkMemory
{
public:
    static uint32_t findMemoryType(
        vk::PhysicalDevice physicalDevice,
        uint32_t memoryTypeBits,
        vk::MemoryPropertyFlags properties,
        const vk::detail::DispatchLoaderDynamic &dld);

    static uint32_t findDeviceLocalMemoryType(
        vk::PhysicalDevice physicalDevice,
        uint32_t memoryTypeBits,
        const vk::detail::DispatchLoaderDynamic &dld);

    static uint32_t findHostVisibleMemoryType(
        vk::PhysicalDevice physicalDevice,
        uint32_t memoryTypeBits,
        vk::MemoryPropertyFlags extraFlags,
        const vk::detail::DispatchLoaderDynamic &dld);
};

} // namespace ffmpeg

QT_END_NAMESPACE

#endif
#endif
