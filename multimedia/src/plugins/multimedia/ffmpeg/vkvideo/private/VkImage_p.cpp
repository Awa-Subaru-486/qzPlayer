#include "../FFmpegHwAccel_vulkan_utils_p.h"

#if QT_FFMPEG_HAS_VULKAN

#include "VkImage_p.h"
#include "VkMemory_p.h"

import qzLog;

#ifdef Q_OS_ANDROID
#include <vulkan/vulkan_android.h>
#endif

extern "C" {
#include <libavutil/pixfmt.h>
}

QT_BEGIN_NAMESPACE

namespace ffmpeg {

std::shared_ptr<VkImageWrapper::LinearImage> VkImageWrapper::createLinear(
    vk::Device device,
    vk::PhysicalDevice physicalDevice,
    const QSize &size,
    vk::Format format,
    int planeCount,
    uint32_t queueFamilyIndex,
    const vk::detail::DispatchLoaderDynamic &dld)
{
    auto image = std::make_shared<LinearImage>();

    vk::ImageCreateInfo imageInfo(
        {},
        vk::ImageType::e2D,
        format,
        vk::Extent3D(static_cast<uint32_t>(size.width()), static_cast<uint32_t>(size.height()), 1),
        1, 1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eLinear,
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled,
        vk::SharingMode::eExclusive,
        1, &queueFamilyIndex,
        vk::ImageLayout::ePreinitialized);

    try {
        image->image = device.createImage(imageInfo, nullptr, dld);
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to create linear image: {}", e.what());
        return nullptr;
    }

    auto memReqs = device.getImageMemoryRequirements(image->image, dld);

    uint32_t memoryTypeIndex = VkMemory::findHostVisibleMemoryType(
        physicalDevice, memReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostCached, dld);

    if (memoryTypeIndex == UINT32_MAX) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to find suitable memory type for linear image");
        device.destroyImage(image->image, nullptr, dld);
        return nullptr;
    }

    vk::MemoryAllocateInfo allocInfo(memReqs.size, memoryTypeIndex);

    try {
        image->memory = device.allocateMemory(allocInfo, nullptr, dld);
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to allocate linear image memory: {}", e.what());
        device.destroyImage(image->image, nullptr, dld);
        return nullptr;
    }

    try {
        device.bindImageMemory(image->image, image->memory, 0, dld);
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to bind linear image memory: {}", e.what());
        device.freeMemory(image->memory, nullptr, dld);
        device.destroyImage(image->image, nullptr, dld);
        return nullptr;
    }

    image->size = memReqs.size;
    image->size_ = size;
    image->format = format;
    image->planeCount = planeCount;

    try {
        image->mappedData = static_cast<uint8_t *>(device.mapMemory(image->memory, 0, memReqs.size, {}, dld));
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to map linear image memory: {}", e.what());
        device.freeMemory(image->memory, nullptr, dld);
        device.destroyImage(image->image, nullptr, dld);
        return nullptr;
    }
    image->mappedSize = memReqs.size;

    vk::ImageSubresource subresource(vk::ImageAspectFlagBits::eColor, 0, 0);
    auto layout = device.getImageSubresourceLayout(image->image, subresource, dld);

    image->planeOffsets[0] = layout.offset;
    image->planeRowPitches[0] = layout.rowPitch;
    image->planeSizes[0] = layout.size;

    return image;
}

void VkImageWrapper::destroyLinear(
    LinearImage &image,
    vk::Device device,
    const vk::detail::DispatchLoaderDynamic &dld)
{
    if (image.mappedData) {
        device.unmapMemory(image.memory, dld);
        image.mappedData = nullptr;
    }

    if (image.memory) {
        device.freeMemory(image.memory, nullptr, dld);
        image.memory = nullptr;
    }

    if (image.image) {
        device.destroyImage(image.image, nullptr, dld);
        image.image = nullptr;
    }
}

vk::Image VkImageWrapper::createOptimal(
    vk::Device device,
    vk::PhysicalDevice physicalDevice,
    uint32_t width, uint32_t height,
    vk::Format format,
    vk::ImageUsageFlags usage,
    const void *imageCreateInfoPNext,
    const void *memoryAllocPNext,
    vk::DeviceMemory &outMemory,
    const vk::detail::DispatchLoaderDynamic &dld,
    uint64_t externalFormat)
{
    outMemory = nullptr;

#ifdef Q_OS_ANDROID
    vk::ExternalFormatANDROID externalFormatInfo;
    if (format == vk::Format::eUndefined && externalFormat != 0) {
        externalFormatInfo.externalFormat = externalFormat;
        externalFormatInfo.pNext = const_cast<void *>(imageCreateInfoPNext);
        imageCreateInfoPNext = &externalFormatInfo;
    }
#endif

    vk::ImageCreateInfo imageInfo(
        {},
        vk::ImageType::e2D,
        format,
        vk::Extent3D(width, height, 1),
        1, 1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        usage,
        vk::SharingMode::eExclusive);

    imageInfo.pNext = imageCreateInfoPNext;

    vk::Image image;
    try {
        image = device.createImage(imageInfo, nullptr, dld);
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to create optimal image: {}", e.what());
        return nullptr;
    }

    auto memReqs = device.getImageMemoryRequirements(image, dld);

    uint32_t memoryTypeIndex = VkMemory::findDeviceLocalMemoryType(
        physicalDevice, memReqs.memoryTypeBits, dld);
    if (memoryTypeIndex == UINT32_MAX) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to find device local memory type");
        device.destroyImage(image, nullptr, dld);
        return nullptr;
    }

    vk::MemoryAllocateInfo allocInfo(memReqs.size, memoryTypeIndex);
    allocInfo.pNext = memoryAllocPNext;

    try {
        outMemory = device.allocateMemory(allocInfo, nullptr, dld);
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to allocate image memory: {}", e.what());
        device.destroyImage(image, nullptr, dld);
        return nullptr;
    }

    try {
        device.bindImageMemory(image, outMemory, 0, dld);
    } catch (const vk::SystemError &) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to bind image memory");
        device.freeMemory(outMemory, nullptr, dld);
        device.destroyImage(image, nullptr, dld);
        outMemory = nullptr;
        return nullptr;
    }

    return image;
}

vk::ImageView VkImageWrapper::createImageView(
    vk::Device device,
    vk::Image image,
    vk::Format format,
    vk::SamplerYcbcrConversion ycbcrConversion,
    const vk::detail::DispatchLoaderDynamic &dld,
    uint64_t externalFormat)
{
    vk::SamplerYcbcrConversionInfo ycbcrInfo;

#ifdef Q_OS_ANDROID
    vk::ExternalFormatANDROID externalFormatInfo;
#endif

    vk::ImageViewCreateInfo viewInfo(
        {},
        image,
        vk::ImageViewType::e2D,
        format,
        vk::ComponentMapping(
            vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity),
        vk::ImageSubresourceRange(
            vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

    if (ycbcrConversion) {
        ycbcrInfo.conversion = ycbcrConversion;
        viewInfo.pNext = &ycbcrInfo;
    }

#ifdef Q_OS_ANDROID
    if (format == vk::Format::eUndefined && externalFormat != 0) {
        externalFormatInfo.externalFormat = externalFormat;
        if (viewInfo.pNext) {
            externalFormatInfo.pNext = const_cast<void *>(viewInfo.pNext);
        }
        viewInfo.pNext = &externalFormatInfo;
    }
#endif

    try {
        return device.createImageView(viewInfo, nullptr, dld);
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to create image view: {}", e.what());
        return nullptr;
    }
}

vk::SamplerYcbcrConversion VkImageWrapper::createYCbCrConversion(
    vk::Device device,
    vk::Format format,
    const vk::detail::DispatchLoaderDynamic &dld,
    int colorSpace,
    int colorRange,
    uint64_t externalFormat)
{
    vk::SamplerYcbcrModelConversion ycbcrModel = vk::SamplerYcbcrModelConversion::eYcbcr709;
    switch (colorSpace) {
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
        ycbcrModel = vk::SamplerYcbcrModelConversion::eYcbcr2020;
        break;
    case AVCOL_SPC_BT709:
    default:
        ycbcrModel = vk::SamplerYcbcrModelConversion::eYcbcr709;
        break;
    }

    vk::SamplerYcbcrRange ycbcrRange = vk::SamplerYcbcrRange::eItuNarrow;
    switch (colorRange) {
    case AVCOL_RANGE_JPEG:
        ycbcrRange = vk::SamplerYcbcrRange::eItuFull;
        break;
    case AVCOL_RANGE_MPEG:
    default:
        ycbcrRange = vk::SamplerYcbcrRange::eItuNarrow;
        break;
    }

    vk::SamplerYcbcrConversionCreateInfo convInfo(
        format,
        ycbcrModel,
        ycbcrRange,
        vk::ComponentMapping(
            vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity),
        vk::ChromaLocation::eMidpoint,
        vk::ChromaLocation::eMidpoint,
        vk::Filter::eLinear,
        VK_FALSE);

#ifdef Q_OS_ANDROID
    vk::ExternalFormatANDROID externalFormatInfo;
    if (format == vk::Format::eUndefined && externalFormat != 0) {
        externalFormatInfo.externalFormat = externalFormat;
        convInfo.pNext = &externalFormatInfo;
    }
#endif

    try {
        return device.createSamplerYcbcrConversionKHR(convInfo, nullptr, dld);
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to create YCbCr conversion: {}", e.what());
        return nullptr;
    }
}

} // namespace ffmpeg

QT_END_NAMESPACE

#endif
