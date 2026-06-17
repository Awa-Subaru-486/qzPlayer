#include "../FFmpegHwAccel_vulkan_utils_p.h"

#if QT_FFMPEG_HAS_VULKAN

#include "VkFormatUtils_p.h"

QT_BEGIN_NAMESPACE

namespace ffmpeg {

vk::Format VkFormatUtils::avPixelFormatToVulkan(AVPixelFormat format)
{
    switch (format) {
    case AV_PIX_FMT_NV12:
        return vk::Format::eG8B8R82Plane420Unorm;
    case AV_PIX_FMT_P010:
    case AV_PIX_FMT_P016:
        return vk::Format::eG10X6B10X6R10X62Plane420Unorm3Pack16;
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
        return vk::Format::eG8B8R83Plane420Unorm;
    case AV_PIX_FMT_YUV420P10:
        return vk::Format::eG10X6B10X6R10X63Plane420Unorm3Pack16;
    case AV_PIX_FMT_YUV420P12:
        return vk::Format::eG12X4B12X4R12X43Plane420Unorm3Pack16;
    case AV_PIX_FMT_YUV420P16:
        return vk::Format::eG16B16R163Plane420Unorm;
    case AV_PIX_FMT_YUV422P:
        return vk::Format::eG8B8R83Plane422Unorm;
    case AV_PIX_FMT_YUV444P:
        return vk::Format::eG8B8R83Plane444Unorm;
    case AV_PIX_FMT_BGRA:
        return vk::Format::eB8G8R8A8Unorm;
    case AV_PIX_FMT_RGBA:
        return vk::Format::eR8G8B8A8Unorm;
    case AV_PIX_FMT_BGR0:
        return vk::Format::eB8G8R8A8Unorm;
    case AV_PIX_FMT_RGB0:
        return vk::Format::eR8G8B8A8Unorm;
    default:
        return vk::Format::eUndefined;
    }
}

vk::Format VkFormatUtils::avPixelFormatToVulkanSW(AVPixelFormat format)
{
    switch (format) {
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
        return vk::Format::eG8B8R83Plane420Unorm;
    case AV_PIX_FMT_YUV420P10:
        return vk::Format::eG10X6B10X6R10X63Plane420Unorm3Pack16;
    case AV_PIX_FMT_YUV420P12:
        return vk::Format::eG12X4B12X4R12X43Plane420Unorm3Pack16;
    case AV_PIX_FMT_YUV420P16:
        return vk::Format::eG16B16R163Plane420Unorm;
    case AV_PIX_FMT_NV12:
        return vk::Format::eG8B8R82Plane420Unorm;
    case AV_PIX_FMT_P010:
    case AV_PIX_FMT_P016:
        return vk::Format::eG10X6B10X6R10X62Plane420Unorm3Pack16;
    case AV_PIX_FMT_BGRA:
        return vk::Format::eB8G8R8A8Unorm;
    case AV_PIX_FMT_RGBA:
        return vk::Format::eR8G8B8A8Unorm;
    default:
        return vk::Format::eUndefined;
    }
}

int VkFormatUtils::getPlaneCount(vk::Format format)
{
    switch (format) {
    case vk::Format::eG8B8R82Plane420Unorm:
    case vk::Format::eG10X6B10X6R10X62Plane420Unorm3Pack16:
        return 2;
    case vk::Format::eG8B8R83Plane420Unorm:
    case vk::Format::eG10X6B10X6R10X63Plane420Unorm3Pack16:
    case vk::Format::eG8B8R83Plane422Unorm:
    case vk::Format::eG8B8R83Plane444Unorm:
        return 3;
    case vk::Format::eB8G8R8A8Unorm:
    case vk::Format::eR8G8B8A8Unorm:
        return 1;
    default:
        return 0;
    }
}

vk::Format VkFormatUtils::getPerPlaneFormat(vk::Format multiPlaneFormat, int plane)
{
    switch (multiPlaneFormat) {
    case vk::Format::eG8B8R82Plane420Unorm:
        return (plane == 0) ? vk::Format::eR8Unorm : vk::Format::eR8G8Unorm;
    case vk::Format::eG10X6B10X6R10X62Plane420Unorm3Pack16:
        return (plane == 0) ? vk::Format::eR16Unorm : vk::Format::eR16G16Unorm;
    case vk::Format::eG8B8R83Plane420Unorm:
    case vk::Format::eG8B8R83Plane422Unorm:
    case vk::Format::eG8B8R83Plane444Unorm:
        return vk::Format::eR8Unorm;
    case vk::Format::eG10X6B10X6R10X63Plane420Unorm3Pack16:
        return vk::Format::eR16Unorm;
    case vk::Format::eB8G8R8A8Unorm:
    case vk::Format::eR8G8B8A8Unorm:
        return multiPlaneFormat;
    default:
        return vk::Format::eUndefined;
    }
}

vk::ImageAspectFlagBits VkFormatUtils::getPlaneAspect(int plane)
{
    switch (plane) {
    case 0: return vk::ImageAspectFlagBits::ePlane0;
    case 1: return vk::ImageAspectFlagBits::ePlane1;
    case 2: return vk::ImageAspectFlagBits::ePlane2;
    default: return vk::ImageAspectFlagBits::eColor;
    }
}

bool VkFormatUtils::isYCbCrFormat(vk::Format format)
{
    return format == vk::Format::eG8B8R82Plane420Unorm
        || format == vk::Format::eG8B8R83Plane420Unorm
        || format == vk::Format::eG10X6B10X6R10X62Plane420Unorm3Pack16
        || format == vk::Format::eG10X6B10X6R10X63Plane420Unorm3Pack16
        || format == vk::Format::eG12X4B12X4R12X42Plane420Unorm3Pack16
        || format == vk::Format::eG12X4B12X4R12X43Plane420Unorm3Pack16
        || format == vk::Format::eG16B16R162Plane420Unorm
        || format == vk::Format::eG16B16R163Plane420Unorm;
}

} // namespace ffmpeg

QT_END_NAMESPACE

#endif
