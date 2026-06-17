#ifndef FFMPEGHWACCEL_VULKANSW_P_H
#define FFMPEGHWACCEL_VULKANSW_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegTextureConverter_p.h>

#if QT_FFMPEG_HAS_VULKAN

#include <vulkan/vulkan.hpp>
#include <QVulkanInstance>

#include <memory>
#include <vector>

extern "C" {
#include <libavutil/pixfmt.h>
}

QT_BEGIN_NAMESPACE

class QRhi;

namespace ffmpeg {

class VkDeviceContext;

class VulkanSWImagePool
{
public:
    VulkanSWImagePool(QVulkanInstance *vkInstance, VkDevice device,
                       VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex);
    ~VulkanSWImagePool();

    bool isValid() const { return m_valid; }

    struct LinearImage
    {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
        uint8_t *mappedData = nullptr;
        VkDeviceSize mappedSize = 0;
        QSize size_;
        VkFormat format = VK_FORMAT_UNDEFINED;
        int planeCount = 0;
        VkDeviceSize planeOffsets[3] = {};
        VkDeviceSize planeSizes[3] = {};
        VkDeviceSize planeRowPitches[3] = {};
        void *codecContextData = nullptr;
    };

    std::shared_ptr<LinearImage> acquireImage(const QSize &size, VkFormat format, int planeCount);
    void releaseImage(std::shared_ptr<LinearImage> image);

private:
    bool createLinearImage(const QSize &size, VkFormat format, int planeCount, LinearImage &image);
    void destroyLinearImage(LinearImage &image);

    QVulkanInstance *m_vkInstance = nullptr;
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    uint32_t m_queueFamilyIndex = 0;
    bool m_valid = false;
    bool m_hostMemorySupported = false;

    std::vector<std::weak_ptr<LinearImage>> m_imagePool;
    int m_maxPoolSize = 4;
};

// Vulkan 软件纹理转换器，将软件帧上传为最优纹理
class VulkanSWTextureConverter : public TextureConverterBackend
{
public:
    VulkanSWTextureConverter(QRhi *rhi);
    ~VulkanSWTextureConverter() override;

    VideoFrameTexturesHandlesUPtr
    createTextureHandles(AVFrame *frame, VideoFrameTexturesHandlesUPtr oldHandles) override;

    bool isValid() const { return m_valid; }

private:
    bool createOutputImage(int width, int height, VkFormat format,
                          VkImage &image, VkDeviceMemory &memory, VkImageView &imageView);
    bool copyLinearToOptimal(VkImage srcImage, VkImage dstImage,
                             int width, int height, int planeCount);
    bool submitCommandBuffer();

    QVulkanInstance *m_vkInstance = nullptr;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
    VkSemaphore m_timelineSemaphore = VK_NULL_HANDLE;
    uint64_t m_timelineValue = 0;
    uint32_t m_graphicsQueueFamilyIndex = 0;
    bool m_valid = false;
    VkSamplerYcbcrConversion m_ycbcrConversion = VK_NULL_HANDLE;

    std::shared_ptr<VkDeviceContext> m_deviceContext;

    VkImageView m_cachedImageView = VK_NULL_HANDLE;
    VkImage m_cachedImageViewForImage = VK_NULL_HANDLE;

    bool isYCbCrFormat(VkFormat format) const;
    bool createYCbCrConversion(VkFormat format, int colorSpace = AVCOL_SPC_BT709, int colorRange = AVCOL_RANGE_MPEG);

    friend class VulkanSWTextureHandles;
};

int vulkanSWGetBuffer2(AVCodecContext *codecCtx, AVFrame *frame, int flags);

void setVulkanSWImagePoolForContext(AVCodecContext *codecCtx, std::shared_ptr<VulkanSWImagePool> pool);

void initVulkanSWContext(AVCodecContext *codecCtx, QVulkanInstance *vkInstance,
                         VkDevice device, VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex);

std::shared_ptr<VulkanSWImagePool> getVulkanSWImagePoolForContext(AVCodecContext *codecCtx);

bool isVulkanSWZeroCopyFrame(const AVFrame *frame);

}

QT_END_NAMESPACE

#endif

#endif
