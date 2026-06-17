#include "FFmpegHwAccel_vulkan_single_p.h"

#if QT_FFMPEG_HAS_VULKAN

#include "FFmpegHwAccel_vulkan_p.h"
#include "FFmpegHwAccel_vulkan_utils_p.h"
#include "private/VkDeviceContext_p.h"
#include "private/VkCommandContext_p.h"
#include "private/VkImage_p.h"
#include "private/VkFormatUtils_p.h"
#include "private/VkDispatch_p.h"

import qzLog;
#include <rhi/qrhi.h>

extern "C" {
#include <libavutil/hwcontext_vulkan.h>
}

QT_BEGIN_NAMESPACE

namespace ffmpeg {

VulkanSingleDeviceConverter::VulkanSingleDeviceConverter(VkInstance vkInstance, VkDevice device,
                                                           VkPhysicalDevice physicalDevice,
                                                           VkQueue graphicsQueue, uint32_t graphicsQueueFamilyIndex)
    : m_vkInstance(vkInstance)
    , m_device(device)
    , m_physicalDevice(physicalDevice)
    , m_graphicsQueue(graphicsQueue)
    , m_graphicsQueueFamilyIndex(graphicsQueueFamilyIndex)
{
    if (m_device == VK_NULL_HANDLE || m_physicalDevice == VK_NULL_HANDLE || m_graphicsQueue == VK_NULL_HANDLE) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "SingleDevice: invalid Vulkan handles");
        return;
    }

    const auto &dld = VkDispatch::dld();
    auto vkDevice = vk::Device(m_device);

    try {
        vk::CommandPoolCreateInfo poolInfo(
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            m_graphicsQueueFamilyIndex);

        m_commandPool = static_cast<VkCommandPool>(vkDevice.createCommandPool(poolInfo, nullptr, dld));

        vk::CommandBufferAllocateInfo cmdInfo(
            vk::CommandPool(m_commandPool),
            vk::CommandBufferLevel::ePrimary,
            kCommandBufferCount);

        auto cmdBuffers = vkDevice.allocateCommandBuffers(cmdInfo, dld);
        for (auto &cmd : cmdBuffers)
            m_commandBufferPool.push_back(static_cast<VkCommandBuffer>(cmd));

        vk::SemaphoreTypeCreateInfo semaphoreType(vk::SemaphoreType::eTimeline, 0);
        vk::SemaphoreCreateInfo semaphoreInfo({}, &semaphoreType);
        m_timelineSemaphore = static_cast<VkSemaphore>(vkDevice.createSemaphore(semaphoreInfo, nullptr, dld));
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "SingleDevice: Vulkan error: {}", e.what());
        return;
    }

    m_valid = true;
    qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "SingleDevice: initialized successfully with {} command buffers", kCommandBufferCount);
}

VulkanSingleDeviceConverter::~VulkanSingleDeviceConverter()
{
    if (m_device == VK_NULL_HANDLE)
        return;

    const auto &dld = VkDispatch::dld();
    auto vkDevice = vk::Device(m_device);

    for (VkCommandBuffer cmdBuf : m_activeCommandBuffers) {
        if (cmdBuf != VK_NULL_HANDLE)
            vkDevice.freeCommandBuffers(vk::CommandPool(m_commandPool), vk::CommandBuffer(cmdBuf), dld);
    }
    m_activeCommandBuffers.clear();

    for (VkCommandBuffer cmdBuf : m_commandBufferPool) {
        if (cmdBuf != VK_NULL_HANDLE)
            vkDevice.freeCommandBuffers(vk::CommandPool(m_commandPool), vk::CommandBuffer(cmdBuf), dld);
    }
    m_commandBufferPool.clear();

    if (m_ycbcrConversion != VK_NULL_HANDLE) {
        vkDevice.destroySamplerYcbcrConversionKHR(
            vk::SamplerYcbcrConversion(m_ycbcrConversion), nullptr, dld);
    }
    if (m_timelineSemaphore != VK_NULL_HANDLE) {
        vkDevice.destroySemaphore(vk::Semaphore(m_timelineSemaphore), nullptr, dld);
        m_timelineSemaphore = VK_NULL_HANDLE;
    }
    if (m_commandPool != VK_NULL_HANDLE)
        vkDevice.destroyCommandPool(vk::CommandPool(m_commandPool), nullptr, dld);
}

void VulkanSingleDeviceConverter::setImagePool(VulkanOutputImagePool *pool)
{
    m_imagePool = pool;
}

VkCommandBuffer VulkanSingleDeviceConverter::acquireCommandBuffer()
{
    if (!m_commandBufferPool.empty()) {
        VkCommandBuffer cmdBuf = m_commandBufferPool.back();
        m_commandBufferPool.pop_back();

        const auto &dld = VkDispatch::dld();
        // Guard against stale/freed handles that may have entered the pool
        // through unexpected paths (e.g. after a device switch).
        try {
            vk::CommandBuffer(cmdBuf).reset(vk::CommandBufferResetFlags(), dld);
        } catch (const vk::SystemError &) {
            qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan,
                "SingleDevice: failed to reset pooled command buffer, discarding");
            return acquireCommandBuffer();  // Try next buffer or allocate new
        }

        m_activeCommandBuffers.push_back(cmdBuf);
        return cmdBuf;
    }

    qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "SingleDevice: no command buffers available, creating additional");
    const auto &dld = VkDispatch::dld();
    auto vkDevice = vk::Device(m_device);

    try {
        vk::CommandBufferAllocateInfo cmdInfo(
            vk::CommandPool(m_commandPool),
            vk::CommandBufferLevel::ePrimary,
            1);

        auto cmdBuffers = vkDevice.allocateCommandBuffers(cmdInfo, dld);
        VkCommandBuffer cmdBuf = static_cast<VkCommandBuffer>(cmdBuffers[0]);
        m_activeCommandBuffers.push_back(cmdBuf);
        return cmdBuf;
    } catch (const vk::SystemError &) {
        return VK_NULL_HANDLE;
    }
}

void VulkanSingleDeviceConverter::returnCommandBuffer(VkCommandBuffer cmdBuf)
{
    if (cmdBuf == VK_NULL_HANDLE)
        return;

    // Only accept command buffers that belong to this converter's pool.
    // Command buffers from a different VulkanSingleDeviceConverter (e.g. after
    // VulkanTextureConverter recreation) must not be added to our pool because
    // they are allocated from a different command pool and will be freed when
    // that converter is destroyed.
    auto it = std::find(m_activeCommandBuffers.begin(), m_activeCommandBuffers.end(), cmdBuf);
    if (it != m_activeCommandBuffers.end()) {
        m_activeCommandBuffers.erase(it);
        if (static_cast<int>(m_commandBufferPool.size()) < kCommandBufferCount) {
            m_commandBufferPool.push_back(cmdBuf);
        }
    }
}

VideoFrameTexturesHandlesUPtr
VulkanSingleDeviceConverter::createTextureHandles(AVFrame *frame,
                                                    VideoFrameTexturesHandlesUPtr oldHandles,
                                                    std::shared_ptr<TextureConverterBackend> converterBackend,
                                                    QRhi *rhi)
{
    if (!m_valid || !frame)
        return nullptr;

    AVVkFrame *vkFrame = vulkan_utils::getAVVkFrame(frame);
    if (!vkFrame)
        return nullptr;

    const int width = frame->width;
    const int height = frame->height;

    const AVHWFramesContext *framesCtx =
        reinterpret_cast<const AVHWFramesContext *>(frame->hw_frames_ctx->data);
    vk::Format vulkanFormat = VkFormatUtils::avPixelFormatToVulkan(
        static_cast<AVPixelFormat>(framesCtx->sw_format));

    if (vulkanFormat == vk::Format::eUndefined) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "SingleDevice: unsupported pixel format");
        return nullptr;
    }

    const int planeCount = VkFormatUtils::getPlaneCount(vulkanFormat);
    if (planeCount == 0) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "SingleDevice: cannot determine plane count for format {}", static_cast<int>(static_cast<VkFormat>(vulkanFormat)));
        return nullptr;
    }

    const auto *vkFramesCtxHw = static_cast<const AVVulkanFramesContext *>(framesCtx->hwctx);
    bool canDirectSample = (vkFramesCtxHw->usage & VK_IMAGE_USAGE_SAMPLED_BIT) != 0;

    if (canDirectSample && !dynamic_cast<VulkanTextureHandles *>(oldHandles.get())) {
        qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "VkVideo zero-copy: direct sampling path selected");

        // GPU-side synchronization: submit a layout transition command buffer that
        // waits on the decode semaphore before transitioning the image layout.
        // This ensures proper cross-queue memory visibility between the decode
        // queue and the graphics queue, which CPU-side waitSemaphores alone
        // cannot provide.

        VkSemaphore decodeSemaphore = vkFrame->sem[0];
        uint64_t decodeSemaphoreValue = vkFrame->sem_value[0];
        bool hasDecodeSemaphore = (decodeSemaphore != VK_NULL_HANDLE);

        VkCommandBuffer acquiredCmdBuf = acquireCommandBuffer();
        if (acquiredCmdBuf == VK_NULL_HANDLE) {
            qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan,
                "Zero-copy: failed to acquire command buffer for layout transition");
            return nullptr;
        }

        const auto &dld = VkDispatch::dld();
        auto cmdBuf = vk::CommandBuffer(acquiredCmdBuf);

        // Record layout transition: decode DST → shader read only
        vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        try {
            cmdBuf.begin(beginInfo, dld);
        } catch (const vk::SystemError &e) {
            qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan,
                "Zero-copy: failed to begin command buffer: {}", e.what());
            returnCommandBuffer(acquiredCmdBuf);
            return nullptr;
        }

        VkImage srcImage = vkFrame->img[0];
        VkImageLayout currentLayout = vkFrame->layout[0];

        // Transition from the decode output layout to shader read-only.
        // The decode semaphore ensures the GPU has finished writing before
        // this barrier executes, and the barrier makes the writes visible
        // to the fragment shader stage on the graphics queue.
        vulkan_utils::transitionImageLayoutCmd(
            acquiredCmdBuf, srcImage,
            currentLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

        cmdBuf.end(dld);

        // Submit with decode semaphore as wait semaphore for GPU-side sync
        ++m_timelineValue;
        uint64_t signalValue = m_timelineValue;

        vk::TimelineSemaphoreSubmitInfo timelineInfo;
        timelineInfo.signalSemaphoreValueCount = 1;
        timelineInfo.pSignalSemaphoreValues = &signalValue;

        vk::Semaphore decodeSemVk(decodeSemaphore);
        uint64_t decodeWaitValue = decodeSemaphoreValue;
        vk::PipelineStageFlags decodeWaitStage = vk::PipelineStageFlagBits::eFragmentShader;

        vk::SubmitInfo submitInfo;
        submitInfo.pNext = &timelineInfo;
        if (hasDecodeSemaphore) {
            timelineInfo.waitSemaphoreValueCount = 1;
            timelineInfo.pWaitSemaphoreValues = &decodeWaitValue;
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &decodeSemVk;
            submitInfo.pWaitDstStageMask = &decodeWaitStage;
        }
        submitInfo.signalSemaphoreCount = 1;
        vk::Semaphore timelineSemaphoreVk(m_timelineSemaphore);
        submitInfo.pSignalSemaphores = &timelineSemaphoreVk;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuf;

        try {
            vk::Queue(m_graphicsQueue).submit(submitInfo, {}, dld);
        } catch (const vk::SystemError &e) {
            qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan,
                "Zero-copy: failed to submit layout transition: {}", e.what());
            returnCommandBuffer(acquiredCmdBuf);
            return nullptr;
        }

        // Create handles with gpuSyncDone=true to skip CPU-side waitSemaphores.
        // The constructor will lock the frame, keeping it locked until destruction.
        auto handles = std::make_unique<VulkanDirectTextureHandles>(
            vkFrame,
            std::move(converterBackend), rhi,
            planeCount, width, height, static_cast<uint32_t>(vulkanFormat),
            framesCtx,
            true /* gpuSyncDone */);

        handles->setPendingTimeline(m_timelineSemaphore, signalValue);
        handles->setCommandBuffer(acquiredCmdBuf);

        return handles;
    }

    VulkanTextureHandles *oldVkHandles = nullptr;
    bool reuseTextures = false;

    if (!canDirectSample) {
        qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan,
            "VkVideo: copy path selected (reason: frame usage lacks SAMPLED_BIT)");
    } else {
        qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan,
            "VkVideo: copy path selected (reason: reusing existing VulkanTextureHandles)");
    }

    if (oldHandles) {
        oldVkHandles = dynamic_cast<VulkanTextureHandles *>(oldHandles.get());
        if (oldVkHandles && oldVkHandles->isCompatibleWith(width, height, static_cast<uint32_t>(vulkanFormat))) {
            reuseTextures = true;
            qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "SingleDevice: reusing existing output textures");
        }
    }

    if (oldVkHandles && oldVkHandles->hasPendingTimeline()) {
        oldVkHandles->waitTimeline();
    }

    if (oldVkHandles) {
        VkCommandBuffer oldCmdBuf = oldVkHandles->commandBuffer();
        if (oldCmdBuf != VK_NULL_HANDLE)
            returnCommandBuffer(oldCmdBuf);
    }

    VideoFrameTexturesHandlesUPtr handles;
    if (reuseTextures && oldVkHandles) {
        oldVkHandles->resetForReuse();
        oldHandles.release();
        handles.reset(oldVkHandles);
    } else {
        handles = std::make_unique<VulkanTextureHandles>(
            std::move(converterBackend), rhi, planeCount, width, height, static_cast<uint32_t>(vulkanFormat),
            m_imagePool);
    }

    auto vkHandles = static_cast<VulkanTextureHandles *>(handles.get());

    if (reuseTextures) {
        for (int plane = 0; plane < planeCount; ++plane) {
            vkHandles->setPlaneHandle(plane, oldVkHandles->planeImage(plane), oldVkHandles->planeMemory(plane));
            vkHandles->setPlaneOutputImage(plane, oldVkHandles->planeOutputImage(plane));
        }
        oldVkHandles->releaseResources();
    }

    if (VkFormatUtils::isYCbCrFormat(vulkanFormat) && m_ycbcrConversion == VK_NULL_HANDLE) {
        const auto &dld = VkDispatch::dld();
        auto result = VkImageWrapper::createYCbCrConversion(
            vk::Device(m_device), vulkanFormat, dld, frame->colorspace, frame->color_range);
        if (result) {
            m_ycbcrConversion = static_cast<VkSamplerYcbcrConversion>(result);
        } else {
            qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "SingleDevice: failed to create YCbCr conversion");
        }
    }

    // Only lock/unlock pool-allocated frames. Frames created by
    // copyFromHwPoolVulkan() are manually allocated (sem[0] == VK_NULL_HANDLE)
    // and must not be passed to lock_frame/unlock_frame.
    const bool isPoolFrame = (vkFrame->sem[0] != VK_NULL_HANDLE);

    if (isPoolFrame && vkFramesCtxHw->lock_frame) {
        vkFramesCtxHw->lock_frame(const_cast<AVHWFramesContext *>(framesCtx), vkFrame);
    }

    const auto &dld = VkDispatch::dld();
    auto vkDevice = vk::Device(m_device);
    auto physDev = vk::PhysicalDevice(m_physicalDevice);

    // Store the decode semaphore for GPU-side synchronization.
    // Instead of blocking the CPU with waitSemaphores, we pass the decode
    // semaphore as a wait semaphore in vkQueueSubmit. This allows the GPU to
    // handle the synchronization, and the CPU can continue processing.
    VkSemaphore decodeSemaphore = vkFrame->sem[0];
    uint64_t decodeSemaphoreValue = vkFrame->sem_value[0];
    bool hasDecodeSemaphore = (decodeSemaphore != VK_NULL_HANDLE);

    VkCommandBuffer acquiredCmdBuf = acquireCommandBuffer();
    if (acquiredCmdBuf == VK_NULL_HANDLE) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "SingleDevice: failed to acquire command buffer");
        if (isPoolFrame && vkFramesCtxHw->unlock_frame) {
            vkFramesCtxHw->unlock_frame(const_cast<AVHWFramesContext *>(framesCtx), vkFrame);
        }
        return nullptr;
    }
    auto cmdBuf = vk::CommandBuffer(acquiredCmdBuf);

    VkImage srcImage = vkFrame->img[0];

    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    try {
        cmdBuf.begin(beginInfo, dld);
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "SingleDevice: failed to begin command buffer: {}", e.what());
        if (isPoolFrame && vkFramesCtxHw->unlock_frame) {
            vkFramesCtxHw->unlock_frame(const_cast<AVHWFramesContext *>(framesCtx), vkFrame);
        }
        return nullptr;
    }

    std::vector<vk::ImageMemoryBarrier> initialBarriers;
    initialBarriers.reserve(planeCount + 1);

    initialBarriers.push_back(vk::ImageMemoryBarrier(
        vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eTransferRead,
        vk::ImageLayout::eGeneral, vk::ImageLayout::eGeneral,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
        vk::Image(srcImage),
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));

    struct PlaneCopyInfo {
        VkImage dstImage;
        int planeWidth;
        int planeHeight;
        vk::ImageAspectFlags srcAspect;
    };
    std::vector<PlaneCopyInfo> planeCopyInfos(planeCount);

    bool success = true;
    for (int plane = 0; plane < planeCount && success; ++plane) {
        vk::Format perPlaneFormat = VkFormatUtils::getPerPlaneFormat(vulkanFormat, plane);
        if (perPlaneFormat == vk::Format::eUndefined) {
            qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "SingleDevice: cannot determine per-plane format for plane {}", plane);
            success = false;
            break;
        }

        int planeWidth = width;
        int planeHeight = height;
        if (plane > 0) {
            planeWidth = (planeWidth + 1) / 2;
            planeHeight = (planeHeight + 1) / 2;
        }

        VkImage dstImage = VK_NULL_HANDLE;
        VkDeviceMemory dstMemory = VK_NULL_HANDLE;

        if (reuseTextures) {
            dstImage = reinterpret_cast<VkImage>(vkHandles->planeImage(plane));
            dstMemory = reinterpret_cast<VkDeviceMemory>(vkHandles->planeMemory(plane));

            initialBarriers.push_back(vk::ImageMemoryBarrier(
                vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite,
                vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eTransferDstOptimal,
                VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                vk::Image(dstImage),
                vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));
        } else {
            OutputImage *pooledImage = m_imagePool
                ? m_imagePool->acquireImage(planeWidth, planeHeight, static_cast<VkFormat>(perPlaneFormat), plane)
                : nullptr;

            if (pooledImage) {
                dstImage = pooledImage->image;
                dstMemory = pooledImage->memory;
                vkHandles->setPlaneOutputImage(plane, pooledImage);

                initialBarriers.push_back(vk::ImageMemoryBarrier(
                    vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite,
                    vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eTransferDstOptimal,
                    VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                    vk::Image(dstImage),
                    vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));
            } else {
                vk::DeviceMemory vkMemory = nullptr;
                auto imgResult = VkImageWrapper::createOptimal(
                    vkDevice, physDev,
                    static_cast<uint32_t>(planeWidth), static_cast<uint32_t>(planeHeight),
                    perPlaneFormat,
                    vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
                    nullptr, nullptr, vkMemory, dld);

                if (!imgResult) {
                    qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "SingleDevice: failed to create per-plane image for plane {}", plane);
                    success = false;
                    break;
                }

                dstImage = static_cast<VkImage>(imgResult);
                dstMemory = static_cast<VkDeviceMemory>(vkMemory);

                initialBarriers.push_back(vk::ImageMemoryBarrier(
                    {}, vk::AccessFlagBits::eTransferWrite,
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                    VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                    imgResult,
                    vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));
            }
        }

        planeCopyInfos[plane] = {dstImage, planeWidth, planeHeight,
            static_cast<vk::ImageAspectFlags>(VkFormatUtils::getPlaneAspect(plane))};

        if (!reuseTextures) {
            vkHandles->setPlaneHandle(plane,
                                     reinterpret_cast<uint64_t>(dstImage),
                                     reinterpret_cast<uint64_t>(dstMemory));
        }
    }

    if (!success) {
        if (isPoolFrame && vkFramesCtxHw->unlock_frame) {
            vkFramesCtxHw->unlock_frame(const_cast<AVHWFramesContext *>(framesCtx), vkFrame);
        }
        return nullptr;
    }

    cmdBuf.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer,
        {}, {}, {}, initialBarriers, dld);

    for (int plane = 0; plane < planeCount; ++plane) {
        const auto &info = planeCopyInfos[plane];

        vk::ImageCopy copyRegion(
            vk::ImageSubresourceLayers(info.srcAspect, 0, 0, 1),
            vk::Offset3D(0, 0, 0),
            vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
            vk::Offset3D(0, 0, 0),
            vk::Extent3D(static_cast<uint32_t>(info.planeWidth), static_cast<uint32_t>(info.planeHeight), 1));

        cmdBuf.copyImage(
            vk::Image(srcImage), vk::ImageLayout::eGeneral,
            vk::Image(info.dstImage), vk::ImageLayout::eTransferDstOptimal,
            1, &copyRegion, dld);
    }

    std::vector<vk::ImageMemoryBarrier> completionBarriers;
    completionBarriers.reserve(planeCount);
    for (int plane = 0; plane < planeCount; ++plane) {
        completionBarriers.push_back(vk::ImageMemoryBarrier(
            vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead,
            vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
            vk::Image(planeCopyInfos[plane].dstImage),
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));
    }

    cmdBuf.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
        {}, {}, {}, completionBarriers, dld);

    cmdBuf.end(dld);

    ++m_timelineValue;
    uint64_t signalValue = m_timelineValue;

    vk::TimelineSemaphoreSubmitInfo timelineInfo;
    timelineInfo.signalSemaphoreValueCount = 1;
    timelineInfo.pSignalSemaphoreValues = &signalValue;

    // If a decode semaphore is available, use it as a wait semaphore for
    // GPU-side synchronization instead of blocking the CPU with waitSemaphores.
    // The GPU will wait for the decode to complete before executing the copy.
    vk::Semaphore decodeSemVk(decodeSemaphore);
    uint64_t decodeWaitValue = decodeSemaphoreValue;
    vk::PipelineStageFlags decodeWaitStage = vk::PipelineStageFlagBits::eTransfer;

    vk::SubmitInfo submitInfo;
    submitInfo.pNext = &timelineInfo;
    if (hasDecodeSemaphore) {
        timelineInfo.waitSemaphoreValueCount = 1;
        timelineInfo.pWaitSemaphoreValues = &decodeWaitValue;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &decodeSemVk;
        submitInfo.pWaitDstStageMask = &decodeWaitStage;
    }
    submitInfo.signalSemaphoreCount = 1;
    vk::Semaphore timelineSemaphoreVk(m_timelineSemaphore);
    submitInfo.pSignalSemaphores = &timelineSemaphoreVk;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuf;

    try {
        vk::Queue(m_graphicsQueue).submit(submitInfo, {}, dld);
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "SingleDevice: failed to submit command buffer: {}", e.what());
        if (isPoolFrame && vkFramesCtxHw->unlock_frame) {
            vkFramesCtxHw->unlock_frame(const_cast<AVHWFramesContext *>(framesCtx), vkFrame);
        }
        return nullptr;
    }

    if (isPoolFrame && vkFramesCtxHw->unlock_frame) {
        vkFramesCtxHw->unlock_frame(const_cast<AVHWFramesContext *>(framesCtx), vkFrame);
    }

    vkHandles->setPendingTimeline(m_timelineSemaphore, signalValue);
    vkHandles->setCommandBuffer(acquiredCmdBuf);

    if (!success)
        return nullptr;

    return handles;
}

}

QT_END_NAMESPACE

#endif
