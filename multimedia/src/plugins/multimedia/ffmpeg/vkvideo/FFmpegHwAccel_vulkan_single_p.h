#ifndef FFMPEGHWACCEL_VULKAN_SINGLE_P_H
#define FFMPEGHWACCEL_VULKAN_SINGLE_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegDefs_p.h>

#if QT_FFMPEG_HAS_VULKAN

#include <vulkan/vulkan.h>
#include <qzMultimedia/private/HwVideoBuffer_p.h>
#include <memory>
#include <vector>

QT_BEGIN_NAMESPACE

class QRhi;

namespace ffmpeg {

class VulkanOutputImagePool;
class VulkanTextureHandles;
class TextureConverterBackend;
using TextureConverterBackendPtr = std::shared_ptr<TextureConverterBackend>;

// Vulkan 单设备转换器，在单个设备上执行帧到纹理转换
class VulkanSingleDeviceConverter
{
public:
    VulkanSingleDeviceConverter(VkInstance vkInstance, VkDevice device, VkPhysicalDevice physicalDevice,
                                 VkQueue graphicsQueue, uint32_t graphicsQueueFamilyIndex);
    ~VulkanSingleDeviceConverter();

    bool isValid() const { return m_valid; }
    void setImagePool(VulkanOutputImagePool *pool);

    VideoFrameTexturesHandlesUPtr createTextureHandles(
        AVFrame *frame,
        VideoFrameTexturesHandlesUPtr oldHandles,
        TextureConverterBackendPtr converterBackend,
        QRhi *rhi);

private:
    VkCommandBuffer acquireCommandBuffer();
    void returnCommandBuffer(VkCommandBuffer cmdBuf);

    VkInstance m_vkInstance = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamilyIndex = 0;
    VulkanOutputImagePool *m_imagePool = nullptr;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBufferPool;
    std::vector<VkCommandBuffer> m_activeCommandBuffers;

    friend class VulkanTextureHandles;
    friend class VulkanDirectTextureHandles;
    static constexpr int kCommandBufferCount = 4;

    VkSemaphore m_timelineSemaphore = VK_NULL_HANDLE;
    uint64_t m_timelineValue = 0;

    VkSamplerYcbcrConversion m_ycbcrConversion = VK_NULL_HANDLE;

    bool m_valid = false;
};

}

QT_END_NAMESPACE

#endif
#endif
