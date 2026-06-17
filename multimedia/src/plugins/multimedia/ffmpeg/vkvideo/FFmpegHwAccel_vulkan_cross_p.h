#ifndef FFMPEGHWACCEL_VULKAN_CROSS_P_H
#define FFMPEGHWACCEL_VULKAN_CROSS_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegDefs_p.h>

#if QT_FFMPEG_HAS_VULKAN

#include <vulkan/vulkan.h>
#include <array>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifdef Q_OS_ANDROID
#include <android/hardware_buffer.h>
#endif

QT_BEGIN_NAMESPACE

namespace ffmpeg {

static constexpr int kMaxPlanes = 3;

// Per-plane resources for cross-device bridge
struct CrossDevicePlaneResources
{
    // Source side (FFmpeg's device): shared image + memory for this plane
    VkImage srcSharedImage = VK_NULL_HANDLE;
    VkDeviceMemory srcSharedMemory = VK_NULL_HANDLE;

    // Destination side (RHI's device): imported image + memory for this plane
    VkImage dstImage = VK_NULL_HANDLE;
    VkDeviceMemory dstImportedMemory = VK_NULL_HANDLE;

    // Exported memory handle
#ifdef Q_OS_WIN
    HANDLE memoryHandle = nullptr;
#elif defined(Q_OS_ANDROID)
    AHardwareBuffer *ahwb = nullptr;
#else
    int memoryFd = -1;
#endif

    int planeWidth = 0;
    int planeHeight = 0;
    VkFormat planeFormat = VK_FORMAT_UNDEFINED;
};

// Vulkan 跨设备桥接，在不同设备间导入/拷贝帧数据
// 支持多平面格式：每个 plane 有独立的共享图像和内存导出/导入
class VulkanCrossDeviceBridge
{
public:
    VulkanCrossDeviceBridge();
    ~VulkanCrossDeviceBridge();

    // Zero-copy import: try to export AVVkFrame's memory directly.
    // This only works if FFmpeg allocated the memory with export handle types.
    // For multi-plane formats, this is typically NOT possible.
    bool importFrameZeroCopy(VkDevice srcDevice, VkPhysicalDevice srcPhysicalDevice,
                              VkImage srcImage, VkDeviceMemory srcMemory, VkDeviceSize srcMemorySize,
                              int width, int height, VkFormat format,
                              VkQueue srcQueue, uint32_t srcQueueFamilyIndex,
                              VkDevice dstDevice, VkPhysicalDevice dstPhysicalDevice);

    // Copy path: copy from source multi-plane image to per-plane shared images,
    // export each plane's memory, import on destination device.
    // srcLayout: the current layout of the source image (from AVVkFrame->layout[0])
    bool copyFrameCrossDevice(VkDevice srcDevice, VkPhysicalDevice srcPhysicalDevice,
                               VkImage srcImage, int width, int height, VkFormat format, int planeCount,
                               VkImageLayout srcLayout,
                               VkQueue srcQueue, uint32_t srcQueueFamilyIndex,
                               VkDevice dstDevice, VkPhysicalDevice dstPhysicalDevice);

    // Get the imported destination image for a specific plane
    VkImage importedImage(int plane = 0) const;

    // Get the number of planes
    int planeCount() const { return m_planeCount; }

    bool isZeroCopy() const { return m_zeroCopy; }

    void reset(bool destroySrcResources = false);

private:
    bool exportMemoryHandle(VkDevice device, VkDeviceMemory memory, int plane);
    VkImage createDestinationImageFromImported(VkDevice device, VkPhysicalDevice physicalDevice,
                                                int width, int height, VkFormat format, int plane);
    bool copyToSharedAndExport(VkDevice srcDevice, VkPhysicalDevice srcPhysicalDevice,
                                VkImage srcImage, int width, int height, VkFormat format, int planeCount,
                                VkImageLayout srcLayout,
                                VkQueue srcQueue, uint32_t srcQueueFamilyIndex);

    std::array<CrossDevicePlaneResources, kMaxPlanes> m_planes;

    VkDevice m_srcDevice = VK_NULL_HANDLE;
    VkDevice m_dstDevice = VK_NULL_HANDLE;

    int m_width = 0;
    int m_height = 0;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    int m_planeCount = 0;
    bool m_initialized = false;
    bool m_zeroCopy = false;

    VkCommandPool m_srcCommandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_srcCommandBuffer = VK_NULL_HANDLE;
    VkFence m_srcFence = VK_NULL_HANDLE;
    uint32_t m_srcQueueFamilyIndex = 0;
};

}

QT_END_NAMESPACE

#endif
#endif
