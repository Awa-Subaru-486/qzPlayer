#ifndef FFMPEGHWACCEL_VULKAN_P_H
#define FFMPEGHWACCEL_VULKAN_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegHwAccel_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegDefs_p.h>

#if QT_FFMPEG_HAS_VULKAN

#include "FFmpegHwAccel_vulkan_cross_p.h"
#include "FFmpegHwAccel_vulkan_single_p.h"

extern "C" {
#include <libavutil/hwcontext_vulkan.h>
}

#include <rhi/qrhi.h>
#include <memory>
#include <array>
#include <vector>

#include <vulkan/vulkan.hpp>

QT_BEGIN_NAMESPACE

namespace ffmpeg {

// 输出图像，封装 Vulkan 图像、内存及状态
struct OutputImage
{
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    int width = 0;
    int height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    int planeIndex = 0;
    bool inUse = false;
};

class VulkanOutputImagePool
{
public:
    VulkanOutputImagePool(VkDevice device, VkPhysicalDevice physicalDevice);
    ~VulkanOutputImagePool();

    OutputImage *acquireImage(int width, int height, VkFormat format, int planeIndex);
    void releaseImage(OutputImage *image);

private:
    std::vector<std::unique_ptr<OutputImage>> takeImagesToClear(int width, int height, VkFormat format, int planeIndex);
    void destroyImage(OutputImage *image, vk::Device vkDevice, const vk::detail::DispatchLoaderDynamic &dld);

    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    std::vector<std::unique_ptr<OutputImage>> m_pool;
    static constexpr size_t kMaxPoolSize = 16;
};

class VulkanTextureConverter : public TextureConverterBackend
{
public:
    explicit VulkanTextureConverter(QRhi *rhi);
    ~VulkanTextureConverter() override;

    VideoFrameTexturesHandlesUPtr
    createTextureHandles(AVFrame *frame, VideoFrameTexturesHandlesUPtr oldHandles) override;

    static void SetupDecoderTextures(AVCodecContext *s);

    bool isValid() const;

    VulkanOutputImagePool *imagePool() const;

private:
    bool initializeFromRhi(QRhi *rhi);

    struct Private;
    std::unique_ptr<Private> d;
    std::unique_ptr<VulkanSingleDeviceConverter> m_singleDeviceConverter;
    VulkanCrossDeviceBridge m_crossDeviceBridge;

    friend class VulkanTextureHandles;
    friend class VulkanDirectTextureHandles;
};

// Vulkan 纹理句柄，管理图像平面句柄和同步
class VulkanTextureHandles : public VideoFrameTexturesHandles
{
public:
    VulkanTextureHandles(TextureConverterBackendPtr &&converterBackend, QRhi *rhi,
                         int planeCount, int width, int height, uint32_t format,
                         VulkanOutputImagePool *pool);
    ~VulkanTextureHandles() override;

    quint64 textureHandle(QRhi &rhi, int plane) override;

    void setPlaneHandle(int plane, uint64_t image, uint64_t memory);
    void setPlaneOutputImage(int plane, OutputImage *outputImage);

    int width() const { return m_width; }
    int height() const { return m_height; }
    uint32_t format() const { return m_format; }
    int planeCount() const { return m_planeCount; }
    uint64_t planeImage(int plane) const;
    uint64_t planeMemory(int plane) const;
    OutputImage *planeOutputImage(int plane) const;

    bool isCompatibleWith(int width, int height, uint32_t format) const;

    void resetForReuse();

    void setPendingTimeline(VkSemaphore semaphore, uint64_t value);
    VkSemaphore pendingTimelineSemaphore() const;
    uint64_t pendingTimelineValue() const;
    bool hasPendingTimeline() const;
    bool isTimelineSignaled() const;
    void waitTimeline();

    void setCommandBuffer(VkCommandBuffer cmdBuf);
    VkCommandBuffer commandBuffer() const;

    void releaseResources();

    // When false, the destructor will not destroy plane images/memory.
    // Used by cross-device bridge path where the bridge owns the resources.
    void setOwnsResources(bool owns) { m_ownsResources = owns; }

private:
    TextureConverterBackendPtr m_parentConverterBackend;
    QRhi *m_owner = nullptr;
    VulkanOutputImagePool *m_pool = nullptr;
    int m_planeCount = 0;
    int m_width = 0;
    int m_height = 0;
    uint32_t m_format = 0;
    bool m_resourcesReleased = false;
    bool m_ownsResources = true;
    VkSemaphore m_timelineSemaphore = VK_NULL_HANDLE;
    uint64_t m_pendingTimelineValue = 0;
    VkDevice m_timelineDevice = VK_NULL_HANDLE;
    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
    std::array<uint64_t, 3> m_planeImages{};
    std::array<uint64_t, 3> m_planeMemory{};
    std::array<OutputImage *, 3> m_planeOutputImages{};
};

// Vulkan 直接纹理句柄，直接使用 AVVkFrame 纹理
// 使用 GPU 端同步（timeline semaphore + 布局转换 command buffer）替代
// CPU 端 waitSemaphores，确保解码队列与图形队列之间的内存可见性
class VulkanDirectTextureHandles : public VideoFrameTexturesHandles
{
public:
    VulkanDirectTextureHandles(AVVkFrame *vkFrame,
                               TextureConverterBackendPtr &&converterBackend,
                               QRhi *rhi, int planeCount, int width, int height, uint32_t format,
                               const AVHWFramesContext *framesCtx,
                               bool gpuSyncDone = false);
    ~VulkanDirectTextureHandles() override;

    quint64 textureHandle(QRhi &rhi, int plane) override;

    void updateAVVkFrameLayout();

    int width() const { return m_width; }
    int height() const { return m_height; }
    uint32_t format() const { return m_format; }
    int planeCount() const { return m_planeCount; }

    // Timeline semaphore support for GPU-side synchronization
    void setPendingTimeline(VkSemaphore semaphore, uint64_t value);
    bool hasPendingTimeline() const;
    bool isTimelineSignaled() const;
    void waitTimeline();

    // Command buffer support (for returning to the pool)
    void setCommandBuffer(VkCommandBuffer cmdBuf);
    VkCommandBuffer commandBuffer() const;

private:
    AVVkFrame *m_vkFrame = nullptr;
    TextureConverterBackendPtr m_parentConverterBackend;
    QRhi *m_owner = nullptr;
    const AVHWFramesContext *m_framesCtx = nullptr;
    int m_planeCount = 0;
    int m_width = 0;
    int m_height = 0;
    uint32_t m_format = 0;
    bool m_locked = false;
    bool m_gpuSyncDone = false;
    VkImageLayout m_initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkAccessFlags m_initialAccess = 0;

    // Decode synchronization: stored for GPU-side synchronization
    VkSemaphore m_decodeSemaphore = VK_NULL_HANDLE;
    uint64_t m_decodeSemaphoreValue = 0;
    bool m_decodeWaited = false; // whether CPU-side wait has been performed

    // GPU-side synchronization: timeline semaphore signaled when layout
    // transition command buffer completes
    VkSemaphore m_timelineSemaphore = VK_NULL_HANDLE;
    uint64_t m_pendingTimelineValue = 0;
    VkDevice m_timelineDevice = VK_NULL_HANDLE;
    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
};

AVBufferUPtr createVulkanDeviceContextFromRhi(QRhi *rhi);

AVFrameUPtr copyFromHwPoolVulkan(AVFrameUPtr src);

}

QT_END_NAMESPACE

#endif

#endif
