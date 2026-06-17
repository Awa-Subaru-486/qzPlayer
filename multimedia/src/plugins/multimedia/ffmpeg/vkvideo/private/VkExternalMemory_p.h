#ifndef VKEXTERNALMEMORY_P_H
#define VKEXTERNALMEMORY_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegDefs_p.h>

#if QT_FFMPEG_HAS_VULKAN

#include "VkDispatch_p.h"
#include <vulkan/vulkan.hpp>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifdef Q_OS_ANDROID
#include <android/hardware_buffer.h>
#endif

QT_BEGIN_NAMESPACE

namespace ffmpeg {

// Vulkan 外部内存管理，支持跨设备/跨进程内存导入导出
class VkExternalMemory
{
public:
    // 外部内存句柄，平台相关的内存引用
    struct Handle
    {
#ifdef Q_OS_WIN
        HANDLE win32Handle = nullptr;
#elif defined(Q_OS_ANDROID)
        AHardwareBuffer* ahwb = nullptr;
#else
        int fd = -1;
#endif
    };

    static Handle exportMemory(
        vk::Device device,
        vk::DeviceMemory memory,
        const vk::detail::DispatchLoaderDynamic &dld);

    static vk::DeviceMemory importMemory(
        vk::Device device,
        const Handle &handle,
        vk::DeviceSize allocationSize,
        uint32_t memoryTypeIndex,
        const vk::detail::DispatchLoaderDynamic &dld);

    static void closeHandle(Handle &handle);

    static vk::Image createExternalImage(
        vk::Device device,
        vk::PhysicalDevice physicalDevice,
        uint32_t width, uint32_t height,
        vk::Format format,
        vk::ImageUsageFlags usage,
        const vk::detail::DispatchLoaderDynamic &dld);
};

} // namespace ffmpeg

QT_END_NAMESPACE

#endif
#endif
