#include "../FFmpegHwAccel_vulkan_utils_p.h"

#if QT_FFMPEG_HAS_VULKAN

#include "VkCommandContext_p.h"

import qzLog;

QT_BEGIN_NAMESPACE

namespace ffmpeg {

VkCommandContext::VkCommandContext(vk::Device device, const vk::detail::DispatchLoaderDynamic &dld)
    : m_device(device)
    , m_dld(dld)
{
}

VkCommandContext::~VkCommandContext()
{
    if (m_commandBuffer && m_commandPool) {
        m_device.freeCommandBuffers(*m_commandPool, m_commandBuffer, m_dld);
        m_commandBuffer = nullptr;
    }
}

std::unique_ptr<VkCommandContext> VkCommandContext::create(
    vk::Device device,
    uint32_t queueFamilyIndex,
    const vk::detail::DispatchLoaderDynamic &dld)
{
    auto ctx = std::unique_ptr<VkCommandContext>(new VkCommandContext(device, dld));

    vk::CommandPoolCreateInfo poolInfo(
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        queueFamilyIndex);

    try {
        ctx->m_commandPool = device.createCommandPoolUnique(poolInfo, nullptr, dld);
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to create command pool: {}", e.what());
        return nullptr;
    }

    vk::CommandBufferAllocateInfo cmdInfo(
        *ctx->m_commandPool,
        vk::CommandBufferLevel::ePrimary,
        1);

    try {
        auto cmdBuffers = device.allocateCommandBuffers(cmdInfo, dld);
        ctx->m_commandBuffer = cmdBuffers[0];
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to allocate command buffer: {}", e.what());
        return nullptr;
    }

    vk::FenceCreateInfo fenceInfo;
    try {
        ctx->m_fence = device.createFenceUnique(fenceInfo, nullptr, dld);
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to create fence: {}", e.what());
        return nullptr;
    }

    return ctx;
}

void VkCommandContext::begin()
{
    constexpr vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    m_commandBuffer.begin(beginInfo, m_dld);
}

void VkCommandContext::end()
{
    m_commandBuffer.end(m_dld);
}

void VkCommandContext::submitAndWait(vk::Queue queue)
{
    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;

    try {
        queue.submit(submitInfo, *m_fence, m_dld);
        (void)m_device.waitForFences(*m_fence, VK_TRUE, UINT64_MAX, m_dld);
    } catch (const vk::SystemError &e) {
        qz::Log::cat_warn(vulkan_utils::qLcMediaFFmpegHWAccelVulkan, "Failed to submit/wait fence: {}", e.what());
    }

    m_device.resetFences(*m_fence, m_dld);
    m_commandBuffer.reset(vk::CommandBufferResetFlagBits::eReleaseResources, m_dld);
}

void VkCommandContext::transitionImageLayout(
    vk::Image image,
    vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
    vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage,
    vk::AccessFlags srcAccess, vk::AccessFlags dstAccess,
    vk::ImageAspectFlags aspectMask)
{
    const vk::ImageMemoryBarrier barrier(
        srcAccess, dstAccess,
        oldLayout, newLayout,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
        image,
        vk::ImageSubresourceRange(aspectMask, 0, 1, 0, 1));

    m_commandBuffer.pipelineBarrier(
        srcStage, dstStage,
        {},
        {}, {}, barrier, m_dld);
}

} // namespace ffmpeg

QT_END_NAMESPACE

#endif
