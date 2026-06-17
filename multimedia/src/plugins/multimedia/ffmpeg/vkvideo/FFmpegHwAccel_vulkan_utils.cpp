#include "FFmpegHwAccel_vulkan_utils_p.h"

#if QT_FFMPEG_HAS_VULKAN

#include "private/VkFormatUtils_p.h"
#include "private/VkMemory_p.h"
#include "private/VkImage_p.h"
#include "private/VkDispatch_p.h"

import qzLog;

#include <rhi/qrhi.h>

QT_BEGIN_NAMESPACE


namespace ffmpeg::vulkan_utils {

qz::Log::LogCategory qLcMediaFFmpegHWAccelVulkan("qz.multimedia.hwaccel.vulkan");

VkFormat avPixelFormatToVulkanFormat(AVPixelFormat format)
{
    return static_cast<VkFormat>(VkFormatUtils::avPixelFormatToVulkan(format));
}

const AVVulkanDeviceContext *getVulkanDeviceContext(const AVHWDeviceContext *ctx)
{
    if (!ctx || ctx->type != AV_HWDEVICE_TYPE_VULKAN)
        return nullptr;
    return static_cast<const AVVulkanDeviceContext *>(ctx->hwctx);
}

AVVkFrame *getAVVkFrame(const AVFrame *frame)
{
    if (!frame || frame->format != AV_PIX_FMT_VULKAN)
        return nullptr;
    return reinterpret_cast<AVVkFrame *>(frame->data[0]);
}

int getVulkanPlaneCount(VkFormat format)
{
    return VkFormatUtils::getPlaneCount(static_cast<vk::Format>(format));
}

VkFormat getVulkanPerPlaneFormat(VkFormat multiPlaneFormat, int plane)
{
    return static_cast<VkFormat>(VkFormatUtils::getPerPlaneFormat(
        static_cast<vk::Format>(multiPlaneFormat), plane));
}

VkImageAspectFlagBits getVulkanPlaneAspect(int plane)
{
    return static_cast<VkImageAspectFlagBits>(VkFormatUtils::getPlaneAspect(plane));
}

bool isYCbCrFormat(VkFormat format)
{
    return VkFormatUtils::isYCbCrFormat(static_cast<vk::Format>(format));
}

uint32_t findDeviceLocalMemoryType(VkPhysicalDevice physicalDevice, uint32_t memoryTypeBits)
{
    const auto &dld = VkDispatch::dld();
    return VkMemory::findDeviceLocalMemoryType(
        vk::PhysicalDevice(physicalDevice), memoryTypeBits, dld);
}

void transitionImageLayoutCmd(VkCommandBuffer cmdBuffer, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout,
                               VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                               VkAccessFlags srcAccess, VkAccessFlags dstAccess)
{
    const auto &dld = VkDispatch::dld();

    vk::ImageMemoryBarrier barrier(
        static_cast<vk::AccessFlags>(srcAccess),
        static_cast<vk::AccessFlags>(dstAccess),
        static_cast<vk::ImageLayout>(oldLayout),
        static_cast<vk::ImageLayout>(newLayout),
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        vk::Image(image),
        vk::ImageSubresourceRange(
            vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

    vk::CommandBuffer(cmdBuffer).pipelineBarrier(
        static_cast<vk::PipelineStageFlags>(srcStage),
        static_cast<vk::PipelineStageFlags>(dstStage),
        {}, {}, {}, barrier, dld);
}

bool submitAndResetCommandBuffer(VkDevice device, VkQueue queue,
                                  VkCommandBuffer commandBuffer, VkFence fence)
{
    const auto &dld = VkDispatch::dld();
    auto vkDevice = vk::Device(device);

    vk::SubmitInfo submitInfo;
    auto vkCmdBuf = vk::CommandBuffer(commandBuffer);
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &vkCmdBuf;

    try {
        vk::Queue(queue).submit(submitInfo, vk::Fence(fence), dld);
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(qLcMediaFFmpegHWAccelVulkan, "Failed to submit command buffer: {}", e.what());
        return false;
    }

    try {
        (void)vkDevice.waitForFences(vk::Fence(fence), VK_TRUE, UINT64_MAX, dld);
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(qLcMediaFFmpegHWAccelVulkan, "Failed to wait for fence: {}", e.what());
        return false;
    }

    vkDevice.resetFences(vk::Fence(fence), dld);
    vkCmdBuf.reset(vk::CommandBufferResetFlagBits::eReleaseResources, dld);
    return true;
}

bool createYCbCrConversion(VkInstance , VkDevice device, VkFormat format,
                            VkSamplerYcbcrConversion &conversion,
                            int colorSpace,
                            int colorRange)
{
    if (conversion != VK_NULL_HANDLE)
        return true;

    const auto &dld = VkDispatch::dld();
    auto result = VkImageWrapper::createYCbCrConversion(
        vk::Device(device), static_cast<vk::Format>(format), dld, colorSpace, colorRange);
    if (!result) {
        qz::Log::cat_warn(qLcMediaFFmpegHWAccelVulkan, "createSamplerYcbcrConversionKHR not available or failed");
        return false;
    }
    conversion = static_cast<VkSamplerYcbcrConversion>(result);
    return true;
}

bool createVulkanImage(VkDevice device, VkPhysicalDevice physicalDevice,
                        uint32_t width, uint32_t height, VkFormat format,
                        VkImageUsageFlags usage,
                        const void *imageCreateInfoPNext,
                        const void *memoryAllocPNext,
                        VkImage &outImage, VkDeviceMemory &outMemory)
{
    const auto &dld = VkDispatch::dld();
    vk::DeviceMemory vkMemory = nullptr;
    auto result = VkImageWrapper::createOptimal(
        vk::Device(device), vk::PhysicalDevice(physicalDevice),
        width, height, static_cast<vk::Format>(format),
        vk::ImageUsageFlags(usage),
        imageCreateInfoPNext, memoryAllocPNext,
        vkMemory, dld);

    if (!result) {
        outImage = VK_NULL_HANDLE;
        outMemory = VK_NULL_HANDLE;
        return false;
    }

    outImage = static_cast<VkImage>(result);
    outMemory = static_cast<VkDeviceMemory>(vkMemory);
    return true;
}

bool createVulkanImageView(VkDevice device, VkImage image, VkFormat format,
                            VkSamplerYcbcrConversion ycbcrConversion,
                            VkImageView &outImageView)
{
    const auto &dld = VkDispatch::dld();
    auto result = VkImageWrapper::createImageView(
        vk::Device(device), vk::Image(image), static_cast<vk::Format>(format),
        ycbcrConversion != VK_NULL_HANDLE
            ? vk::SamplerYcbcrConversion(ycbcrConversion)
            : vk::SamplerYcbcrConversion(),
        dld);

    if (!result) {
        outImageView = VK_NULL_HANDLE;
        return false;
    }

    outImageView = static_cast<VkImageView>(result);
    return true;
}

}


QT_END_NAMESPACE

#endif
