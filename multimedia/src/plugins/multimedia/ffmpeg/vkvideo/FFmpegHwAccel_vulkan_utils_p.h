#ifndef FFMPEGHWACCEL_VULKAN_UTILS_P_H
#define FFMPEGHWACCEL_VULKAN_UTILS_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegDefs_p.h>

#if QT_FFMPEG_HAS_VULKAN

#include <vulkan/vulkan.h>

import qzLog;

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vulkan.h>
#include <libavutil/pixdesc.h>
}

QT_BEGIN_NAMESPACE

namespace ffmpeg::vulkan_utils
{

    extern qz::Log::LogCategory qLcMediaFFmpegHWAccelVulkan;

    VkFormat avPixelFormatToVulkanFormat(AVPixelFormat format);
    const AVVulkanDeviceContext *getVulkanDeviceContext(const AVHWDeviceContext *ctx);
    AVVkFrame *getAVVkFrame(const AVFrame *frame);
    int getVulkanPlaneCount(VkFormat format);
    VkFormat getVulkanPerPlaneFormat(VkFormat multiPlaneFormat, int plane);
    VkImageAspectFlagBits getVulkanPlaneAspect(int plane);
    bool isYCbCrFormat(VkFormat format);
    uint32_t findDeviceLocalMemoryType(VkPhysicalDevice physicalDevice, uint32_t memoryTypeBits);
    void transitionImageLayoutCmd(VkCommandBuffer cmdBuffer, VkImage image,
                                  VkImageLayout oldLayout, VkImageLayout newLayout,
                                  VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                                  VkAccessFlags srcAccess, VkAccessFlags dstAccess);
    bool submitAndResetCommandBuffer(VkDevice device, VkQueue queue,
                                     VkCommandBuffer commandBuffer, VkFence fence);
    bool createYCbCrConversion(VkInstance instance, VkDevice device, VkFormat format,
                               VkSamplerYcbcrConversion &conversion,
                               int colorSpace = AVCOL_SPC_BT709,
                               int colorRange = AVCOL_RANGE_MPEG);
    bool createVulkanImage(VkDevice device, VkPhysicalDevice physicalDevice,
                           uint32_t width, uint32_t height, VkFormat format,
                           VkImageUsageFlags usage,
                           const void *imageCreateInfoPNext,
                           const void *memoryAllocPNext,
                           VkImage &outImage, VkDeviceMemory &outMemory);
    bool createVulkanImageView(VkDevice device, VkImage image, VkFormat format,
                               VkSamplerYcbcrConversion ycbcrConversion,
                               VkImageView &outImageView);

}

QT_END_NAMESPACE

#endif
#endif
