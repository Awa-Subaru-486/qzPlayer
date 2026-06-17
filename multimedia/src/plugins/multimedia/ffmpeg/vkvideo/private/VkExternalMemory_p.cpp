#include "../FFmpegHwAccel_vulkan_utils_p.h"

#if QT_FFMPEG_HAS_VULKAN

#include "VkExternalMemory_p.h"
#include "VkImage_p.h"
#include "VkMemory_p.h"

import qzLog;

#ifdef Q_OS_WIN
#include <vulkan/vulkan_win32.h>
#endif

#ifdef Q_OS_ANDROID
#include <vulkan/vulkan_android.h>
#endif

QT_BEGIN_NAMESPACE

namespace ffmpeg {

VkExternalMemory::Handle VkExternalMemory::exportMemory(
    vk::Device device,
    vk::DeviceMemory memory,
    const vk::detail::DispatchLoaderDynamic &dld)
{
    Handle handle;

#ifdef Q_OS_WIN
    vk::MemoryGetWin32HandleInfoKHR handleInfo(
        memory,
        vk::ExternalMemoryHandleTypeFlagBits::eOpaqueWin32);

    try {
        handle.win32Handle = device.getMemoryWin32HandleKHR(handleInfo, dld);
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Bridge: failed to get memory win32 handle: {}", e.what());
    }
#elif defined(Q_OS_ANDROID)
    vk::MemoryGetAndroidHardwareBufferInfoANDROID ahwbInfo(memory);

    try {
        handle.ahwb = device.getMemoryAndroidHardwareBufferANDROID(ahwbInfo, dld);
        if (handle.ahwb) {
            AHardwareBuffer_acquire(handle.ahwb);
        }
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Bridge: failed to get AHardwareBuffer: {}", e.what());
    }
#else
    vk::MemoryGetFdInfoKHR fdInfo(
        memory,
        vk::ExternalMemoryHandleTypeFlagBits::eOpaqueFd);

    try {
        handle.fd = device.getMemoryFdKHR(fdInfo, dld);
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Bridge: failed to get memory fd: {}", e.what());
    }
#endif

    return handle;
}

vk::DeviceMemory VkExternalMemory::importMemory(
    vk::Device device,
    const Handle &handle,
    vk::DeviceSize allocationSize,
    uint32_t memoryTypeIndex,
    const vk::detail::DispatchLoaderDynamic &dld)
{
    vk::MemoryAllocateInfo allocInfo(allocationSize, memoryTypeIndex);

#ifdef Q_OS_WIN
    vk::ImportMemoryWin32HandleInfoKHR importInfo(
        vk::ExternalMemoryHandleTypeFlagBits::eOpaqueWin32,
        handle.win32Handle);
    allocInfo.pNext = &importInfo;
#elif defined(Q_OS_ANDROID)
    vk::ImportAndroidHardwareBufferInfoANDROID importInfo(handle.ahwb);
    allocInfo.pNext = &importInfo;
#else
    vk::ImportMemoryFdInfoKHR importInfo(
        vk::ExternalMemoryHandleTypeFlagBits::eOpaqueFd,
        handle.fd);
    allocInfo.pNext = &importInfo;
#endif

    try {
        return device.allocateMemory(allocInfo, nullptr, dld);
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Bridge: failed to import memory: {}", e.what());
        return nullptr;
    }
}

void VkExternalMemory::closeHandle(Handle &handle)
{
#ifdef Q_OS_WIN
    if (handle.win32Handle) {
        CloseHandle(handle.win32Handle);
        handle.win32Handle = nullptr;
    }
#elif defined(Q_OS_ANDROID)
    if (handle.ahwb) {
        AHardwareBuffer_release(handle.ahwb);
        handle.ahwb = nullptr;
    }
#else
    if (handle.fd >= 0) {
        close(handle.fd);
        handle.fd = -1;
    }
#endif
}

vk::Image VkExternalMemory::createExternalImage(
    vk::Device device,
    vk::PhysicalDevice physicalDevice,
    uint32_t width, uint32_t height,
    vk::Format format,
    vk::ImageUsageFlags usage,
    const vk::detail::DispatchLoaderDynamic &dld)
{
    vk::ExternalMemoryImageCreateInfo externalInfo;

#ifdef Q_OS_WIN
    externalInfo.handleTypes = vk::ExternalMemoryHandleTypeFlagBits::eOpaqueWin32;
#elif defined(Q_OS_ANDROID)
    externalInfo.handleTypes =
        vk::ExternalMemoryHandleTypeFlagBits::eAndroidHardwareBufferANDROID;
#else
    externalInfo.handleTypes = vk::ExternalMemoryHandleTypeFlagBits::eOpaqueFd;
#endif

    vk::DeviceMemory outMemory = nullptr;
    return VkImageWrapper::createOptimal(
        device, physicalDevice,
        width, height, format, usage,
        &externalInfo, nullptr,
        outMemory, dld);
}

} // namespace ffmpeg

QT_END_NAMESPACE

#endif
