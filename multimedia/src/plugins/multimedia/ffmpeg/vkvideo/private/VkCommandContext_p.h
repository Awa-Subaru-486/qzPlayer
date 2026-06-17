#ifndef VKCOMMANDCONTEXT_P_H
#define VKCOMMANDCONTEXT_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegDefs_p.h>

#if QT_FFMPEG_HAS_VULKAN

#include "VkDispatch_p.h"
#include <vulkan/vulkan.hpp>

QT_BEGIN_NAMESPACE

namespace ffmpeg {

// Vulkan 命令上下文，管理命令缓冲的录制和提交
class VkCommandContext
{
public:
    static std::unique_ptr<VkCommandContext> create(
        vk::Device device,
        uint32_t queueFamilyIndex,
        const vk::detail::DispatchLoaderDynamic &dld);

    ~VkCommandContext();

    vk::CommandBuffer commandBuffer() const { return m_commandBuffer; }
    vk::Device device() const { return m_device; }
    const vk::detail::DispatchLoaderDynamic &dld() const { return m_dld; }

    void begin();
    void end();
    void submitAndWait(vk::Queue queue);

    void transitionImageLayout(
        vk::Image image,
        vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
        vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage,
        vk::AccessFlags srcAccess, vk::AccessFlags dstAccess,
        vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor);

private:
    VkCommandContext(vk::Device device, const vk::detail::DispatchLoaderDynamic &dld);

    vk::Device m_device;
    const vk::detail::DispatchLoaderDynamic &m_dld;

    vk::UniqueCommandPool m_commandPool;
    vk::CommandBuffer m_commandBuffer;
    vk::UniqueFence m_fence;
};

} // namespace ffmpeg

QT_END_NAMESPACE

#endif
#endif
