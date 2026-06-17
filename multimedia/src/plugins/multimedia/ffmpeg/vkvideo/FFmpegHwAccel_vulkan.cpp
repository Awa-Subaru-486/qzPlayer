#include "FFmpegHwAccel_vulkan_p.h"

#if QT_FFMPEG_HAS_VULKAN

#include "FFmpegHwAccel_vulkan_utils_p.h"
#include "playbackengine/FFmpegStreamDecoder_p.h"
#include "FFmpegVideoBuffer_p.h"
#include "private/VkDeviceContext_p.h"
#include "private/VkCommandContext_p.h"
#include "private/VkImage_p.h"
#include "private/VkMemory_p.h"
#include "private/VkFormatUtils_p.h"
#include "private/VkDispatch_p.h"

import qzLog;
#include <rhi/qrhi.h>

extern "C" {
#include <libavutil/hwcontext_vulkan.h>
#include <libavutil/pixdesc.h>
}

#include <vulkan/vulkan.hpp>
#include <cstring>

#ifdef Q_OS_WIN
#include <vulkan/vulkan_win32.h>
#endif

import qzVulkanContext;

QT_BEGIN_NAMESPACE

namespace ffmpeg {

struct VulkanTextureConverter::Private
{
    VkInstance vkInstance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamilyIndex = 0;

    // RHI's Vulkan device (for cross-device bridge destination and QRhiTexture creation)
    VkDevice rhiVkDevice = VK_NULL_HANDLE;
    VkPhysicalDevice rhiPhysicalDevice = VK_NULL_HANDLE;

    bool valid = false;
    bool usesExternalDevice = false;
    bool rhiIsVulkan = false;

    std::shared_ptr<VkDeviceContext> deviceContext;
    std::unique_ptr<VulkanOutputImagePool> imagePool;
};

VulkanTextureConverter::VulkanTextureConverter(QRhi *rhi)
    : TextureConverterBackend(rhi)
    , d(std::make_unique<Private>())
{
    d->valid = initializeFromRhi(rhi);
    if (!d->valid) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to initialize Vulkan texture converter");
    }
}

VulkanTextureConverter::~VulkanTextureConverter() = default;

bool VulkanTextureConverter::isValid() const
{
    return d->valid;
}

VulkanOutputImagePool *VulkanTextureConverter::imagePool() const
{
    return d->imagePool.get();
}

bool VulkanTextureConverter::initializeFromRhi(QRhi *rhi)
{
    // Try to initialize from qzVulkanContext singleton first
    // This allows Vulkan texture conversion even when RHI is not Vulkan (e.g., D3D11 on Windows)
    auto *vkContext = qzVulkanContext::instance();
    if (vkContext && vkContext->isInitialized()) {
        d->vkInstance = vkContext->vkInstance();
        d->physicalDevice = vkContext->physicalDevice();
        d->device = vkContext->device();

        // Get graphics queue from qzVulkanContext
        const auto &queueFamilies = vkContext->queueFamilies();
        for (const auto &[familyIndex, queueCount, flags, videoOps] : queueFamilies) {
            if (static_cast<VkQueueFlags>(flags) & VK_QUEUE_GRAPHICS_BIT) {
                d->graphicsQueueFamilyIndex = static_cast<uint32_t>(familyIndex);
                const auto &dld = VkDispatch::dld();
                d->graphicsQueue = static_cast<VkQueue>(
                    vk::Device(d->device).getQueue(d->graphicsQueueFamilyIndex, 0, dld));
                break;
            }
        }

        if (d->vkInstance == VK_NULL_HANDLE || d->physicalDevice == VK_NULL_HANDLE ||
            d->device == VK_NULL_HANDLE || d->graphicsQueue == VK_NULL_HANDLE) {
            qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Invalid Vulkan handles from qzVulkanContext");
        } else {
            // Store RHI's Vulkan device for cross-device bridge destination
            d->rhiIsVulkan = (rhi && rhi->backend() == QRhi::Vulkan);
            if (d->rhiIsVulkan) {
                const QRhiVulkanNativeHandles *nativeHandles =
                    static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles());
                if (nativeHandles) {
                    d->rhiVkDevice = nativeHandles->dev;
                    d->rhiPhysicalDevice = nativeHandles->physDev;
                }
            }

            // Same-device if qzVulkanContext's device matches RHI's Vulkan device
            d->usesExternalDevice = d->rhiIsVulkan && (d->rhiVkDevice == d->device);

            d->deviceContext = VkDeviceContext::fromQzVulkanContext();

            d->imagePool = std::make_unique<VulkanOutputImagePool>(d->device, d->physicalDevice);

            m_singleDeviceConverter = std::make_unique<VulkanSingleDeviceConverter>(
                d->vkInstance, d->device, d->physicalDevice,
                d->graphicsQueue, d->graphicsQueueFamilyIndex);
            m_singleDeviceConverter->setImagePool(d->imagePool.get());

            qz::Log::cat_info(vulkan_utils::qLcMediaFFmpegHWAccelVulkan,
                "Vulkan texture converter initialized from qzVulkanContext, sharedDevice={}, rhiIsVulkan={}",
                d->usesExternalDevice, d->rhiIsVulkan);
            return true;
        }
    }

    // Fallback: try to initialize from RHI Vulkan backend
    if (!rhi || rhi->backend() != QRhi::Vulkan) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "RHI is not Vulkan backend and qzVulkanContext not available");
        return false;
    }

    const QRhiVulkanNativeHandles *nativeHandles =
        static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles());
    if (!nativeHandles) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to get Vulkan native handles from RHI");
        return false;
    }

    if (!nativeHandles->inst) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "QVulkanInstance is null");
        return false;
    }

    d->vkInstance = nativeHandles->inst->vkInstance();
    d->physicalDevice = nativeHandles->physDev;
    d->device = nativeHandles->dev;
    d->graphicsQueue = nativeHandles->gfxQueue;
    d->graphicsQueueFamilyIndex = nativeHandles->gfxQueueFamilyIdx;
    d->rhiVkDevice = nativeHandles->dev;
    d->rhiPhysicalDevice = nativeHandles->physDev;
    d->rhiIsVulkan = true;

    if (d->vkInstance == VK_NULL_HANDLE || d->physicalDevice == VK_NULL_HANDLE ||
        d->device == VK_NULL_HANDLE || d->graphicsQueue == VK_NULL_HANDLE) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Invalid Vulkan handles from RHI");
        return false;
    }

    d->deviceContext = VkDeviceContext::fromQRhi(rhi);
    if (!d->deviceContext) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to create VkDeviceContext from RHI");
        return false;
    }

    d->usesExternalDevice = true;

    d->imagePool = std::make_unique<VulkanOutputImagePool>(d->device, d->physicalDevice);

    m_singleDeviceConverter = std::make_unique<VulkanSingleDeviceConverter>(
        d->vkInstance, d->device, d->physicalDevice,
        d->graphicsQueue, d->graphicsQueueFamilyIndex);
    m_singleDeviceConverter->setImagePool(d->imagePool.get());

    qz::Log::cat_info(vulkan_utils::qLcMediaFFmpegHWAccelVulkan,
        "Vulkan texture converter initialized from RHI, sharedDevice=true");
    return true;
}

VideoFrameTexturesHandlesUPtr
VulkanTextureConverter::createTextureHandles(AVFrame *frame,
                                              VideoFrameTexturesHandlesUPtr oldHandles)
{
    if (!d->valid || !frame || frame->format != AV_PIX_FMT_VULKAN) {
        return nullptr;
    }

    const AVHWDeviceContext *hwDevCtx = avFrameDeviceContext(frame);
    if (!hwDevCtx || hwDevCtx->type != AV_HWDEVICE_TYPE_VULKAN) {
        return nullptr;
    }

    const AVVulkanDeviceContext *vkDevCtx = vulkan_utils::getVulkanDeviceContext(hwDevCtx);
    if (!vkDevCtx) {
        return nullptr;
    }

    // Determine the render device: the device that will actually consume the texture.
    // If RHI is Vulkan, the render device is RHI's VkDevice; otherwise it's qzVulkanContext's device.
    // When FFmpeg's device differs from the render device, we need the cross-device bridge path.
    VkDevice renderDevice = (d->rhiIsVulkan && d->rhiVkDevice != VK_NULL_HANDLE) ? d->rhiVkDevice : d->device;

    if (vkDevCtx->act_dev != renderDevice) {
        // qz::Log::debug("Cross-device: FFmpeg device != RHI render device (ffmpeg={}, rhi={})",
        //     reinterpret_cast<uintptr_t>(vkDevCtx->act_dev), reinterpret_cast<uintptr_t>(renderDevice));

        AVVkFrame *vkFrame = vulkan_utils::getAVVkFrame(frame);
        if (!vkFrame) {
            return nullptr;
        }

        const auto *framesCtx =
            reinterpret_cast<const AVHWFramesContext *>(frame->hw_frames_ctx->data);
        const auto *vkFramesCtxHw = static_cast<const AVVulkanFramesContext *>(framesCtx->hwctx);

        // Only lock/unlock pool-allocated frames. Frames created by
        // copyFromHwPoolVulkan() are manually allocated (sem[0] == VK_NULL_HANDLE)
        // and must not be passed to lock_frame/unlock_frame.
        const bool isPoolFrame = (vkFrame->sem[0] != VK_NULL_HANDLE);

        if (isPoolFrame && vkFramesCtxHw->lock_frame)
            vkFramesCtxHw->lock_frame(const_cast<AVHWFramesContext *>(framesCtx), vkFrame);

        QScopeGuard autoUnlock([&] {
            if (isPoolFrame && vkFramesCtxHw->unlock_frame)
                vkFramesCtxHw->unlock_frame(const_cast<AVHWFramesContext *>(framesCtx), vkFrame);
        });

        VkFormat vulkanFormat = vulkan_utils::avPixelFormatToVulkanFormat(framesCtx->sw_format);
        if (vulkanFormat == VK_FORMAT_UNDEFINED) {
            qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Unsupported pixel format for cross-device sharing");
            return nullptr;
        }

        VkImage srcImage = vkFrame->img[0];
        VkDeviceMemory srcMemory = vkFrame->mem[0];
        VkDeviceSize srcMemorySize = vkFrame->size[0];
        VkImageLayout srcLayout = vkFrame->layout[0];
        uint32_t srcQueueFamilyIndex = vkDevCtx->qf[0].idx;
        VkQueue srcQueue = VK_NULL_HANDLE;
        {
            const auto &dld = VkDispatch::dld();
            srcQueue = static_cast<VkQueue>(
                vk::Device(vkDevCtx->act_dev).getQueue(srcQueueFamilyIndex, 0, dld));
        }

        // Use RHI's Vulkan device as the cross-device bridge destination
        // so the resulting VkImage can be used with QRhiTexture::createFrom()
        VkDevice dstDevice = d->rhiVkDevice != VK_NULL_HANDLE ? d->rhiVkDevice : d->device;
        VkPhysicalDevice dstPhysicalDevice = d->rhiPhysicalDevice != VK_NULL_HANDLE ? d->rhiPhysicalDevice : d->physicalDevice;

        const int planeCount = vulkan_utils::getVulkanPlaneCount(vulkanFormat);

        // Try zero-copy first (only works if FFmpeg allocated memory with export handle types)
        bool bridgeOk = m_crossDeviceBridge.importFrameZeroCopy(
            vkDevCtx->act_dev, vkDevCtx->phys_dev,
            srcImage, srcMemory, srcMemorySize,
            frame->width, frame->height, vulkanFormat,
            srcQueue, srcQueueFamilyIndex,
            dstDevice, dstPhysicalDevice);

        if (bridgeOk) {
        } else {
            bridgeOk = m_crossDeviceBridge.copyFrameCrossDevice(
                vkDevCtx->act_dev, vkDevCtx->phys_dev,
                srcImage, frame->width, frame->height, vulkanFormat, planeCount,
                srcLayout,
                srcQueue, srcQueueFamilyIndex,
                dstDevice, dstPhysicalDevice);
        }

        if (!bridgeOk) {
            return nullptr;
        }

        // Verify all plane images were imported successfully
        for (int plane = 0; plane < planeCount; ++plane) {
            const VkImage dstImage = m_crossDeviceBridge.importedImage(plane);
            if (dstImage == VK_NULL_HANDLE) {
                qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan,
                    "Bridge: imported image for plane {} is null", plane);
                return nullptr;
            }
        }

        // Release old handles if they exist.
        // The bridge reuses its per-plane resources when dimensions/format match,
        // so old handles' VkImage pointers may still be valid if nothing changed.
        if (oldHandles) {
            auto *oldVkHandles = dynamic_cast<VulkanTextureHandles *>(oldHandles.get());
            if (oldVkHandles)
                oldVkHandles->releaseResources();
        }
        oldHandles.reset();

        // Pass nullptr pool: the bridge owns the dstImages, not the pool.
        // setOwnsResources(false) prevents the destructor from destroying the bridge's images.
        auto handles = std::make_unique<VulkanTextureHandles>(
            shared_from_this(), rhi, planeCount, frame->width, frame->height,
            static_cast<uint32_t>(vulkanFormat), nullptr
        );
        handles->setOwnsResources(false);

        // Set handle for each plane
        for (int plane = 0; plane < planeCount; ++plane) {
            VkImage dstImage = m_crossDeviceBridge.importedImage(plane);
            handles->setPlaneHandle(plane, reinterpret_cast<uint64_t>(dstImage), 0);
        }

        // Write back the AVVkFrame layout/access after the cross-device copy.
        // The copy command transitions the source image to TRANSFER_SRC_OPTIMAL,
        // and the copy is synchronous (we waited for the timeline semaphore),
        // so we can safely update the frame's layout now.
        if (!m_crossDeviceBridge.isZeroCopy()) {
            vkFrame->layout[0] = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            vkFrame->access[0] = VK_ACCESS_TRANSFER_READ_BIT;
        }

        return handles;
    }

    qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Zero-copy path selected: single device direct sampling");
    return m_singleDeviceConverter->createTextureHandles(
        frame, std::move(oldHandles), shared_from_this(), rhi);
}

void VulkanTextureConverter::SetupDecoderTextures(AVCodecContext *s)
{
    int ret = avcodec_get_hw_frames_parameters(s, s->hw_device_ctx, AV_PIX_FMT_VULKAN,
                                                &s->hw_frames_ctx);
    if (ret < 0) {
        qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to allocate HW frames context {}", ret);
        return;
    }

    auto *framesCtx = reinterpret_cast<AVHWFramesContext *>(s->hw_frames_ctx->data);
    auto *vkFramesCtx = static_cast<AVVulkanFramesContext *>(framesCtx->hwctx);

    // framesCtx->initial_pool_size = 16;

    vkFramesCtx->tiling = VK_IMAGE_TILING_OPTIMAL;
    // Vulkan Video decode output images MUST have VIDEO_DECODE_DST_BIT_KHR.
    // SAMPLED_BIT is needed for zero-copy rendering (shader sampling).
    // TRANSFER_SRC_BIT is needed for potential GPU-side copies.
    vkFramesCtx->usage = static_cast<VkImageUsageFlagBits>(
        VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR |
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    ret = av_hwframe_ctx_init(s->hw_frames_ctx);
    if (ret < 0) {
        qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to initialize HW frames context {}", ret);
        av_buffer_unref(&s->hw_frames_ctx);
    }
}

VulkanTextureHandles::VulkanTextureHandles(TextureConverterBackendPtr &&converterBackend,
                                           QRhi *rhi,
                                           int planeCount, int width, int height, uint32_t format,
                                           VulkanOutputImagePool *pool)
    : m_parentConverterBackend(std::move(converterBackend))
    , m_owner(rhi)
    , m_pool(pool)
    , m_planeCount(planeCount)
    , m_width(width)
    , m_height(height)
    , m_format(format)
{
}

VulkanTextureHandles::~VulkanTextureHandles()
{
    if (hasPendingTimeline())
        waitTimeline();

    // Return the command buffer to the VulkanSingleDeviceConverter that owns it.
    // This must happen before m_parentConverterBackend is released, because the
    // converter might be destroyed when the last shared_ptr reference is dropped.
    if (m_commandBuffer != VK_NULL_HANDLE && m_parentConverterBackend) {
        auto *vkConverter = dynamic_cast<VulkanTextureConverter *>(m_parentConverterBackend.get());
        if (vkConverter && vkConverter->m_singleDeviceConverter) {
            vkConverter->m_singleDeviceConverter->returnCommandBuffer(m_commandBuffer);
        }
        m_commandBuffer = VK_NULL_HANDLE;
    }

    if (m_resourcesReleased || !m_ownsResources)
        return;

    for (int i = 0; i < m_planeCount; ++i) {
        if (m_planeOutputImages[i] && m_pool) {
            m_pool->releaseImage(m_planeOutputImages[i]);
            m_planeOutputImages[i] = nullptr;
            m_planeImages[i] = 0;
            m_planeMemory[i] = 0;
        }
    }

    if (!m_pool) {
        const QRhiVulkanNativeHandles *nativeHandles =
            static_cast<const QRhiVulkanNativeHandles *>(m_owner->nativeHandles());
        VkDevice device = nativeHandles->dev;

        const auto &dld = VkDispatch::dld();
        auto vkDevice = vk::Device(device);

        for (int i = 0; i < m_planeCount; ++i) {
            if (m_planeImages[i]) {
                vkDevice.destroyImage(vk::Image(reinterpret_cast<VkImage>(m_planeImages[i])), nullptr, dld);
                m_planeImages[i] = 0;
            }
            if (m_planeMemory[i]) {
                vkDevice.freeMemory(vk::DeviceMemory(reinterpret_cast<VkDeviceMemory>(m_planeMemory[i])), nullptr, dld);
                m_planeMemory[i] = 0;
            }
        }
    }
}

void VulkanTextureHandles::setPlaneHandle(int plane, uint64_t image, uint64_t memory)
{
    if (plane >= 0 && plane < m_planeCount) {
        m_planeImages[plane] = image;
        m_planeMemory[plane] = memory;
    }
}

void VulkanTextureHandles::setPlaneOutputImage(int plane, OutputImage *outputImage)
{
    if (plane >= 0 && plane < m_planeCount) {
        m_planeOutputImages[plane] = outputImage;
        if (outputImage) {
            m_planeImages[plane] = reinterpret_cast<uint64_t>(outputImage->image);
            m_planeMemory[plane] = reinterpret_cast<uint64_t>(outputImage->memory);
        }
    }
}

quint64 VulkanTextureHandles::textureHandle(QRhi &rhi, int plane)
{
    if (&rhi != m_owner)
        return 0;

    if (hasPendingTimeline() && !isTimelineSignaled()) {
        if (plane >= 0 && plane < m_planeCount)
            return m_planeImages[plane];
        return 0;
    }

    if (hasPendingTimeline())
        waitTimeline();

    if (plane >= 0 && plane < m_planeCount)
        return m_planeImages[plane];

    return 0;
}

uint64_t VulkanTextureHandles::planeImage(int plane) const
{
    if (plane >= 0 && plane < m_planeCount)
        return m_planeImages[plane];
    return 0;
}

uint64_t VulkanTextureHandles::planeMemory(int plane) const
{
    if (plane >= 0 && plane < m_planeCount)
        return m_planeMemory[plane];
    return 0;
}

OutputImage *VulkanTextureHandles::planeOutputImage(int plane) const
{
    if (plane >= 0 && plane < m_planeCount)
        return m_planeOutputImages[plane];
    return nullptr;
}

void VulkanTextureHandles::setPendingTimeline(VkSemaphore semaphore, uint64_t value)
{
    m_timelineSemaphore = semaphore;
    m_pendingTimelineValue = value;
    if (semaphore != VK_NULL_HANDLE) {
        const QRhiVulkanNativeHandles *nativeHandles =
            static_cast<const QRhiVulkanNativeHandles *>(m_owner->nativeHandles());
        if (nativeHandles)
            m_timelineDevice = nativeHandles->dev;
    }
}

VkSemaphore VulkanTextureHandles::pendingTimelineSemaphore() const
{
    return m_timelineSemaphore;
}

uint64_t VulkanTextureHandles::pendingTimelineValue() const
{
    return m_pendingTimelineValue;
}

bool VulkanTextureHandles::hasPendingTimeline() const
{
    return m_timelineSemaphore != VK_NULL_HANDLE && m_pendingTimelineValue > 0;
}

bool VulkanTextureHandles::isTimelineSignaled() const
{
    if (m_timelineSemaphore == VK_NULL_HANDLE || m_timelineDevice == VK_NULL_HANDLE || m_pendingTimelineValue == 0)
        return true;

    const auto &dld = VkDispatch::dld();
    auto vkDevice = vk::Device(m_timelineDevice);

    try {
        VkSemaphoreWaitInfo waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.semaphoreCount = 1;
        waitInfo.pSemaphores = &m_timelineSemaphore;
        waitInfo.pValues = &m_pendingTimelineValue;
        vk::Result result = vkDevice.waitSemaphores(vk::SemaphoreWaitInfo(waitInfo), 0, dld);
        return result == vk::Result::eSuccess;
    } catch (const vk::SystemError &) {
        return false;
    }
}

void VulkanTextureHandles::waitTimeline()
{
    if (m_timelineSemaphore == VK_NULL_HANDLE || m_timelineDevice == VK_NULL_HANDLE || m_pendingTimelineValue == 0)
        return;

    const auto &dld = VkDispatch::dld();
    auto vkDevice = vk::Device(m_timelineDevice);

    try {
        VkSemaphoreWaitInfo waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.semaphoreCount = 1;
        waitInfo.pSemaphores = &m_timelineSemaphore;
        waitInfo.pValues = &m_pendingTimelineValue;
        (void)vkDevice.waitSemaphores(vk::SemaphoreWaitInfo(waitInfo), UINT64_MAX, dld);
    } catch (const vk::SystemError &) {
    }

    m_pendingTimelineValue = 0;
}

void VulkanTextureHandles::setCommandBuffer(VkCommandBuffer cmdBuf)
{
    m_commandBuffer = cmdBuf;
}

VkCommandBuffer VulkanTextureHandles::commandBuffer() const
{
    return m_commandBuffer;
}

bool VulkanTextureHandles::isCompatibleWith(int width, int height, uint32_t format) const
{
    return m_width == width && m_height == height && m_format == format;
}

void VulkanTextureHandles::resetForReuse()
{
    m_timelineSemaphore = VK_NULL_HANDLE;
    m_pendingTimelineValue = 0;
    m_commandBuffer = VK_NULL_HANDLE;
}

void VulkanTextureHandles::releaseResources()
{
    m_resourcesReleased = true;
    for (int i = 0; i < m_planeCount; ++i) {
        m_planeImages[i] = 0;
        m_planeMemory[i] = 0;
        m_planeOutputImages[i] = nullptr;
    }
}

VulkanDirectTextureHandles::VulkanDirectTextureHandles(
    AVVkFrame *vkFrame,
    TextureConverterBackendPtr &&converterBackend,
    QRhi *rhi, int planeCount, int width, int height, uint32_t format,
    const AVHWFramesContext *framesCtx,
    bool gpuSyncDone)
    : m_vkFrame(vkFrame)
    , m_parentConverterBackend(std::move(converterBackend))
    , m_owner(rhi)
    , m_framesCtx(framesCtx)
    , m_planeCount(planeCount)
    , m_width(width)
    , m_height(height)
    , m_format(format)
    , m_gpuSyncDone(gpuSyncDone)
{
    if (m_framesCtx) {
        const auto *vkFramesCtxHw = static_cast<const AVVulkanFramesContext *>(m_framesCtx->hwctx);
        // Only lock pool-allocated frames. Manually created frames from
        // copyFromHwPoolVulkan() have sem[0] == VK_NULL_HANDLE and must
        // not be passed to lock_frame/unlock_frame.
        if (vkFramesCtxHw && vkFramesCtxHw->lock_frame && m_vkFrame->sem[0] != VK_NULL_HANDLE) {
            vkFramesCtxHw->lock_frame(const_cast<AVHWFramesContext *>(m_framesCtx), m_vkFrame);
            m_locked = true;
        }
    }

    m_initialLayout = vkFrame->layout[0];
    m_initialAccess = vkFrame->access[0];

    // Store decode semaphore for GPU-side synchronization
    m_decodeSemaphore = vkFrame->sem[0];
    m_decodeSemaphoreValue = vkFrame->sem_value[0];

    // When GPU-side sync has been done (layout transition command buffer
    // submitted with decode semaphore as wait semaphore), skip the
    // CPU-side waitSemaphores. The GPU-side approach provides proper
    // cross-queue memory dependency that CPU-side waitSemaphores lacks.
    if (!m_gpuSyncDone && m_vkFrame && m_vkFrame->sem[0] != VK_NULL_HANDLE) {
        const auto &dld = VkDispatch::dld();
        auto vkDevice = vk::Device(static_cast<const QRhiVulkanNativeHandles *>(
            m_owner->nativeHandles())->dev);

        // Try non-blocking check first to avoid CPU stall
        VkSemaphoreWaitInfo vkWaitInfo{};
        vkWaitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        vkWaitInfo.semaphoreCount = 1;
        vkWaitInfo.pSemaphores = &m_vkFrame->sem[0];
        vkWaitInfo.pValues = &m_vkFrame->sem_value[0];
        vk::Result result = vkDevice.waitSemaphores(vk::SemaphoreWaitInfo(vkWaitInfo), 0, dld);
        if (result == vk::Result::eSuccess) {
            m_decodeWaited = true;
        } else {
            // Semaphore not yet signaled, must block-wait
            (void)vkDevice.waitSemaphores(vk::SemaphoreWaitInfo(vkWaitInfo), UINT64_MAX, dld);
            m_decodeWaited = true;
        }
    }
}

VulkanDirectTextureHandles::~VulkanDirectTextureHandles()
{
    // Wait for GPU-side layout transition to complete before unlocking the frame
    if (hasPendingTimeline())
        waitTimeline();

    // Return the command buffer to the VulkanSingleDeviceConverter that owns it.
    // This must happen before m_parentConverterBackend is released, because the
    // converter might be destroyed when the last shared_ptr reference is dropped.
    if (m_commandBuffer != VK_NULL_HANDLE && m_parentConverterBackend) {
        auto *vkConverter = dynamic_cast<VulkanTextureConverter *>(m_parentConverterBackend.get());
        if (vkConverter && vkConverter->m_singleDeviceConverter) {
            vkConverter->m_singleDeviceConverter->returnCommandBuffer(m_commandBuffer);
        }
        m_commandBuffer = VK_NULL_HANDLE;
    }

    if (m_vkFrame) {
        m_vkFrame->layout[0] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_vkFrame->access[0] = VK_ACCESS_SHADER_READ_BIT;
    }

    if (m_locked && m_framesCtx) {
        const auto *vkFramesCtxHw = static_cast<const AVVulkanFramesContext *>(m_framesCtx->hwctx);
        if (vkFramesCtxHw && vkFramesCtxHw->unlock_frame) {
            vkFramesCtxHw->unlock_frame(const_cast<AVHWFramesContext *>(m_framesCtx), m_vkFrame);
        }
        m_locked = false;
    }
}

quint64 VulkanDirectTextureHandles::textureHandle(QRhi &rhi, int plane)
{
    if (&rhi != m_owner || !m_vkFrame)
        return 0;

    if (plane >= 0 && plane < m_planeCount) {
        return reinterpret_cast<quint64>(m_vkFrame->img[0]);
    }

    return 0;
}

void VulkanDirectTextureHandles::updateAVVkFrameLayout()
{
    if (m_vkFrame) {
        m_vkFrame->layout[0] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_vkFrame->access[0] = VK_ACCESS_SHADER_READ_BIT;
    }
}

void VulkanDirectTextureHandles::setPendingTimeline(VkSemaphore semaphore, uint64_t value)
{
    m_timelineSemaphore = semaphore;
    m_pendingTimelineValue = value;
    if (semaphore != VK_NULL_HANDLE) {
        const QRhiVulkanNativeHandles *nativeHandles =
            static_cast<const QRhiVulkanNativeHandles *>(m_owner->nativeHandles());
        if (nativeHandles)
            m_timelineDevice = nativeHandles->dev;
    }
}

bool VulkanDirectTextureHandles::hasPendingTimeline() const
{
    return m_timelineSemaphore != VK_NULL_HANDLE && m_pendingTimelineValue > 0;
}

bool VulkanDirectTextureHandles::isTimelineSignaled() const
{
    if (m_timelineSemaphore == VK_NULL_HANDLE || m_timelineDevice == VK_NULL_HANDLE || m_pendingTimelineValue == 0)
        return true;

    const auto &dld = VkDispatch::dld();
    auto vkDevice = vk::Device(m_timelineDevice);

    try {
        VkSemaphoreWaitInfo waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.semaphoreCount = 1;
        waitInfo.pSemaphores = &m_timelineSemaphore;
        waitInfo.pValues = &m_pendingTimelineValue;
        vk::Result result = vkDevice.waitSemaphores(vk::SemaphoreWaitInfo(waitInfo), 0, dld);
        return result == vk::Result::eSuccess;
    } catch (const vk::SystemError &) {
        return false;
    }
}

void VulkanDirectTextureHandles::waitTimeline()
{
    if (m_timelineSemaphore == VK_NULL_HANDLE || m_timelineDevice == VK_NULL_HANDLE || m_pendingTimelineValue == 0)
        return;

    const auto &dld = VkDispatch::dld();
    auto vkDevice = vk::Device(m_timelineDevice);

    try {
        VkSemaphoreWaitInfo waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.semaphoreCount = 1;
        waitInfo.pSemaphores = &m_timelineSemaphore;
        waitInfo.pValues = &m_pendingTimelineValue;
        (void)vkDevice.waitSemaphores(vk::SemaphoreWaitInfo(waitInfo), UINT64_MAX, dld);
    } catch (const vk::SystemError &) {
    }

    m_pendingTimelineValue = 0;
}

void VulkanDirectTextureHandles::setCommandBuffer(VkCommandBuffer cmdBuf)
{
    m_commandBuffer = cmdBuf;
}

VkCommandBuffer VulkanDirectTextureHandles::commandBuffer() const
{
    return m_commandBuffer;
}

VulkanOutputImagePool::VulkanOutputImagePool(VkDevice device, VkPhysicalDevice physicalDevice)
    : m_device(device)
    , m_physicalDevice(physicalDevice)
{
}

VulkanOutputImagePool::~VulkanOutputImagePool()
{
    const auto &dld = VkDispatch::dld();
    auto vkDevice = vk::Device(m_device);

    for (auto &img : m_pool) {
        destroyImage(img.get(), vkDevice, dld);
    }
    m_pool.clear();
}

OutputImage *VulkanOutputImagePool::acquireImage(int width, int height, VkFormat format, int planeIndex)
{
    std::vector<std::unique_ptr<OutputImage>> imagesToClear = takeImagesToClear(width, height, format, planeIndex);
    imagesToClear.clear();

    for (auto it = m_pool.begin(); it != m_pool.end(); ++it) {
        OutputImage *img = it->get();
        if (img && img->width == width && img->height == height
            && img->format == format && img->planeIndex == planeIndex && !img->inUse) {
            img->inUse = true;
            it->release();
            return img;
        }
    }

    auto image = std::make_unique<OutputImage>();
    image->width = width;
    image->height = height;
    image->format = format;
    image->planeIndex = planeIndex;
    image->inUse = true;

    if (!vulkan_utils::createVulkanImage(m_device, m_physicalDevice,
                                           static_cast<uint32_t>(width), static_cast<uint32_t>(height), format,
                                           VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                           nullptr, nullptr,
                                           image->image, image->memory)) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Pool: failed to create image");
        return nullptr;
    }

    qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Pool: created new image {}x{} format:{} plane:{} pool size:{}", width, height, static_cast<int>(format), planeIndex, m_pool.size());
    return image.release();
}

void VulkanOutputImagePool::releaseImage(OutputImage *image)
{
    if (!image)
        return;

    image->inUse = false;

    if (m_pool.size() >= kMaxPoolSize) {
        const auto &dld = VkDispatch::dld();
        destroyImage(image, vk::Device(m_device), dld);
        delete image;
        return;
    }

    m_pool.emplace_back(image);
    qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Pool: returned image to pool, size:{}", m_pool.size());
}

std::vector<std::unique_ptr<OutputImage>> VulkanOutputImagePool::takeImagesToClear(int width, int height, VkFormat format, int planeIndex)
{
    std::vector<std::unique_ptr<OutputImage>> imagesToClear;

    if (m_pool.empty())
        return imagesToClear;

    OutputImage *first = m_pool[0].get();
    if (!first)
        return imagesToClear;

    if (first->width != width || first->height != height
        || first->format != format || first->planeIndex != planeIndex) {
        for (auto it = m_pool.begin(); it != m_pool.end();) {
            if (!it->get()->inUse) {
                imagesToClear.push_back(std::move(*it));
                it = m_pool.erase(it);
            } else {
                ++it;
            }
        }
    }

    return imagesToClear;
}

void VulkanOutputImagePool::destroyImage(OutputImage *image, vk::Device vkDevice, const vk::detail::DispatchLoaderDynamic &dld)
{
    if (!image)
        return;

    if (image->image != VK_NULL_HANDLE) {
        vkDevice.destroyImage(vk::Image(image->image), nullptr, dld);
        image->image = VK_NULL_HANDLE;
    }
    if (image->memory != VK_NULL_HANDLE) {
        vkDevice.freeMemory(vk::DeviceMemory(image->memory), nullptr, dld);
        image->memory = VK_NULL_HANDLE;
    }
}

namespace {
// Check if a queue family index has the graphics flag
bool hasQueueFamilyGraphicsFlag(const std::vector<qzVulkanContext::QueueFamilyInfo> &queueFamilies, int idx)
{
    for (const auto &[familyIndex, queueCount, flags, videoOps] : queueFamilies) {
        if (static_cast<int>(familyIndex) == idx)
            return (static_cast<VkQueueFlags>(flags) & VK_QUEUE_GRAPHICS_BIT) != 0;
    }
    return false;
}
} // namespace

AVBufferUPtr createVulkanDeviceContextFromRhi(QRhi *rhi)
{
    // Always use qzVulkanContext's device for FFmpeg, since it has video decode extensions.
    // RHI's Vulkan device typically doesn't have video decode extensions enabled.
    // The sharedDevice flag is determined by comparing qzVulkanContext's VkDevice with RHI's VkDevice.
    auto *vkContext = qzVulkanContext::instance();
    if (!vkContext || !vkContext->isInitialized()) {
        qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "qzVulkanContext not initialized");
        return {};
    }

    VkInstance vkInstance = vkContext->vkInstance();
    VkPhysicalDevice physicalDevice = vkContext->physicalDevice();
    VkDevice device = vkContext->device();
    PFN_vkGetInstanceProcAddr getProcAddr = vkContext->getVkGetInstanceProcAddr();
    VkPhysicalDeviceFeatures2 deviceFeatures = vkContext->enabledDeviceFeatures();

    if (!getProcAddr || vkInstance == VK_NULL_HANDLE || device == VK_NULL_HANDLE) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to get Vulkan handles from qzVulkanContext");
        return {};
    }

    // Log whether this is same-device or cross-device
    if (rhi && rhi->backend() == QRhi::Vulkan) {
        const QRhiVulkanNativeHandles *nativeHandles =
            static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles());
        if (nativeHandles && nativeHandles->dev != VK_NULL_HANDLE) {
            if (nativeHandles->dev == device) {
                qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan,
                    "FFmpeg Vulkan device matches RHI device (same-device zero-copy possible)");
            } else {
                qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan,
                    "FFmpeg Vulkan device differs from RHI device (cross-device bridge needed)");
            }
        }
    }

    AVBufferRef *hwDeviceCtx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VULKAN);
    if (!hwDeviceCtx) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to allocate Vulkan device context");
        return {};
    }

    auto *ctx = reinterpret_cast<AVHWDeviceContext *>(hwDeviceCtx->data);
    auto *vkCtx = static_cast<AVVulkanDeviceContext *>(ctx->hwctx);

    vkCtx->inst = vkInstance;
    vkCtx->phys_dev = physicalDevice;
    vkCtx->act_dev = device;
    vkCtx->get_proc_addr = getProcAddr;
    vkCtx->device_features = deviceFeatures;

    if (!vkCtx->get_proc_addr) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to get vkGetInstanceProcAddr");
        av_buffer_unref(&hwDeviceCtx);
        return {};
    }

    // Get extensions directly from qzVulkanContext's persistent storage.
    // We must NOT copy to a local vector and then take c_str(), because the local
    // vector would be destroyed when the function returns, leaving dangling pointers.
    // Instead, take c_str() directly from the persistent unordered_set<std::string>.
    const auto &instExts = vkContext->enabledInstanceExtensions();
    const auto &devExts = vkContext->enabledDeviceExtensions();
    bool hasVideoDecode = vkContext->hasDeviceExtension(VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME);

    // Platform-specific external memory extension that FFmpeg doesn't understand.
    // Must be skipped, same as QMPlay2 does.
    const char *skipExtName =
#ifdef VK_USE_PLATFORM_WIN32_KHR
        VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME;
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
        VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME;
#else
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME;
#endif

    // Instance extensions - take c_str() from persistent set
    vkCtx->nb_enabled_inst_extensions = static_cast<int>(instExts.size());
    auto instExtArray = new const char *[instExts.size()];
    {
        int j = 0;
        for (const auto &str : instExts)
            instExtArray[j++] = str.c_str();
    }
    vkCtx->enabled_inst_extensions = instExtArray;

    // Device extensions - take c_str() from persistent set, skip platform-specific ext
    int devExtCount = 0;
    for (const auto &str : devExts) {
        if (strcmp(str.c_str(), skipExtName) != 0)
            ++devExtCount;
    }
    vkCtx->nb_enabled_dev_extensions = devExtCount;
    auto devExtArray = new const char *[devExtCount];
    {
        int index = 0;
        for (const auto &str : devExts) {
            if (strcmp(str.c_str(), skipExtName) != 0)
                devExtArray[index++] = str.c_str();
        }
    }
    vkCtx->enabled_dev_extensions = devExtArray;

    // Debug: verify the video decode extension is present in the array
    {
        bool foundVideoDecodeExt = false;
        for (int i = 0; i < vkCtx->nb_enabled_dev_extensions; ++i) {
            if (strcmp(vkCtx->enabled_dev_extensions[i], VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME) == 0) {
                foundVideoDecodeExt = true;
                break;
            }
        }
        qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan,
            "Device extensions: {} total, hasVideoDecodeExt={}, skipped ext: {}",
            vkCtx->nb_enabled_dev_extensions, foundVideoDecodeExt, skipExtName);
    }

    ctx->free = [](AVHWDeviceContext *ctx) {
        const auto *vk_ctx = static_cast<AVVulkanDeviceContext *>(ctx->hwctx);
        delete[] vk_ctx->enabled_dev_extensions;
        delete[] vk_ctx->enabled_inst_extensions;
    };

    // Queue family configuration - follow QMPlay2's approach:
    // Iterate over all queue families, prefer dedicated (non-graphics) queues
    // for non-graphics slots to avoid starving the graphics queue.
    const auto &queueFamilies = vkContext->queueFamilies();
    vkCtx->nb_qf = 0;

    for (const auto &[familyIndex, queueCount, flags, videoOps] : queueFamilies) {
        auto setQueue = [&](int qfIdx, VkQueueFlags flag) {
            auto &qf = vkCtx->qf[qfIdx];
            if (!(static_cast<VkQueueFlags>(flags) & flag))
                return;
            // Only set if slot is empty, or current selection also has graphics
            // (prefer dedicated queues over shared graphics+X queues)
            if (qf.num == 0 || (qf.num > 0 && hasQueueFamilyGraphicsFlag(queueFamilies, qf.idx))) {
                qf.idx = static_cast<int>(familyIndex);
                qf.num = static_cast<int>(queueCount);
                qf.flags = static_cast<VkQueueFlagBits>(flag);
                qf.video_caps = static_cast<VkVideoCodecOperationFlagBitsKHR>(0);
                vkCtx->nb_qf = std::max(qfIdx + 1, vkCtx->nb_qf);
            }
        };

        setQueue(0, VK_QUEUE_GRAPHICS_BIT);
        setQueue(1, VK_QUEUE_TRANSFER_BIT);
        setQueue(2, VK_QUEUE_COMPUTE_BIT);
        if (hasVideoDecode)
            setQueue(3, VK_QUEUE_VIDEO_DECODE_BIT_KHR);
    }

    // Set video_caps for the decode queue if present
    if (hasVideoDecode && vkCtx->nb_qf > 3 && vkCtx->qf[3].num > 0) {
        for (const auto &[familyIndex, queueCount, flags, videoOps] : queueFamilies) {
            if (static_cast<uint32_t>(vkCtx->qf[3].idx) == familyIndex) {
                vkCtx->qf[3].video_caps = static_cast<VkVideoCodecOperationFlagBitsKHR>(static_cast<VkVideoCodecOperationFlagsKHR>(videoOps));
                break;
            }
        }
    }

    qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "========================================");
    qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "FFmpeg Vulkan Queue Configuration:");
    qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "----------------------------------------");
    qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "qf[0] Graphics  - idx:{}, num:{}", vkCtx->qf[0].idx, vkCtx->qf[0].num);
    qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "qf[1] Transfer  - idx:{}, num:{}", vkCtx->qf[1].idx, vkCtx->qf[1].num);
    qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "qf[2] Compute   - idx:{}, num:{}", vkCtx->qf[2].idx, vkCtx->qf[2].num);
    qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "qf[3] Decode    - idx:{}, num:{}", vkCtx->qf[3].idx, vkCtx->qf[3].num);
    qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "nb_qf:{}", vkCtx->nb_qf);
    qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "hasVideoDecode:{}", hasVideoDecode);
    qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "----------------------------------------");

    vkCtx->lock_queue = [](AVHWDeviceContext *, uint32_t queueFamilyIndex, uint32_t index) {
        if (auto *ctx = qzVulkanContext::instance())
            ctx->lockQueue(queueFamilyIndex, index);
    };
    vkCtx->unlock_queue = [](AVHWDeviceContext *, uint32_t queueFamilyIndex, uint32_t index) {
        if (auto *ctx = qzVulkanContext::instance())
            ctx->unlockQueue(queueFamilyIndex, index);
    };

    if (av_hwdevice_ctx_init(hwDeviceCtx) < 0) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to initialize Vulkan device context");
        av_buffer_unref(&hwDeviceCtx);
        return {};
    }

    qz::Log::cat_debug(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Successfully created Vulkan device context from qzVulkanContext singleton");
    return AVBufferUPtr(hwDeviceCtx);
}

namespace {

struct VulkanHwPoolCopyContext
{
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    uint32_t queueFamilyIndex = 0;
    VkQueue queue = VK_NULL_HANDLE;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;

    VkImage dstImage = VK_NULL_HANDLE;
    VkDeviceMemory dstMemory = VK_NULL_HANDLE;
    int dstWidth = 0;
    int dstHeight = 0;
    VkFormat dstFormat = VK_FORMAT_UNDEFINED;
    bool dstImageNeedsInit = true;

    bool ensureCommandResources(VkDevice dev, VkPhysicalDevice physDev, uint32_t qfIdx)
    {
        if (device != dev || queueFamilyIndex != qfIdx) {
            // The previous device may have already been destroyed (e.g. when
            // switching decoders). Do NOT attempt to destroy Vulkan objects on
            // the old device — vkDestroyFence/vkDestroyCommandPool would crash
            // with an invalid device handle. The old device's resources are
            // implicitly freed when the VkDevice is destroyed.
            resetHandles();
            device = dev;
            physicalDevice = physDev;
            queueFamilyIndex = qfIdx;
            {
                const auto &dld = VkDispatch::dld();
                queue = static_cast<VkQueue>(
                    vk::Device(device).getQueue(queueFamilyIndex, 0, dld));
            }
        }

        if (commandPool == VK_NULL_HANDLE) {
            const auto &dld = VkDispatch::dld();
            auto vkDevice = vk::Device(device);

            vk::CommandPoolCreateInfo poolInfo(
                vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                queueFamilyIndex);

            try {
                commandPool = static_cast<VkCommandPool>(vkDevice.createCommandPool(poolInfo, nullptr, dld));
            } catch (const vk::SystemError &) {
                return false;
            }
        }

        if (commandBuffer == VK_NULL_HANDLE) {
            const auto &dld = VkDispatch::dld();
            auto vkDevice = vk::Device(device);

            vk::CommandBufferAllocateInfo cmdInfo(
                vk::CommandPool(commandPool),
                vk::CommandBufferLevel::ePrimary,
                1);

            try {
                auto cmdBuffers = vkDevice.allocateCommandBuffers(cmdInfo, dld);
                commandBuffer = static_cast<VkCommandBuffer>(cmdBuffers[0]);
            } catch (const vk::SystemError &) {
                return false;
            }
        }

        if (fence == VK_NULL_HANDLE) {
            const auto &dld = VkDispatch::dld();
            auto vkDevice = vk::Device(device);

            vk::FenceCreateInfo fenceInfo;
            try {
                fence = static_cast<VkFence>(vkDevice.createFence(fenceInfo, nullptr, dld));
            } catch (const vk::SystemError &) {
                return false;
            }
        }

        return commandBuffer != VK_NULL_HANDLE && fence != VK_NULL_HANDLE;
    }

    bool ensureDstImage(int width, int height, VkFormat format)
    {
        if (dstImage != VK_NULL_HANDLE && dstWidth == width && dstHeight == height && dstFormat == format) {
            dstImageNeedsInit = false;
            return true;
        }

        if (dstImage != VK_NULL_HANDLE) {
            const auto &dld = VkDispatch::dld();
            auto vkDevice = vk::Device(device);
            vkDevice.destroyImage(vk::Image(dstImage), nullptr, dld);
            dstImage = VK_NULL_HANDLE;
        }
        if (dstMemory != VK_NULL_HANDLE) {
            const auto &dld = VkDispatch::dld();
            auto vkDevice = vk::Device(device);
            vkDevice.freeMemory(vk::DeviceMemory(dstMemory), nullptr, dld);
            dstMemory = VK_NULL_HANDLE;
        }

        VkFormat effectiveFormat = (format != VK_FORMAT_UNDEFINED) ? format : VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
        if (!vulkan_utils::createVulkanImage(device, physicalDevice,
                                               static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                                               effectiveFormat,
                                               VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                               nullptr, nullptr,
                                               dstImage, dstMemory)) {
            return false;
        }

        dstWidth = width;
        dstHeight = height;
        dstFormat = format;
        dstImageNeedsInit = true;
        return true;
    }

    void destroyAll()
    {
        const auto &dld = VkDispatch::dld();
        auto vkDevice = vk::Device(device);

        if (fence != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDevice.destroyFence(vk::Fence(fence), nullptr, dld);
            fence = VK_NULL_HANDLE;
        }
        if (commandPool != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            if (commandBuffer != VK_NULL_HANDLE) {
                vkDevice.freeCommandBuffers(vk::CommandPool(commandPool), vk::CommandBuffer(commandBuffer), dld);
                commandBuffer = VK_NULL_HANDLE;
            }
            vkDevice.destroyCommandPool(vk::CommandPool(commandPool), nullptr, dld);
            commandPool = VK_NULL_HANDLE;
        }
        if (dstImage != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDevice.destroyImage(vk::Image(dstImage), nullptr, dld);
            dstImage = VK_NULL_HANDLE;
        }
        if (dstMemory != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDevice.freeMemory(vk::DeviceMemory(dstMemory), nullptr, dld);
            dstMemory = VK_NULL_HANDLE;
        }
        device = VK_NULL_HANDLE;
        physicalDevice = VK_NULL_HANDLE;
        queue = VK_NULL_HANDLE;
        dstWidth = 0;
        dstHeight = 0;
        dstFormat = VK_FORMAT_UNDEFINED;
    }

    // Reset all handles to VK_NULL_HANDLE without calling Vulkan destroy functions.
    // Use this when the VkDevice may have already been destroyed (e.g. after a
    // decoder switch). The old device's child objects are implicitly freed when
    // the VkDevice is destroyed, so explicit cleanup is unnecessary and would
    // crash with an invalid device handle.
    void resetHandles()
    {
        fence = VK_NULL_HANDLE;
        commandBuffer = VK_NULL_HANDLE;
        commandPool = VK_NULL_HANDLE;
        dstImage = VK_NULL_HANDLE;
        dstMemory = VK_NULL_HANDLE;
        device = VK_NULL_HANDLE;
        physicalDevice = VK_NULL_HANDLE;
        queue = VK_NULL_HANDLE;
        dstWidth = 0;
        dstHeight = 0;
        dstFormat = VK_FORMAT_UNDEFINED;
    }
};

thread_local VulkanHwPoolCopyContext *tlCopyCtx = nullptr;

void destroyThreadLocalCopyContext()
{
    if (tlCopyCtx) {
        tlCopyCtx->destroyAll();
        delete tlCopyCtx;
        tlCopyCtx = nullptr;
    }
}

VulkanHwPoolCopyContext *getCopyContext()
{
    if (!tlCopyCtx) {
        tlCopyCtx = new VulkanHwPoolCopyContext();
    }
    return tlCopyCtx;
}

}

AVFrameUPtr copyFromHwPoolVulkan(AVFrameUPtr src)
{
    if (!src || !src->hw_frames_ctx || src->format != AV_PIX_FMT_VULKAN) {
        return src;
    }

    const AVHWDeviceContext *avDevCtx = avFrameDeviceContext(src.get());
    if (!avDevCtx || avDevCtx->type != AV_HWDEVICE_TYPE_VULKAN) {
        return src;
    }

    const AVVulkanDeviceContext *vkDevCtx = vulkan_utils::getVulkanDeviceContext(avDevCtx);
    if (!vkDevCtx) {
        return src;
    }

    // Check if we can use zero-copy path - if the frame's device matches the RHI device
    // and the frame supports sampling, we can pass it through directly
    auto *vkContext = qzVulkanContext::instance();
    if (vkContext && vkContext->isInitialized()) {
        if (vkDevCtx->act_dev == static_cast<VkDevice>(vkContext->device())) {
            const AVHWFramesContext *framesCtx =
                reinterpret_cast<const AVHWFramesContext *>(src->hw_frames_ctx->data);
            const AVVulkanFramesContext *vkFramesCtx = static_cast<const AVVulkanFramesContext *>(framesCtx->hwctx);
            if (vkFramesCtx->usage & VK_IMAGE_USAGE_SAMPLED_BIT) {
                return src;
            }
        }
    }

    AVVkFrame *srcVkFrame = vulkan_utils::getAVVkFrame(src.get());
    if (!srcVkFrame) {
        return src;
    }

    const AVHWFramesContext *framesCtx =
        reinterpret_cast<const AVHWFramesContext *>(src->hw_frames_ctx->data);
    const AVVulkanFramesContext *vkFramesCtx = static_cast<const AVVulkanFramesContext *>(framesCtx->hwctx);

    VkDevice device = vkDevCtx->act_dev;
    VkImage srcImage = srcVkFrame->img[0];

    if (vkFramesCtx->lock_frame)
        vkFramesCtx->lock_frame(const_cast<AVHWFramesContext *>(framesCtx), srcVkFrame);

    QScopeGuard autoUnlockSrc([&] {
        if (vkFramesCtx->unlock_frame)
            vkFramesCtx->unlock_frame(const_cast<AVHWFramesContext *>(framesCtx), srcVkFrame);
    });

    VkFormat srcFormat = vulkan_utils::avPixelFormatToVulkanFormat(
        static_cast<AVPixelFormat>(framesCtx->sw_format));

    VulkanHwPoolCopyContext *ctx = getCopyContext();
    if (!ctx->ensureCommandResources(device, vkDevCtx->phys_dev, vkDevCtx->qf[0].idx)) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to ensure command resources for HW pool copy");
        return src;
    }

    if (!ctx->ensureDstImage(src->width, src->height, srcFormat)) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to ensure destination image for HW pool copy");
        return src;
    }

    VkCommandBuffer cmdBuf = ctx->commandBuffer;

    const auto &dld = VkDispatch::dld();
    auto vkCmdBuf = vk::CommandBuffer(cmdBuf);
    auto vkDevice = vk::Device(device);

    try {
        vkCmdBuf.reset(vk::CommandBufferResetFlags(), dld);
    } catch (const vk::SystemError &) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan,
            "HwPoolCopy: failed to reset command buffer, returning source frame");
        return src;
    }

    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    vkCmdBuf.begin(beginInfo, dld);

    vulkan_utils::transitionImageLayoutCmd(cmdBuf, srcImage,
                                            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                                            VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                            VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);

    {
        VkAccessFlags dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
        VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkAccessFlags srcAccess = 0;

        if (!ctx->dstImageNeedsInit) {
            oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            srcAccess = VK_ACCESS_SHADER_READ_BIT;
        }

        vulkan_utils::transitionImageLayoutCmd(cmdBuf, ctx->dstImage,
                                                oldLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                srcStage, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                srcAccess, dstAccess);
    }

    // Multi-planar formats MUST use per-plane aspects (ePlane0, ePlane1, etc.)
    // for vkCmdCopyImage. Using eColor on a multi-planar image only copies
    // plane 0 (Y), losing UV data and causing color loss.
    const vk::Format vkFormat = static_cast<vk::Format>(srcFormat);
    const int planeCount = VkFormatUtils::getPlaneCount(vkFormat);
    const bool isMultiPlanar = (planeCount > 1);

    for (int plane = 0; plane < planeCount; ++plane) {
        vk::ImageAspectFlagBits aspect = isMultiPlanar
            ? VkFormatUtils::getPlaneAspect(plane)
            : vk::ImageAspectFlagBits::eColor;

        int planeWidth = src->width;
        int planeHeight = src->height;
        if (plane > 0) {
            planeWidth = (planeWidth + 1) / 2;
            planeHeight = (planeHeight + 1) / 2;
        }

        vk::ImageCopy copyRegion(
            vk::ImageSubresourceLayers(aspect, 0, 0, 1),
            vk::Offset3D(0, 0, 0),
            vk::ImageSubresourceLayers(aspect, 0, 0, 1),
            vk::Offset3D(0, 0, 0),
            vk::Extent3D(static_cast<uint32_t>(planeWidth), static_cast<uint32_t>(planeHeight), 1));

        vkCmdBuf.copyImage(
            vk::Image(srcImage), vk::ImageLayout::eGeneral,
            vk::Image(ctx->dstImage), vk::ImageLayout::eTransferDstOptimal,
            1, &copyRegion, dld);
    }

    vulkan_utils::transitionImageLayoutCmd(cmdBuf, ctx->dstImage,
                                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                                            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

    vkCmdBuf.end(dld);

    vkDevice.resetFences(vk::Fence(ctx->fence), dld);

    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &vkCmdBuf;
    vk::Queue(ctx->queue).submit(submitInfo, vk::Fence(ctx->fence), dld);

    constexpr uint64_t kWaitTimeoutNs = 100'000'000;
    vk::Result waitResult = vkDevice.waitForFences(vk::Fence(ctx->fence), VK_TRUE, kWaitTimeoutNs, dld);
    if (waitResult != vk::Result::eSuccess) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "HW pool copy: fence wait timed out, falling back to source frame");
        return src;
    }

    ctx->dstImageNeedsInit = false;

    AVFrameUPtr dest = makeAVFrame();
    if (av_frame_copy_props(dest.get(), src.get()) != 0) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Unable to copy frame props from decoder pool");
        return src;
    }

    auto *destVkFrame = reinterpret_cast<AVVkFrame *>(av_mallocz(sizeof(AVVkFrame)));
    if (!destVkFrame) {
        return src;
    }

    destVkFrame->img[0] = ctx->dstImage;
    destVkFrame->layout[0] = VK_IMAGE_LAYOUT_GENERAL;
    destVkFrame->access[0] = VK_ACCESS_SHADER_READ_BIT;

    dest->data[0] = reinterpret_cast<uint8_t *>(destVkFrame);
    dest->width = src->width;
    dest->height = src->height;
    dest->format = src->format;
    dest->hw_frames_ctx = av_buffer_ref(src->hw_frames_ctx);

    dest->buf[0] = av_buffer_create(
        reinterpret_cast<uint8_t *>(destVkFrame),
        sizeof(AVVkFrame),
        [](void *opaque, uint8_t *data) {
            Q_UNUSED(opaque);
            auto *frame = reinterpret_cast<AVVkFrame *>(data);
            frame->img[0] = VK_NULL_HANDLE;
            av_free(data);
        },
        src->hw_frames_ctx->data, 0
    );

    return dest;
}

}

QT_END_NAMESPACE

#endif
