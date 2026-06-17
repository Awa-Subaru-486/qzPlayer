#include "../FFmpegHwAccel_vulkan_utils_p.h"

#if QT_FFMPEG_HAS_VULKAN

#include "VkMemory_p.h"

#include <QtCore/qloggingcategory.h>

QT_BEGIN_NAMESPACE

namespace ffmpeg {

uint32_t VkMemory::findMemoryType(
    vk::PhysicalDevice physicalDevice,
    uint32_t memoryTypeBits,
    vk::MemoryPropertyFlags properties,
    const vk::detail::DispatchLoaderDynamic &dld)
{
    auto memProps = physicalDevice.getMemoryProperties(dld);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return UINT32_MAX;
}

uint32_t VkMemory::findDeviceLocalMemoryType(
    vk::PhysicalDevice physicalDevice,
    uint32_t memoryTypeBits,
    const vk::detail::DispatchLoaderDynamic &dld)
{
    return findMemoryType(physicalDevice, memoryTypeBits,
                          vk::MemoryPropertyFlagBits::eDeviceLocal, dld);
}

uint32_t VkMemory::findHostVisibleMemoryType(
    vk::PhysicalDevice physicalDevice,
    uint32_t memoryTypeBits,
    vk::MemoryPropertyFlags extraFlags,
    const vk::detail::DispatchLoaderDynamic &dld)
{
    vk::MemoryPropertyFlags flags = vk::MemoryPropertyFlagBits::eHostVisible |
                                     vk::MemoryPropertyFlagBits::eHostCoherent |
                                     extraFlags;

    uint32_t result = findMemoryType(physicalDevice, memoryTypeBits, flags, dld);
    if (result != UINT32_MAX)
        return result;

    flags = vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent;
    return findMemoryType(physicalDevice, memoryTypeBits, flags, dld);
}

} // namespace ffmpeg

QT_END_NAMESPACE

#endif
