#ifndef VKFORMATUTILS_P_H
#define VKFORMATUTILS_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegDefs_p.h>

#if QT_FFMPEG_HAS_VULKAN

#include <vulkan/vulkan.hpp>

extern "C" {
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vulkan.h>
}

QT_BEGIN_NAMESPACE

namespace ffmpeg {

// Vulkan 格式工具，提供像素格式到 Vulkan 格式的转换
class VkFormatUtils
{
public:
    static vk::Format avPixelFormatToVulkan(AVPixelFormat format);
    static vk::Format avPixelFormatToVulkanSW(AVPixelFormat format);
    static int getPlaneCount(vk::Format format);
    static vk::Format getPerPlaneFormat(vk::Format multiPlaneFormat, int plane);
    static vk::ImageAspectFlagBits getPlaneAspect(int plane);
    static bool isYCbCrFormat(vk::Format format);
};

} // namespace ffmpeg

QT_END_NAMESPACE

#endif
#endif
