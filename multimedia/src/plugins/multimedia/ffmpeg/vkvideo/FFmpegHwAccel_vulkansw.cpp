#include "FFmpegHwAccel_vulkansw_p.h"

#if QT_FFMPEG_HAS_VULKAN

#include "FFmpegHwAccel_vulkan_p.h"
#include "FFmpegHwAccel_vulkan_utils_p.h"
#include "private/VkDeviceContext_p.h"
#include "private/VkCommandContext_p.h"
#include "private/VkImage_p.h"
#include "private/VkMemory_p.h"
#include "private/VkFormatUtils_p.h"
#include "private/VkDispatch_p.h"
#include <private/VideoTextureHelper_p.h>
#include <rhi/qrhi.h>
import qzLog;
#include <QtCore/qmutex.h>
#include <array>

extern "C" {
#include <libavutil/pixdesc.h>
}

QT_BEGIN_NAMESPACE

namespace {

qz::Log::LogCategory qLcVulkanSWZeroCopy("qz.multimedia.vulkan.swzerocopy");

struct PlaneFormatInfo
{
    VkFormat format;
    int planeCount;
    int bytesPerPixel[3];
};

PlaneFormatInfo getPlaneFormatInfo(AVPixelFormat format)
{
    PlaneFormatInfo result{};

    switch (format) {
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
        result.format = VK_FORMAT_R8_UNORM;
        result.planeCount = 3;
        result.bytesPerPixel[0] = 1;
        result.bytesPerPixel[1] = 1;
        result.bytesPerPixel[2] = 1;
        break;
    case AV_PIX_FMT_YUV420P10:
    case AV_PIX_FMT_YUV420P12:
    case AV_PIX_FMT_YUV420P16:
        result.format = VK_FORMAT_R16_UNORM;
        result.planeCount = 3;
        result.bytesPerPixel[0] = 2;
        result.bytesPerPixel[1] = 2;
        result.bytesPerPixel[2] = 2;
        break;
    case AV_PIX_FMT_NV12:
        result.format = VK_FORMAT_R8_UNORM;
        result.planeCount = 2;
        result.bytesPerPixel[0] = 1;
        result.bytesPerPixel[1] = 2;
        break;
    case AV_PIX_FMT_P010:
    case AV_PIX_FMT_P016:
        result.format = VK_FORMAT_R16_UNORM;
        result.planeCount = 2;
        result.bytesPerPixel[0] = 2;
        result.bytesPerPixel[1] = 4;
        break;
    case AV_PIX_FMT_BGRA:
        result.format = VK_FORMAT_B8G8R8A8_UNORM;
        result.planeCount = 1;
        result.bytesPerPixel[0] = 4;
        break;
    case AV_PIX_FMT_RGBA:
        result.format = VK_FORMAT_R8G8B8A8_UNORM;
        result.planeCount = 1;
        result.bytesPerPixel[0] = 4;
        break;
    default:
        result.format = VK_FORMAT_UNDEFINED;
        break;
    }

    return result;
}

struct CodecContextSWData
{
    QVulkanInstance *vkInstance = nullptr;
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    uint32_t queueFamilyIndex = 0;
    std::shared_ptr<ffmpeg::VulkanSWImagePool> imagePool;
    std::vector<std::shared_ptr<ffmpeg::VulkanSWImagePool::LinearImage>> framePool;
    int maxPoolSize = 4;
};

}

namespace ffmpeg {

VulkanSWImagePool::VulkanSWImagePool(QVulkanInstance *vkInstance, VkDevice device,
                                     VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex)
    : m_vkInstance(vkInstance)
    , m_device(device)
    , m_physicalDevice(physicalDevice)
    , m_queueFamilyIndex(queueFamilyIndex)
{
    if (!m_vkInstance || m_device == VK_NULL_HANDLE || m_physicalDevice == VK_NULL_HANDLE) {
        qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Invalid Vulkan parameters for SW image pool");
        return;
    }

    const auto &dld = VkDispatch::dld();
    auto physDev = vk::PhysicalDevice(m_physicalDevice);

    uint32_t typeIndex = 0;
    m_hostMemorySupported = (VkMemory::findHostVisibleMemoryType(
        physDev, 0, vk::MemoryPropertyFlagBits::eHostCached, dld) != UINT32_MAX);

    if (!m_hostMemorySupported) {
        m_hostMemorySupported = (VkMemory::findHostVisibleMemoryType(
            physDev, 0, {}, dld) != UINT32_MAX);
    }

    if (!m_hostMemorySupported) {
        qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Host visible memory not supported, zero-copy disabled");
        return;
    }

    m_valid = true;
    qz::Log::cat_debug(qLcVulkanSWZeroCopy, "VulkanSWImagePool created successfully, host memory supported: {}", m_hostMemorySupported);
}

VulkanSWImagePool::~VulkanSWImagePool()
{
    for (auto &weakImg : m_imagePool) {
        if (auto img = weakImg.lock()) {
            destroyLinearImage(*img);
        }
    }
}

bool VulkanSWImagePool::createLinearImage(const QSize &size, VkFormat format, int planeCount, LinearImage &image)
{
    const auto &dld = VkDispatch::dld();
    auto vkDevice = vk::Device(m_device);
    auto physDev = vk::PhysicalDevice(m_physicalDevice);

    auto result = VkImageWrapper::createLinear(
        vkDevice, physDev, size, static_cast<vk::Format>(format),
        planeCount, m_queueFamilyIndex, dld);

    if (!result) {
        qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Failed to create linear image");
        return false;
    }

    image.image = static_cast<VkImage>(result->image);
    image.memory = static_cast<VkDeviceMemory>(result->memory);
    image.size = static_cast<VkDeviceSize>(result->size);
    image.mappedData = result->mappedData;
    image.mappedSize = static_cast<VkDeviceSize>(result->mappedSize);
    image.size_ = result->size_;
    image.format = static_cast<VkFormat>(result->format);
    image.planeCount = result->planeCount;
    for (int i = 0; i < 3; ++i) {
        image.planeOffsets[i] = static_cast<VkDeviceSize>(result->planeOffsets[i]);
        image.planeSizes[i] = static_cast<VkDeviceSize>(result->planeSizes[i]);
        image.planeRowPitches[i] = static_cast<VkDeviceSize>(result->planeRowPitches[i]);
    }
    image.codecContextData = result->codecContextData;

    return true;
}

void VulkanSWImagePool::destroyLinearImage(LinearImage &image)
{
    if (!m_vkInstance || m_device == VK_NULL_HANDLE)
        return;

    const auto &dld = VkDispatch::dld();
    auto vkDevice = vk::Device(m_device);

    VkImageWrapper::LinearImage vkImage;
    vkImage.image = vk::Image(image.image);
    vkImage.memory = vk::DeviceMemory(image.memory);
    vkImage.mappedData = image.mappedData;

    VkImageWrapper::destroyLinear(vkImage, vkDevice, dld);

    image.image = VK_NULL_HANDLE;
    image.memory = VK_NULL_HANDLE;
    image.mappedData = nullptr;
}

std::shared_ptr<VulkanSWImagePool::LinearImage> VulkanSWImagePool::acquireImage(const QSize &size, VkFormat format, int planeCount)
{
    for (auto it = m_imagePool.begin(); it != m_imagePool.end(); ++it) {
        auto img = it->lock();
        if (img && img->size_ == size && img->format == format && img->planeCount == planeCount) {
            m_imagePool.erase(it);
            return img;
        }
    }

    auto image = std::make_shared<LinearImage>();
    if (createLinearImage(size, format, planeCount, *image)) {
        return image;
    }

    return nullptr;
}

void VulkanSWImagePool::releaseImage(std::shared_ptr<LinearImage> image)
{
    if (!image)
        return;

    if (static_cast<int>(m_imagePool.size()) >= m_maxPoolSize) {
        for (auto it = m_imagePool.begin(); it != m_imagePool.end(); ++it) {
            if (it->expired()) {
                m_imagePool.erase(it);
                break;
            }
        }
    }

    if (static_cast<int>(m_imagePool.size()) < m_maxPoolSize) {
        m_imagePool.push_back(image);
    } else {
        destroyLinearImage(*image);
    }
}

VulkanSWTextureConverter::VulkanSWTextureConverter(QRhi *rhi)
    : TextureConverterBackend(rhi)
{
    if (!rhi || rhi->backend() != QRhi::Vulkan) {
        qz::Log::cat_warn(qLcVulkanSWZeroCopy, "RHI is not Vulkan backend");
        return;
    }

    const QRhiVulkanNativeHandles *nativeHandles =
        static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles());
    if (!nativeHandles || !nativeHandles->inst) {
        qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Failed to get Vulkan native handles from RHI");
        return;
    }

    m_vkInstance = nativeHandles->inst;
    m_physicalDevice = nativeHandles->physDev;
    m_device = nativeHandles->dev;
    m_graphicsQueue = nativeHandles->gfxQueue;
    m_graphicsQueueFamilyIndex = nativeHandles->gfxQueueFamilyIdx;

    if (m_physicalDevice == VK_NULL_HANDLE || m_device == VK_NULL_HANDLE ||
        m_graphicsQueue == VK_NULL_HANDLE) {
        qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Invalid Vulkan handles from RHI");
        return;
    }

    m_deviceContext = VkDeviceContext::fromQRhi(rhi);
    if (!m_deviceContext) {
        qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Failed to create VkDeviceContext");
        return;
    }

    const auto &dld = m_deviceContext->dld();
    auto vkDevice = m_deviceContext->device();

    vk::CommandPoolCreateInfo poolInfo(
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        m_graphicsQueueFamilyIndex);

    try {
        m_commandPool = static_cast<VkCommandPool>(vkDevice.createCommandPool(poolInfo, nullptr, dld));
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Failed to create command pool: {}", e.what());
        return;
    }

    vk::CommandBufferAllocateInfo cmdInfo(
        vk::CommandPool(m_commandPool),
        vk::CommandBufferLevel::ePrimary,
        1);

    try {
        auto cmdBuffers = vkDevice.allocateCommandBuffers(cmdInfo, dld);
        m_commandBuffer = static_cast<VkCommandBuffer>(cmdBuffers[0]);
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Failed to allocate command buffer: {}", e.what());
        return;
    }

    vk::SemaphoreTypeCreateInfo semaphoreType(vk::SemaphoreType::eTimeline, 0);
    vk::SemaphoreCreateInfo semaphoreInfo({}, &semaphoreType);
    try {
        m_timelineSemaphore = static_cast<VkSemaphore>(vkDevice.createSemaphore(semaphoreInfo, nullptr, dld));
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Failed to create timeline semaphore: {}", e.what());
        return;
    }

    m_valid = true;
    qz::Log::cat_debug(qLcVulkanSWZeroCopy, "VulkanSWTextureConverter initialized successfully");
}

VulkanSWTextureConverter::~VulkanSWTextureConverter()
{
    if (m_device != VK_NULL_HANDLE && m_deviceContext) {
        const auto &dld = m_deviceContext->dld();
        auto vkDevice = vk::Device(m_device);

        if (m_ycbcrConversion != VK_NULL_HANDLE) {
            vkDevice.destroySamplerYcbcrConversionKHR(
                vk::SamplerYcbcrConversion(m_ycbcrConversion), nullptr, dld);
            m_ycbcrConversion = VK_NULL_HANDLE;
        }
        if (m_timelineSemaphore != VK_NULL_HANDLE) {
            vkDevice.destroySemaphore(vk::Semaphore(m_timelineSemaphore), nullptr, dld);
            m_timelineSemaphore = VK_NULL_HANDLE;
        }
        if (m_cachedImageView != VK_NULL_HANDLE) {
            vkDevice.destroyImageView(vk::ImageView(m_cachedImageView), nullptr, dld);
            m_cachedImageView = VK_NULL_HANDLE;
            m_cachedImageViewForImage = VK_NULL_HANDLE;
        }
        if (m_commandPool != VK_NULL_HANDLE) {
            vkDevice.destroyCommandPool(vk::CommandPool(m_commandPool), nullptr, dld);
            m_commandPool = VK_NULL_HANDLE;
        }
    }
}

bool VulkanSWTextureConverter::isYCbCrFormat(VkFormat format) const
{
    return vulkan_utils::isYCbCrFormat(format);
}

bool VulkanSWTextureConverter::createYCbCrConversion(VkFormat format, int colorSpace, int colorRange)
{
    if (m_ycbcrConversion != VK_NULL_HANDLE)
        return true;

    if (!m_deviceContext)
        return false;

    const auto &dld = m_deviceContext->dld();
    auto result = VkImageWrapper::createYCbCrConversion(
        vk::Device(m_device), static_cast<vk::Format>(format), dld, colorSpace, colorRange);
    if (!result) {
        qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Failed to create YCbCr conversion");
        return false;
    }
    m_ycbcrConversion = static_cast<VkSamplerYcbcrConversion>(result);
    return true;
}

class VulkanSWTextureHandles : public VideoFrameTexturesHandles
{
public:
    VulkanSWTextureHandles(TextureConverterBackendPtr &&converterBackend, QRhi *rhi,
                           std::shared_ptr<VulkanSWImagePool::LinearImage> linearImage,
                           VkImageView imageView,
                           const QSize &size)
        : m_parentConverterBackend(std::move(converterBackend))
        , m_owner(rhi)
        , m_linearImage(std::move(linearImage))
        , m_imageView(imageView)
        , m_size(size)
    {
    }

    ~VulkanSWTextureHandles() override
    {
        auto converter = std::dynamic_pointer_cast<VulkanSWTextureConverter>(m_parentConverterBackend);
        if (!converter || !converter->isValid()) {
            return;
        }

        if (m_imageView != VK_NULL_HANDLE) {
            if (converter->m_cachedImageView == VK_NULL_HANDLE) {
                converter->m_cachedImageView = m_imageView;
                converter->m_cachedImageViewForImage = m_linearImage->image;
                m_imageView = VK_NULL_HANDLE;
            } else {
                const auto &dld = VkDispatch::dld();
                const QRhiVulkanNativeHandles *nativeHandles =
                    static_cast<const QRhiVulkanNativeHandles *>(converter->rhi->nativeHandles());
                if (nativeHandles) {
                    vk::Device(nativeHandles->dev).destroyImageView(vk::ImageView(m_imageView), nullptr, dld);
                }
            }
        }
    }

    quint64 textureHandle(QRhi &rhi, int plane) override
    {
        if (&rhi != m_owner)
            return 0;

        Q_UNUSED(plane);
        return reinterpret_cast<quint64>(m_linearImage->image);
    }

private:
    TextureConverterBackendPtr m_parentConverterBackend;
    QRhi *m_owner = nullptr;
    std::shared_ptr<VulkanSWImagePool::LinearImage> m_linearImage;
    VkImageView m_imageView = VK_NULL_HANDLE;
    QSize m_size;
};

VideoFrameTexturesHandlesUPtr
VulkanSWTextureConverter::createTextureHandles(AVFrame *frame, VideoFrameTexturesHandlesUPtr )
{
    if (!m_valid || !frame) {
        return nullptr;
    }

    if (!isVulkanSWZeroCopyFrame(frame)) {
        return nullptr;
    }

    auto *linearImagePtr = reinterpret_cast<VulkanSWImagePool::LinearImage*>(frame->data[0]);
    if (!linearImagePtr || linearImagePtr->image == VK_NULL_HANDLE) {
        return nullptr;
    }

    auto *sharedPtrStorage = reinterpret_cast<std::shared_ptr<VulkanSWImagePool::LinearImage>*>(av_buffer_get_opaque(frame->buf[0]));
    if (!sharedPtrStorage) {
        qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Failed to get shared_ptr from AVBuffer");
        return nullptr;
    }

    std::shared_ptr<VulkanSWImagePool::LinearImage> linearImage = *sharedPtrStorage;

    const int width = frame->width;
    const int height = frame->height;
    vk::Format vulkanFormat = VkFormatUtils::avPixelFormatToVulkanSW(static_cast<AVPixelFormat>(frame->format));

    if (vulkanFormat == vk::Format::eUndefined) {
        qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Unsupported pixel format");
        return nullptr;
    }

    const auto &dld = m_deviceContext->dld();
    auto vkDevice = vk::Device(m_device);

    vk::SamplerYcbcrConversion ycbcrConv;
    if (VkFormatUtils::isYCbCrFormat(vulkanFormat)) {
        if (!createYCbCrConversion(static_cast<VkFormat>(vulkanFormat), frame->colorspace, frame->color_range)) {
            qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Failed to create YCbCr conversion for format {}", static_cast<int>(static_cast<VkFormat>(vulkanFormat)));
            return nullptr;
        }
        ycbcrConv = vk::SamplerYcbcrConversion(m_ycbcrConversion);
    }

    VkImageView imageView = VK_NULL_HANDLE;
    if (m_cachedImageView != VK_NULL_HANDLE && m_cachedImageViewForImage == linearImage->image) {
        imageView = m_cachedImageView;
        m_cachedImageView = VK_NULL_HANDLE;
        m_cachedImageViewForImage = VK_NULL_HANDLE;
    } else {
        if (m_cachedImageView != VK_NULL_HANDLE) {
            vkDevice.destroyImageView(vk::ImageView(m_cachedImageView), nullptr, dld);
            m_cachedImageView = VK_NULL_HANDLE;
            m_cachedImageViewForImage = VK_NULL_HANDLE;
        }

        auto viewResult = VkImageWrapper::createImageView(
            vkDevice, vk::Image(linearImage->image), vulkanFormat, ycbcrConv, dld);
        if (!viewResult) {
            qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Failed to create image view");
            return nullptr;
        }
        imageView = static_cast<VkImageView>(viewResult);
    }

    return std::make_unique<VulkanSWTextureHandles>(
        shared_from_this(), rhi,
        linearImage, imageView,
        QSize(width, height));
}

bool VulkanSWTextureConverter::createOutputImage(int width, int height, VkFormat format,
                                                  VkImage &image, VkDeviceMemory &memory, VkImageView &imageView)
{
    const auto &dld = m_deviceContext->dld();
    auto vkDevice = vk::Device(m_device);
    auto physDev = vk::PhysicalDevice(m_physicalDevice);

    vk::DeviceMemory vkMemory = nullptr;
    auto imgResult = VkImageWrapper::createOptimal(
        vkDevice, physDev,
        static_cast<uint32_t>(width), static_cast<uint32_t>(height),
        static_cast<vk::Format>(format),
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        nullptr, nullptr, vkMemory, dld);

    if (!imgResult) {
        qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Failed to create image");
        return false;
    }

    image = static_cast<VkImage>(imgResult);
    memory = static_cast<VkDeviceMemory>(vkMemory);

    vk::SamplerYcbcrConversion ycbcrConv;
    if (isYCbCrFormat(format)) {
        if (!createYCbCrConversion(format)) {
            qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Failed to create YCbCr conversion for format {}", static_cast<int>(format));
            vkDevice.destroyImage(imgResult, nullptr, dld);
            vkDevice.freeMemory(vkMemory, nullptr, dld);
            return false;
        }
        ycbcrConv = vk::SamplerYcbcrConversion(m_ycbcrConversion);
    }

    auto viewResult = VkImageWrapper::createImageView(
        vkDevice, imgResult, static_cast<vk::Format>(format), ycbcrConv, dld);
    if (!viewResult) {
        qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Failed to create image view");
        vkDevice.freeMemory(vkMemory, nullptr, dld);
        vkDevice.destroyImage(imgResult, nullptr, dld);
        return false;
    }

    imageView = static_cast<VkImageView>(viewResult);
    return true;
}

bool VulkanSWTextureConverter::copyLinearToOptimal(VkImage srcImage, VkImage dstImage,
                                                     int width, int height, int )
{
    const auto &dld = m_deviceContext->dld();
    auto vkDevice = vk::Device(m_device);
    auto cmdBuf = vk::CommandBuffer(m_commandBuffer);

    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    try {
        cmdBuf.begin(beginInfo, dld);
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Failed to begin command buffer: {}", e.what());
        return false;
    }

    std::array<vk::ImageMemoryBarrier, 2> initialBarriers = {
        vk::ImageMemoryBarrier(
            vk::AccessFlagBits::eHostWrite, vk::AccessFlagBits::eTransferRead,
            vk::ImageLayout::ePreinitialized, vk::ImageLayout::eTransferSrcOptimal,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
            vk::Image(srcImage),
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)),
        vk::ImageMemoryBarrier(
            {}, vk::AccessFlagBits::eTransferWrite,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
            vk::Image(dstImage),
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1))
    };

    cmdBuf.pipelineBarrier(
        vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eTransfer,
        {}, {}, {}, initialBarriers, dld);

    vk::ImageCopy copyRegion(
        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
        vk::Offset3D(0, 0, 0),
        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
        vk::Offset3D(0, 0, 0),
        vk::Extent3D(static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1));

    cmdBuf.copyImage(
        vk::Image(srcImage), vk::ImageLayout::eTransferSrcOptimal,
        vk::Image(dstImage), vk::ImageLayout::eTransferDstOptimal,
        1, &copyRegion, dld);

    vk::ImageMemoryBarrier dstBarrier2(
        vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead,
        vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
        vk::Image(dstImage),
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

    cmdBuf.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
        {}, {}, {}, dstBarrier2, dld);

    cmdBuf.end(dld);

    return submitCommandBuffer();
}

bool VulkanSWTextureConverter::submitCommandBuffer()
{
    const auto &dld = m_deviceContext->dld();
    auto vkDevice = vk::Device(m_device);

    ++m_timelineValue;
    uint64_t signalValue = m_timelineValue;

    vk::TimelineSemaphoreSubmitInfo timelineInfo;
    timelineInfo.signalSemaphoreValueCount = 1;
    timelineInfo.pSignalSemaphoreValues = &signalValue;

    vk::SubmitInfo submitInfo;
    submitInfo.pNext = &timelineInfo;
    submitInfo.signalSemaphoreCount = 1;
    vk::Semaphore timelineSemaphoreVk(m_timelineSemaphore);
    submitInfo.pSignalSemaphores = &timelineSemaphoreVk;
    submitInfo.commandBufferCount = 1;
    auto cmdBuf = vk::CommandBuffer(m_commandBuffer);
    submitInfo.pCommandBuffers = &cmdBuf;

    try {
        vk::Queue(m_graphicsQueue).submit(submitInfo, {}, dld);
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Failed to submit command buffer: {}", e.what());
        return false;
    }

    VkSemaphoreWaitInfo waitInfo{};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &m_timelineSemaphore;
    waitInfo.pValues = &signalValue;
    try {
        (void)vkDevice.waitSemaphores(vk::SemaphoreWaitInfo(waitInfo), UINT64_MAX, dld);
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Failed to wait for timeline semaphore: {}", e.what());
        return false;
    }

    vk::CommandBuffer(m_commandBuffer).reset(vk::CommandBufferResetFlags(), dld);
    return true;
}

int vulkanSWGetBuffer2(AVCodecContext *codecCtx, AVFrame *frame, int )
{
    auto *data = static_cast<CodecContextSWData*>(codecCtx->opaque);
    if (!data || !data->imagePool || !data->imagePool->isValid()) {
        return avcodec_default_get_buffer2(codecCtx, frame, 0);
    }

    int linesizeAligns[AV_NUM_DATA_POINTERS] = {};
    avcodec_align_dimensions2(codecCtx, &frame->width, &frame->height, linesizeAligns);

    const int lineSizeAlign = linesizeAligns[0];
    int w = FFALIGN(frame->width, lineSizeAlign);
    int h = frame->height;

    uint32_t paddingHeight = h - codecCtx->height + 1;

    QSize imageSize(w, h + paddingHeight);
    PlaneFormatInfo planeInfo = getPlaneFormatInfo(codecCtx->pix_fmt);

    qz::Log::cat_debug(qLcVulkanSWZeroCopy, "Creating linear image: size={} pix_fmt={} ({}) planeCount={}", imageSize, static_cast<int>(codecCtx->pix_fmt), av_get_pix_fmt_name(codecCtx->pix_fmt), planeInfo.planeCount);

    if (planeInfo.format == VK_FORMAT_UNDEFINED) {
        qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Unsupported pixel format: {} ({})", static_cast<int>(codecCtx->pix_fmt), av_get_pix_fmt_name(codecCtx->pix_fmt));
        return avcodec_default_get_buffer2(codecCtx, frame, 0);
    }

    VkFormat vulkanFormat = static_cast<VkFormat>(VkFormatUtils::avPixelFormatToVulkanSW(codecCtx->pix_fmt));
    if (vulkanFormat == VK_FORMAT_UNDEFINED) {
        qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Cannot convert pixel format to Vulkan format");
        return avcodec_default_get_buffer2(codecCtx, frame, 0);
    }

    auto linearImage = data->imagePool->acquireImage(imageSize, vulkanFormat, planeInfo.planeCount);
    if (!linearImage) {
        qz::Log::cat_warn(qLcVulkanSWZeroCopy, "Failed to acquire linear image from pool");
        return avcodec_default_get_buffer2(codecCtx, frame, 0);
    }

    linearImage->codecContextData = data;

    frame->data[0] = reinterpret_cast<uint8_t*>(linearImage.get());
    frame->data[1] = nullptr;
    frame->data[2] = nullptr;

    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(codecCtx->pix_fmt);
    if (desc && linearImage->mappedData) {
        for (int plane = 0; plane < linearImage->planeCount && plane < AV_NUM_DATA_POINTERS; ++plane) {
            frame->data[plane] = linearImage->mappedData + linearImage->planeOffsets[plane];
            frame->linesize[plane] = static_cast<int>(linearImage->planeRowPitches[plane]);
        }
    }

    frame->buf[0] = av_buffer_create(
        reinterpret_cast<uint8_t*>(linearImage.get()),
        sizeof(void*),
        [](void *opaque, uint8_t *data) {
            Q_UNUSED(data);
            auto *imagePtr = static_cast<std::shared_ptr<VulkanSWImagePool::LinearImage>*>(opaque);
            auto image = *imagePtr;

            if (image->codecContextData) {
                auto *codecData = static_cast<CodecContextSWData*>(image->codecContextData);
                if (codecData && codecData->imagePool) {
                    codecData->imagePool->releaseImage(image);
                }
            }

            delete imagePtr;
        },
        new std::shared_ptr<VulkanSWImagePool::LinearImage>(linearImage),
        0
    );

    frame->extended_data = frame->data;

    return 0;
}

void setVulkanSWImagePoolForContext(AVCodecContext *codecCtx, std::shared_ptr<VulkanSWImagePool> pool)
{
    if (!codecCtx)
        return;

    auto *data = reinterpret_cast<CodecContextSWData*>(codecCtx->opaque);
    if (!data) {
        data = new CodecContextSWData();
        codecCtx->opaque = data;
    }

    data->imagePool = pool;
}

void initVulkanSWContext(AVCodecContext *codecCtx, QVulkanInstance *vkInstance,
                          VkDevice device, VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex)
{
    if (!codecCtx)
        return;

    auto *data = reinterpret_cast<CodecContextSWData*>(codecCtx->opaque);
    if (!data) {
        data = new CodecContextSWData();
        codecCtx->opaque = data;
    }

    data->vkInstance = vkInstance;
    data->device = device;
    data->physicalDevice = physicalDevice;
    data->queueFamilyIndex = queueFamilyIndex;

    if (!data->imagePool) {
        data->imagePool = std::make_shared<VulkanSWImagePool>(vkInstance, device, physicalDevice, queueFamilyIndex);
    }
}

std::shared_ptr<VulkanSWImagePool> getVulkanSWImagePoolForContext(AVCodecContext *codecCtx)
{
    if (!codecCtx)
        return nullptr;

    auto *data = reinterpret_cast<CodecContextSWData*>(codecCtx->opaque);
    if (!data)
        return nullptr;

    return data->imagePool;
}

bool isVulkanSWZeroCopyFrame(const AVFrame *frame)
{
    if (!frame || frame->format < 0)
        return false;

    return frame->buf[0] && frame->buf[0]->size == sizeof(void*);
}

}

QT_END_NAMESPACE

#endif
