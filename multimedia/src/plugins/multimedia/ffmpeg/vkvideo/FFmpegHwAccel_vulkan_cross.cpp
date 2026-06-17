#include "FFmpegHwAccel_vulkan_cross_p.h"

#if QT_FFMPEG_HAS_VULKAN

#include "FFmpegHwAccel_vulkan_utils_p.h"
#include "private/VkExternalMemory_p.h"
#include "private/VkImage_p.h"
#include "private/VkMemory_p.h"
#include "private/VkFormatUtils_p.h"
#include "private/VkDispatch_p.h"

import qzLog;
#include <array>

#ifdef Q_OS_WIN
#include <vulkan/vulkan_win32.h>
#endif

#ifdef Q_OS_ANDROID
#include <vulkan/vulkan_android.h>
#endif

QT_BEGIN_NAMESPACE

namespace ffmpeg {

VulkanCrossDeviceBridge::VulkanCrossDeviceBridge()
{
}

VulkanCrossDeviceBridge::~VulkanCrossDeviceBridge()
{
    reset();
}

void VulkanCrossDeviceBridge::reset(bool destroySrcResources)
{
    const auto &dld = VkDispatch::dld();

    for (int i = 0; i < kMaxPlanes; ++i) {
        auto &plane = m_planes[i];

        // Destroy destination side resources (m_dstDevice is RHI's VkDevice,
        // which is always valid during VulkanTextureConverter's lifetime).
        if (plane.dstImage != VK_NULL_HANDLE && m_dstDevice != VK_NULL_HANDLE) {
            vk::Device(m_dstDevice).destroyImage(vk::Image(plane.dstImage), nullptr, dld);
            plane.dstImage = VK_NULL_HANDLE;
        }
        if (plane.dstImportedMemory != VK_NULL_HANDLE && m_dstDevice != VK_NULL_HANDLE) {
            vk::Device(m_dstDevice).freeMemory(vk::DeviceMemory(plane.dstImportedMemory), nullptr, dld);
            plane.dstImportedMemory = VK_NULL_HANDLE;
        }

        // Source side resources: m_srcDevice is FFmpeg's VkDevice which may
        // have already been destroyed (e.g. after a decoder switch). Only
        // attempt to destroy them when the caller confirms the device is still
        // valid (destroySrcResources=true). Otherwise just reset the handles —
        // the old device's child objects are implicitly freed when the VkDevice
        // is destroyed.
        if (destroySrcResources && m_srcDevice != VK_NULL_HANDLE) {
            if (plane.srcSharedImage != VK_NULL_HANDLE) {
                vk::Device(m_srcDevice).destroyImage(vk::Image(plane.srcSharedImage), nullptr, dld);
            }
            if (plane.srcSharedMemory != VK_NULL_HANDLE && !m_zeroCopy) {
                vk::Device(m_srcDevice).freeMemory(vk::DeviceMemory(plane.srcSharedMemory), nullptr, dld);
            }
        }
        plane.srcSharedImage = VK_NULL_HANDLE;
        plane.srcSharedMemory = VK_NULL_HANDLE;

        // Close exported memory handles
#ifdef Q_OS_WIN
        if (plane.memoryHandle) {
            CloseHandle(plane.memoryHandle);
            plane.memoryHandle = nullptr;
        }
#elif defined(Q_OS_ANDROID)
        if (plane.ahwb) {
            AHardwareBuffer_release(plane.ahwb);
            plane.ahwb = nullptr;
        }
#else
        if (plane.memoryFd >= 0) {
            close(plane.memoryFd);
            plane.memoryFd = -1;
        }
#endif

        plane.planeWidth = 0;
        plane.planeHeight = 0;
        plane.planeFormat = VK_FORMAT_UNDEFINED;
    }

    // Same as above: only destroy command pool/fence if the source device is
    // known to be still valid.
    if (destroySrcResources && m_srcDevice != VK_NULL_HANDLE) {
        if (m_srcCommandPool != VK_NULL_HANDLE) {
            vk::Device(m_srcDevice).destroyCommandPool(vk::CommandPool(m_srcCommandPool), nullptr, dld);
        }
        if (m_srcFence != VK_NULL_HANDLE) {
            vk::Device(m_srcDevice).destroyFence(vk::Fence(m_srcFence), nullptr, dld);
        }
    }
    m_srcCommandPool = VK_NULL_HANDLE;
    m_srcCommandBuffer = VK_NULL_HANDLE;
    m_srcFence = VK_NULL_HANDLE;
    m_srcQueueFamilyIndex = 0;

    m_srcDevice = VK_NULL_HANDLE;
    m_dstDevice = VK_NULL_HANDLE;
    m_width = 0;
    m_height = 0;
    m_format = VK_FORMAT_UNDEFINED;
    m_planeCount = 0;
    m_initialized = false;
    m_zeroCopy = false;
}

bool VulkanCrossDeviceBridge::exportMemoryHandle(VkDevice device, VkDeviceMemory memory, int plane)
{
    const auto &dld = VkDispatch::dld();
    auto &p = m_planes[plane];

#ifdef Q_OS_WIN
    if (p.memoryHandle)
        return true;
#elif defined(Q_OS_ANDROID)
    if (p.ahwb)
        return true;
#else
    if (p.memoryFd >= 0)
        return true;
#endif

    auto handle = VkExternalMemory::exportMemory(vk::Device(device), vk::DeviceMemory(memory), dld);

#ifdef Q_OS_WIN
    if (!handle.win32Handle) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Bridge: plane {} vkGetMemoryWin32HandleKHR failed", plane);
        return false;
    }
    p.memoryHandle = handle.win32Handle;
#elif defined(Q_OS_ANDROID)
    if (!handle.ahwb) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Bridge: plane {} vkGetMemoryAndroidHardwareBufferKHR failed", plane);
        return false;
    }
    p.ahwb = handle.ahwb;
#else
    if (handle.fd < 0) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Bridge: plane {} vkGetMemoryFdKHR failed", plane);
        return false;
    }
    p.memoryFd = handle.fd;
#endif

    return true;
}

VkImage VulkanCrossDeviceBridge::createDestinationImageFromImported(VkDevice device, VkPhysicalDevice physicalDevice,
                                                                      int width, int height, VkFormat format, int plane)
{
    auto &p = m_planes[plane];

    if (p.dstImage != VK_NULL_HANDLE && m_dstDevice == device
        && p.planeWidth == width && p.planeHeight == height && p.planeFormat == format)
        return p.dstImage;

    const auto &dld = VkDispatch::dld();
    auto vkDevice = vk::Device(device);
    auto physDev = vk::PhysicalDevice(physicalDevice);

    // Clean up old destination resources for this plane
    if (p.dstImage != VK_NULL_HANDLE) {
        vkDevice.destroyImage(vk::Image(p.dstImage), nullptr, dld);
        p.dstImage = VK_NULL_HANDLE;
    }
    if (p.dstImportedMemory != VK_NULL_HANDLE) {
        vkDevice.freeMemory(vk::DeviceMemory(p.dstImportedMemory), nullptr, dld);
        p.dstImportedMemory = VK_NULL_HANDLE;
    }

    // Destination image must declare compatible external handle types at creation time
    vk::ExternalMemoryImageCreateInfo externalImgInfo;

#ifdef Q_OS_WIN
    externalImgInfo.handleTypes = vk::ExternalMemoryHandleTypeFlagBits::eOpaqueWin32;
#elif defined(Q_OS_ANDROID)
    externalImgInfo.handleTypes =
        vk::ExternalMemoryHandleTypeFlagBits::eAndroidHardwareBufferANDROID;
#else
    externalImgInfo.handleTypes = vk::ExternalMemoryHandleTypeFlagBits::eOpaqueFd;
#endif

    vk::ImageCreateInfo imageInfo(
        {},
        vk::ImageType::e2D,
        static_cast<vk::Format>(format),
        vk::Extent3D(static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1),
        1, 1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eSampled,
        vk::SharingMode::eExclusive);
    imageInfo.pNext = &externalImgInfo;

    vk::Image dstImage;
    try {
        dstImage = vkDevice.createImage(imageInfo, nullptr, dld);
    } catch (vk::SystemError &) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Bridge: failed to create destination image for plane {}", plane);
        return VK_NULL_HANDLE;
    }

    auto memReqs = vkDevice.getImageMemoryRequirements(dstImage, dld);

    uint32_t memoryTypeIndex = VkMemory::findDeviceLocalMemoryType(physDev, memReqs.memoryTypeBits, dld);
    if (memoryTypeIndex == UINT32_MAX) {
        vkDevice.destroyImage(dstImage, nullptr, dld);
        return VK_NULL_HANDLE;
    }

    // Import memory with dedicated allocation info.
    // On Windows, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT requires
    // dedicated allocation (VkMemoryDedicatedAllocateInfo) for imported memory too,
    // matching the export side which also uses dedicated allocation.
    vk::MemoryDedicatedAllocateInfo dedicatedAllocInfo;
    dedicatedAllocInfo.image = dstImage;

#ifdef Q_OS_WIN
    vk::ImportMemoryWin32HandleInfoKHR importInfo(
        vk::ExternalMemoryHandleTypeFlagBits::eOpaqueWin32,
        p.memoryHandle);
    dedicatedAllocInfo.pNext = &importInfo;
#elif defined(Q_OS_ANDROID)
    vk::ImportAndroidHardwareBufferInfoANDROID importInfo(p.ahwb);
    dedicatedAllocInfo.pNext = &importInfo;
#else
    vk::ImportMemoryFdInfoKHR importInfo(
        vk::ExternalMemoryHandleTypeFlagBits::eOpaqueFd,
        p.memoryFd);
    dedicatedAllocInfo.pNext = &importInfo;
#endif

    vk::MemoryAllocateInfo allocInfo(memReqs.size, memoryTypeIndex);
    allocInfo.pNext = &dedicatedAllocInfo;

    vk::DeviceMemory importedMemory;
    try {
        importedMemory = vkDevice.allocateMemory(allocInfo, nullptr, dld);
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan,
            "Bridge: failed to import memory for plane {}: {}", plane, e.what());
        vkDevice.destroyImage(dstImage, nullptr, dld);
        return VK_NULL_HANDLE;
    }

    vkDevice.bindImageMemory(dstImage, importedMemory, 0, dld);

    p.dstImage = static_cast<VkImage>(dstImage);
    p.dstImportedMemory = static_cast<VkDeviceMemory>(importedMemory);
    p.planeWidth = width;
    p.planeHeight = height;
    p.planeFormat = format;

    return p.dstImage;
}

bool VulkanCrossDeviceBridge::importFrameZeroCopy(VkDevice srcDevice, VkPhysicalDevice srcPhysicalDevice,
                                                    VkImage srcImage, VkDeviceMemory srcMemory, VkDeviceSize srcMemorySize,
                                                    int width, int height, VkFormat format,
                                                    VkQueue srcQueue, uint32_t srcQueueFamilyIndex,
                                                    VkDevice dstDevice, VkPhysicalDevice dstPhysicalDevice)
{
    Q_UNUSED(srcPhysicalDevice);
    Q_UNUSED(srcMemorySize);
    Q_UNUSED(srcQueue);
    Q_UNUSED(srcQueueFamilyIndex);

    // If already initialized as zero-copy with same parameters, reuse
    if (m_initialized && m_zeroCopy && m_width == width && m_height == height
        && m_format == format && m_srcDevice == srcDevice && m_dstDevice == dstDevice) {
        if (m_planes[0].dstImage != VK_NULL_HANDLE)
            return true;
    }

    // Zero-copy requires exporting the source memory.
    // FFmpeg's AVVkFrame memory is typically NOT allocated with export handle types.
    // Attempting to export such memory generates Vulkan validation errors and may
    // cause side effects on some drivers. Skip the zero-copy attempt entirely
    // and let the copy path handle it.
    Q_UNUSED(srcDevice);
    Q_UNUSED(srcImage);
    Q_UNUSED(srcMemory);
    return false;
}

bool VulkanCrossDeviceBridge::copyToSharedAndExport(VkDevice srcDevice, VkPhysicalDevice srcPhysicalDevice,
                                                      VkImage srcImage, int width, int height, VkFormat format, int planeCount,
                                                      VkImageLayout srcLayout,
                                                      VkQueue srcQueue, uint32_t srcQueueFamilyIndex)
{
    const auto &dld = VkDispatch::dld();
    auto vkSrcDevice = vk::Device(srcDevice);
    auto physDev = vk::PhysicalDevice(srcPhysicalDevice);
    auto vkFormat = static_cast<vk::Format>(format);

    // Check if we can reuse existing per-plane shared images
    bool canReuse = m_initialized && !m_zeroCopy
        && m_width == width && m_height == height && m_format == format
        && m_planeCount == planeCount
        && m_srcDevice == srcDevice;

    if (canReuse) {
        // Verify all per-plane handles are still valid
        bool allHandlesValid = true;
        for (int i = 0; i < planeCount; ++i) {
            const bool hasHandle =
#ifdef Q_OS_WIN
                m_planes[i].memoryHandle != nullptr;
#elif defined(Q_OS_ANDROID)
                m_planes[i].ahwb != nullptr;
#else
                m_planes[i].memoryFd >= 0;
#endif
            if (!hasHandle) {
                allHandlesValid = false;
                break;
            }
        }
        if (allHandlesValid) {
        } else {
            // Need to re-export handles
            for (int i = 0; i < planeCount; ++i) {
                if (m_planes[i].srcSharedMemory != VK_NULL_HANDLE) {
                    if (!exportMemoryHandle(srcDevice, m_planes[i].srcSharedMemory, i))
                        return false;
                }
            }
        }
    } else {
        // Need to create new per-plane shared images
        // If the source device hasn't changed, it's safe to destroy old resources.
        // If the device changed (e.g. decoder switch), the old device may already
        // be destroyed, so we must not call vkDestroy* on it.
        const bool srcDeviceStillValid = (m_srcDevice == srcDevice);
        reset(srcDeviceStillValid);

        m_srcDevice = srcDevice;
        m_dstDevice = VK_NULL_HANDLE; // Will be set in copyFrameCrossDevice
        m_width = width;
        m_height = height;
        m_format = format;
        m_planeCount = planeCount;
        m_zeroCopy = false;

        // Create per-plane shared images on the source device
        for (int plane = 0; plane < planeCount; ++plane) {
            vk::Format perPlaneFormat = VkFormatUtils::getPerPlaneFormat(vkFormat, plane);
            if (perPlaneFormat == vk::Format::eUndefined) {
                qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan,
                    "Bridge: cannot determine per-plane format for plane {}", plane);
                return false;
            }

            int planeWidth = width;
            int planeHeight = height;
            if (plane > 0) {
                planeWidth = (planeWidth + 1) / 2;
                planeHeight = (planeHeight + 1) / 2;
            }

            auto &p = m_planes[plane];

            // Create external image with export capabilities for this plane.
            // We must create the image and memory separately because the memory
            // needs VkExportMemoryAllocateInfo to be exportable.
            vk::ExternalMemoryImageCreateInfo externalImgInfo;
#ifdef Q_OS_WIN
            externalImgInfo.handleTypes = vk::ExternalMemoryHandleTypeFlagBits::eOpaqueWin32;
#elif defined(Q_OS_ANDROID)
            externalImgInfo.handleTypes =
                vk::ExternalMemoryHandleTypeFlagBits::eAndroidHardwareBufferANDROID;
#else
            externalImgInfo.handleTypes = vk::ExternalMemoryHandleTypeFlagBits::eOpaqueFd;
#endif

            vk::ImageCreateInfo imageInfo(
                {},
                vk::ImageType::e2D,
                perPlaneFormat,
                vk::Extent3D(static_cast<uint32_t>(planeWidth), static_cast<uint32_t>(planeHeight), 1),
                1, 1,
                vk::SampleCountFlagBits::e1,
                vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
                vk::SharingMode::eExclusive);
            imageInfo.pNext = &externalImgInfo;

            vk::Image externalImg;
            try {
                externalImg = vkSrcDevice.createImage(imageInfo, nullptr, dld);
            } catch (vk::SystemError &) {
                qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan,
                    "Bridge: failed to create shared image for plane {}", plane);
                return false;
            }

            auto memReqs = vkSrcDevice.getImageMemoryRequirements(externalImg, dld);
            uint32_t memoryTypeIndex = VkMemory::findDeviceLocalMemoryType(physDev, memReqs.memoryTypeBits, dld);
            if (memoryTypeIndex == UINT32_MAX) {
                vkSrcDevice.destroyImage(externalImg, nullptr, dld);
                return false;
            }

            // Allocate memory with export handle types and dedicated allocation.
            // On Windows, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT requires
            // dedicated allocation (VkMemoryDedicatedAllocateInfo) for image exports.
            vk::ExportMemoryAllocateInfo exportMemInfo;
#ifdef Q_OS_WIN
            exportMemInfo.handleTypes = vk::ExternalMemoryHandleTypeFlagBits::eOpaqueWin32;
#elif defined(Q_OS_ANDROID)
            exportMemInfo.handleTypes =
                vk::ExternalMemoryHandleTypeFlagBits::eAndroidHardwareBufferANDROID;
#else
            exportMemInfo.handleTypes = vk::ExternalMemoryHandleTypeFlagBits::eOpaqueFd;
#endif

            vk::MemoryDedicatedAllocateInfo dedicatedAllocInfo;
            dedicatedAllocInfo.image = externalImg;
            dedicatedAllocInfo.pNext = &exportMemInfo;

            vk::MemoryAllocateInfo allocInfo(memReqs.size, memoryTypeIndex);
            allocInfo.pNext = &dedicatedAllocInfo;

            vk::DeviceMemory sharedMemory;
            try {
                sharedMemory = vkSrcDevice.allocateMemory(allocInfo, nullptr, dld);
            } catch (vk::SystemError &) {
                vkSrcDevice.destroyImage(externalImg, nullptr, dld);
                return false;
            }

            vkSrcDevice.bindImageMemory(externalImg, sharedMemory, 0, dld);

            p.srcSharedImage = static_cast<VkImage>(externalImg);
            p.srcSharedMemory = static_cast<VkDeviceMemory>(sharedMemory);
            p.planeWidth = planeWidth;
            p.planeHeight = planeHeight;
            p.planeFormat = static_cast<VkFormat>(perPlaneFormat);

            // Export the memory handle
            if (!exportMemoryHandle(srcDevice, p.srcSharedMemory, plane))
                return false;
        }
    }

    // Set up command resources
    // Must check device change in addition to pool null / qf change,
    // because copyToSharedAndExport may have reused shared images without
    // recreating the command pool (e.g. re-export path after handle loss).
    if (m_srcCommandPool == VK_NULL_HANDLE || m_srcQueueFamilyIndex != srcQueueFamilyIndex
        || m_srcDevice != srcDevice) {
        if (m_srcCommandPool != VK_NULL_HANDLE) {
            vkSrcDevice.destroyCommandPool(vk::CommandPool(m_srcCommandPool), nullptr, dld);
            m_srcCommandPool = VK_NULL_HANDLE;
            m_srcCommandBuffer = VK_NULL_HANDLE;
        }
        if (m_srcFence != VK_NULL_HANDLE) {
            vkSrcDevice.destroyFence(vk::Fence(m_srcFence), nullptr, dld);
            m_srcFence = VK_NULL_HANDLE;
        }

        vk::CommandPoolCreateInfo poolInfo(
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            srcQueueFamilyIndex);

        try {
            m_srcCommandPool = static_cast<VkCommandPool>(vkSrcDevice.createCommandPool(poolInfo, nullptr, dld));
        } catch (vk::SystemError &) {
            return false;
        }

        vk::CommandBufferAllocateInfo cmdInfo(
            vk::CommandPool(m_srcCommandPool),
            vk::CommandBufferLevel::ePrimary,
            1);

        try {
            auto cmdBuffers = vkSrcDevice.allocateCommandBuffers(cmdInfo, dld);
            m_srcCommandBuffer = static_cast<VkCommandBuffer>(cmdBuffers[0]);
        } catch (vk::SystemError &) {
            vkSrcDevice.destroyCommandPool(vk::CommandPool(m_srcCommandPool), nullptr, dld);
            m_srcCommandPool = VK_NULL_HANDLE;
            return false;
        }

        // Create a fence for precise synchronization instead of vkDeviceWaitIdle()
        vk::FenceCreateInfo fenceInfo;
        try {
            m_srcFence = static_cast<VkFence>(vkSrcDevice.createFence(fenceInfo, nullptr, dld));
        } catch (vk::SystemError &) {
            vkSrcDevice.destroyCommandPool(vk::CommandPool(m_srcCommandPool), nullptr, dld);
            m_srcCommandPool = VK_NULL_HANDLE;
            m_srcCommandBuffer = VK_NULL_HANDLE;
            return false;
        }

        m_srcQueueFamilyIndex = srcQueueFamilyIndex;
    }

    // Record and submit copy commands
    auto vkSrcLayout = static_cast<vk::ImageLayout>(srcLayout);
    auto commandBuffer = vk::CommandBuffer(m_srcCommandBuffer);
    try {
        commandBuffer.reset(vk::CommandBufferResetFlags(), dld);
    } catch (const vk::SystemError &) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan,
            "Bridge: failed to reset command buffer");
        return false;
    }

    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    commandBuffer.begin(beginInfo, dld);

    // Build barriers for all planes
    std::vector<vk::ImageMemoryBarrier> initialBarriers;
    initialBarriers.reserve(planeCount * 2);

    // Source image barriers: for multi-planar images, each plane must have its own
    // barrier with the plane-specific aspect (ePlane0, ePlane1, etc.).
    // Using eColor for a multi-planar image's subresource range is invalid when
    // the copy regions reference individual planes.
    for (int plane = 0; plane < planeCount; ++plane) {
        vk::ImageAspectFlagBits srcAspect = VkFormatUtils::getPlaneAspect(plane);
        initialBarriers.push_back(vk::ImageMemoryBarrier(
            vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eTransferRead,
            vkSrcLayout, vk::ImageLayout::eTransferSrcOptimal,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
            vk::Image(srcImage),
            vk::ImageSubresourceRange(srcAspect, 0, 1, 0, 1)));
    }

    // Per-plane destination barriers
    for (int plane = 0; plane < planeCount; ++plane) {
        auto &p = m_planes[plane];
        initialBarriers.push_back(vk::ImageMemoryBarrier(
            {}, vk::AccessFlagBits::eTransferWrite,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
            vk::Image(p.srcSharedImage),
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));
    }

    commandBuffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eTransfer,
        {}, {}, {}, initialBarriers, dld);

    // Copy each plane from the multi-planar source image to per-plane destination images
    for (int plane = 0; plane < planeCount; ++plane) {
        auto &p = m_planes[plane];
        vk::ImageAspectFlagBits srcAspect = VkFormatUtils::getPlaneAspect(plane);

        vk::ImageCopy copyRegion(
            vk::ImageSubresourceLayers(srcAspect, 0, 0, 1),
            vk::Offset3D(0, 0, 0),
            vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
            vk::Offset3D(0, 0, 0),
            vk::Extent3D(static_cast<uint32_t>(p.planeWidth), static_cast<uint32_t>(p.planeHeight), 1));

        commandBuffer.copyImage(
            vk::Image(srcImage), vk::ImageLayout::eTransferSrcOptimal,
            vk::Image(p.srcSharedImage), vk::ImageLayout::eTransferDstOptimal,
            1, &copyRegion, dld);
    }

    // Completion barriers for all planes
    // Source image: transition each plane back to the original layout
    // Destination images: transition to general layout for sampling
    std::vector<vk::ImageMemoryBarrier> completionBarriers;
    completionBarriers.reserve(planeCount * 2);
    for (int plane = 0; plane < planeCount; ++plane) {
        vk::ImageAspectFlagBits srcAspect = VkFormatUtils::getPlaneAspect(plane);
        completionBarriers.push_back(vk::ImageMemoryBarrier(
            vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eMemoryRead,
            vk::ImageLayout::eTransferSrcOptimal, vkSrcLayout,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
            vk::Image(srcImage),
            vk::ImageSubresourceRange(srcAspect, 0, 1, 0, 1)));
    }
    for (int plane = 0; plane < planeCount; ++plane) {
        auto &p = m_planes[plane];
        completionBarriers.push_back(vk::ImageMemoryBarrier(
            vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eMemoryRead,
            vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eGeneral,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
            vk::Image(p.srcSharedImage),
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));
    }

    commandBuffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eTransfer,
        {}, {}, {}, completionBarriers, dld);

    commandBuffer.end(dld);

    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    try {
        vkSrcDevice.resetFences(vk::Fence(m_srcFence), dld);
        vk::Queue(srcQueue).submit(submitInfo, vk::Fence(m_srcFence), dld);
    } catch (const vk::SystemError &e) {
        qz::Log::warn("Bridge: vkQueueSubmit failed: {}", e.what());
        return false;
    }
    // Use fence-based precise wait instead of vkDeviceWaitIdle().
    // vkDeviceWaitIdle() blocks until ALL GPU operations on the device complete,
    // which is extremely wasteful. A fence only waits for the specific submitted
    // command buffer, allowing other GPU work to proceed in parallel.
    constexpr uint64_t kFenceTimeoutNs = 1'000'000'000; // 1 second
    try {
        vk::Result waitResult = vkSrcDevice.waitForFences(vk::Fence(m_srcFence), VK_TRUE, kFenceTimeoutNs, dld);
        if (waitResult != vk::Result::eSuccess) {
            qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Bridge: fence wait timed out");
            return false;
        }
    } catch (const vk::SystemError &e) {
        qz::Log::warn("Bridge: vkWaitForFences failed: {}", e.what());
        return false;
    }

    m_initialized = true;
    return true;
}

bool VulkanCrossDeviceBridge::copyFrameCrossDevice(VkDevice srcDevice, VkPhysicalDevice srcPhysicalDevice,
                                                     VkImage srcImage, int width, int height, VkFormat format, int planeCount,
                                                     VkImageLayout srcLayout,
                                                     VkQueue srcQueue, uint32_t srcQueueFamilyIndex,
                                                     VkDevice dstDevice, VkPhysicalDevice dstPhysicalDevice)
{
    m_dstDevice = dstDevice;

    if (!copyToSharedAndExport(srcDevice, srcPhysicalDevice, srcImage, width, height, format, planeCount,
                               srcLayout, srcQueue, srcQueueFamilyIndex)) {
        return false;
    }

    // Create destination images on the RHI device for each plane
    for (int plane = 0; plane < planeCount; ++plane) {
        auto &p = m_planes[plane];
        VkImage dstImage = createDestinationImageFromImported(
            dstDevice, dstPhysicalDevice,
            p.planeWidth, p.planeHeight, p.planeFormat, plane);
        if (dstImage == VK_NULL_HANDLE) {
            qz::Log::warn("Bridge: failed to import destination image for plane {}", plane);
            return false;
        }
    }

    return true;
}

VkImage VulkanCrossDeviceBridge::importedImage(int plane) const
{
    if (plane >= 0 && plane < kMaxPlanes)
        return m_planes[plane].dstImage;
    return VK_NULL_HANDLE;
}

}

QT_END_NAMESPACE

#endif
