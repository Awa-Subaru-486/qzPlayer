#ifndef VKIMAGE_P_H
#define VKIMAGE_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegDefs_p.h>

#if QT_FFMPEG_HAS_VULKAN

#include "VkDispatch_p.h"
#include <vulkan/vulkan.hpp>
#include <QSize>

QT_BEGIN_NAMESPACE

namespace ffmpeg {

class VkImageWrapper
{
public:
    // 线性图像，封装 Vulkan 线性图像及多平面布局
    struct LinearImage
    {
        vk::Image image;
        vk::DeviceMemory memory;
        vk::DeviceSize size = 0;
        uint8_t *mappedData = nullptr;
        vk::DeviceSize mappedSize = 0;
        QSize size_;
        vk::Format format = vk::Format::eUndefined;
        int planeCount = 0;
        vk::DeviceSize planeOffsets[3] = {};
        vk::DeviceSize planeSizes[3] = {};
        vk::DeviceSize planeRowPitches[3] = {};
        void *codecContextData = nullptr;
    };

    static std::shared_ptr<LinearImage> createLinear(
        vk::Device device,
        vk::PhysicalDevice physicalDevice,
        const QSize &size,
        vk::Format format,
        int planeCount,
        uint32_t queueFamilyIndex,
        const vk::detail::DispatchLoaderDynamic &dld);

    static void destroyLinear(
        LinearImage &image,
        vk::Device device,
        const vk::detail::DispatchLoaderDynamic &dld);

    static vk::Image createOptimal(
        vk::Device device,
        vk::PhysicalDevice physicalDevice,
        uint32_t width, uint32_t height,
        vk::Format format,
        vk::ImageUsageFlags usage,
        const void *imageCreateInfoPNext,
        const void *memoryAllocPNext,
        vk::DeviceMemory &outMemory,
        const vk::detail::DispatchLoaderDynamic &dld,
        uint64_t externalFormat = 0);

    static vk::ImageView createImageView(
        vk::Device device,
        vk::Image image,
        vk::Format format,
        vk::SamplerYcbcrConversion ycbcrConversion,
        const vk::detail::DispatchLoaderDynamic &dld,
        uint64_t externalFormat = 0);

    static vk::SamplerYcbcrConversion createYCbCrConversion(
        vk::Device device,
        vk::Format format,
        const vk::detail::DispatchLoaderDynamic &dld,
        int colorSpace = 2,
        int colorRange = 1,
        uint64_t externalFormat = 0);
};

} // namespace ffmpeg

QT_END_NAMESPACE

#endif
#endif
