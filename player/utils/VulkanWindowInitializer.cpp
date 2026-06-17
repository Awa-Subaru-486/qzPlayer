#include "VulkanWindowInitializer.hpp"

#include <QSGRendererInterface>
#include <vulkan/vulkan.hpp>
#include <QQmlEngine>

import qzVulkanContext;

namespace qz
{

auto debug_vkinfo(const qzVulkanContext* vkContext)
{
        qDebug() << "========================================";
        qDebug() << "Vulkan 设备初始化完成，队列族信息如下:";
        qDebug() << "========================================";
        qDebug() << "共管理" << vkContext->queueFamilies().size() << "个队列族";
        qDebug() << "----------------------------------------";

        for (const auto& family : vkContext->queueFamilies()) {
            QStringList flagNames;
            if (family.flags & vk::QueueFlagBits::eGraphics)    flagNames << "Graphics";
            if (family.flags & vk::QueueFlagBits::eCompute)     flagNames << "Compute";
            if (family.flags & vk::QueueFlagBits::eTransfer)    flagNames << "Transfer";
            if (family.flags & vk::QueueFlagBits::eSparseBinding) flagNames << "SparseBinding";
            if (family.flags & vk::QueueFlagBits::eProtected)   flagNames << "Protected";
    #if VK_HEADER_VERSION > 237
            if (family.flags & vk::QueueFlagBits::eVideoDecodeKHR) flagNames << "VideoDecode";
            if (family.flags & vk::QueueFlagBits::eVideoEncodeKHR) flagNames << "VideoEncode";
    #endif

            QStringList descriptions;
            if (family.flags & vk::QueueFlagBits::eGraphics)    descriptions << "图形渲染";
            if (family.flags & vk::QueueFlagBits::eCompute)     descriptions << "计算着色器";
            if (family.flags & vk::QueueFlagBits::eTransfer)    descriptions << "数据传输";
            if (family.flags & vk::QueueFlagBits::eSparseBinding) descriptions << "稀疏资源绑定";
            if (family.flags & vk::QueueFlagBits::eProtected)   descriptions << "受保护内容";
    #if VK_HEADER_VERSION > 237
            if (family.flags & vk::QueueFlagBits::eVideoDecodeKHR) descriptions << "视频解码";
            if (family.flags & vk::QueueFlagBits::eVideoEncodeKHR) descriptions << "视频编码";
    #endif

            uint32_t actualQueueCount = 0;
            for (const auto& queue : vkContext->queues()) {
                if (queue.familyIndex == family.familyIndex)
                    actualQueueCount++;
            }

            qDebug() << "【队列族" << family.familyIndex << "】";
            qDebug() << "  标志:" << flagNames.join(" | ");
            qDebug() << "  功能:" << descriptions.join("; ");
            qDebug() << "  队列数量:" << family.queueCount;
            qDebug() << "  实际获取队列数:" << actualQueueCount;
            qDebug() << "----------------------------------------";
        }
    }

VulkanWindowInitializer::VulkanWindowInitializer(QObject* parent)
    : QObject(parent)
{
}

VulkanWindowInitializer::~VulkanWindowInitializer() = default;

VulkanWindowInitializer* VulkanWindowInitializer::instance()
{
    static VulkanWindowInitializer s_instance;
    QQmlEngine::setObjectOwnership(&s_instance, QQmlEngine::CppOwnership);
    return &s_instance;
}

bool VulkanWindowInitializer::initialize(QQuickWindow* window)
{
    if (m_initialized) {
        return true;
    }

    if (!window) {
        return false;
    }

    if (QQuickWindow::graphicsApi() != QSGRendererInterface::Vulkan) {
        return false;
    }

    auto* vkContext = qzVulkanContext::instance();
    if (!vkContext->initialize()) {
        qWarning() << "Failed to initialize Vulkan context";
        return false;
    }

    auto m_qVkInstance = new QVulkanInstance();
    m_qVkInstance->setVkInstance(vkContext->vkInstance());
    if (!m_qVkInstance->create()) {
        qWarning() << "Failed to create QVulkanInstance";
        m_qVkInstance->destroy();
        delete m_qVkInstance;
        m_qVkInstance = nullptr;
        return false;
    }

    window->setVulkanInstance(m_qVkInstance);

    const auto physicalDevice = vkContext->physicalDevice();
    const auto device = vkContext->device();

    int queueFamilyIndex = -1;
    for (const auto& queueFamilies = vkContext->queueFamilies(); const auto& family : queueFamilies) {
        if (family.flags & vk::QueueFlagBits::eGraphics) {
            queueFamilyIndex = static_cast<int>(family.familyIndex);
            break;
        }
    }

    if (queueFamilyIndex < 0) {
        qWarning() << "Failed to find graphics queue family";
        return false;
    }

    constexpr int queueIndex = 0;
    window->setGraphicsDevice(QQuickGraphicsDevice::fromDeviceObjects(physicalDevice, device, queueFamilyIndex, queueIndex));

    // debug_vkinfo();
    m_initialized = true;
    return true;
}

bool VulkanWindowInitializer::isInitialized() const
{
    return m_initialized;
}

}
