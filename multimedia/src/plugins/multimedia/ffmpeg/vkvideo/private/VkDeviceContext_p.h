#ifndef VKDEVICECONTEXT_P_H
#define VKDEVICECONTEXT_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegDefs_p.h>

#if QT_FFMPEG_HAS_VULKAN

#include "VkDispatch_p.h"
#include <vulkan/vulkan.hpp>
#include <QVulkanInstance>
#include <memory>

QT_BEGIN_NAMESPACE

class QRhi;

namespace ffmpeg {

// Vulkan 设备上下文，封装实例、设备和队列
class VkDeviceContext
{
public:
    static std::shared_ptr<VkDeviceContext> fromQRhi(QRhi *rhi);
    static std::shared_ptr<VkDeviceContext> fromQVulkanInstance(
        QVulkanInstance *vkInstance, VkDevice device,
        VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex);
    static std::shared_ptr<VkDeviceContext> fromQzVulkanContext();

    [[nodiscard]] vk::Instance vkInstance() const { return m_instance; }
    [[nodiscard]] vk::PhysicalDevice physicalDevice() const { return m_physicalDevice; }
    [[nodiscard]] vk::Device device() const { return m_device; }
    [[nodiscard]] vk::Queue graphicsQueue() const { return m_graphicsQueue; }
    [[nodiscard]] uint32_t graphicsQueueFamilyIndex() const { return m_graphicsQueueFamilyIndex; }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic &dld() const { return m_dld; }

    [[nodiscard]] bool isValid() const { return m_valid; }

    [[nodiscard]] VkDevice vkDevice() const { return static_cast<VkDevice>(m_device); }
    [[nodiscard]] VkPhysicalDevice vkPhysicalDevice() const { return static_cast<VkPhysicalDevice>(m_physicalDevice); }

public:
    VkDeviceContext() = default;

    vk::Instance m_instance;
    vk::PhysicalDevice m_physicalDevice;
    vk::Device m_device;
    vk::Queue m_graphicsQueue;
    uint32_t m_graphicsQueueFamilyIndex = 0;
    vk::detail::DispatchLoaderDynamic m_dld;
    bool m_valid = false;
};

} // namespace ffmpeg

QT_END_NAMESPACE

#endif
#endif
