module;

#include "vulkancontext_platform.hpp"

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <map>
#include <cstdint>
#include <string>
#include <algorithm>
#include <ranges>
#include <memory>
#include <utility>
#include <cstring>

#include <QLibrary>

module qzVulkanContext;

import qzLog;

#ifdef QT_DEBUG
#   define USE_VALIDATION_LAYER 1
#else
#   define USE_VALIDATION_LAYER 0
#endif

#if USE_VALIDATION_LAYER
static VKAPI_ATTR vk::Bool32 VKAPI_CALL vulkanDebugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    vk::DebugUtilsMessageTypeFlagBitsEXT messageType,
    const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData)
{
    Q_UNUSED(messageType);
    Q_UNUSED(pUserData);

    const char* message = pCallbackData->pMessage;

    if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
        qz::Log::error("[Vulkan] {}", message);
    } else if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
        qz::Log::warn("[Vulkan] {}", message);
    } else if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo) {
        qz::Log::info("[Vulkan] {}", message);
    } else if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose) {
        qz::Log::debug("[Vulkan] {}", message);
    }
    return VK_FALSE;
}
#endif

qzVulkanContext* qzVulkanContext::instance()
{
    static qzVulkanContext s_instance;
    return &s_instance;
}

void qzVulkanContext::destroy()
{

}

qzVulkanContext::qzVulkanContext() = default;

qzVulkanContext::~qzVulkanContext()
{
    // if (m_device) { m_device = nullptr; }
    // if (m_instance) { m_instance = nullptr; }

    // m_queueLocks.clear();

    // if (m_device) {
    //     m_device.waitIdle();
    //     m_device.destroy();
    // }
    // if (m_instance) {
    //     m_instance.destroy();
    // }
}

PFN_vkGetInstanceProcAddr qzVulkanContext::loadVulkanLibrary(const std::string& vulkanLibrary)
{
    Q_UNUSED(vulkanLibrary);

#ifdef Q_OS_ANDROID
    QLibrary vulkanLib(QStringLiteral("vulkan"));
#else
    QLibrary vulkanLib(QStringLiteral("vulkan-1"));
#endif
    if (!vulkanLib.load()) {
        qz::Log::warn("Failed to load Vulkan library");
        return nullptr;
    }

    const auto vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        vulkanLib.resolve("vkGetInstanceProcAddr"));
    if (!vkGetInstanceProcAddr) {
        qz::Log::warn("Unable to get vkGetInstanceProcAddr");
        return nullptr;
    }

    return vkGetInstanceProcAddr;
}

void qzVulkanContext::initDispatchLoaderDynamic(PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr, vk::Instance instance)
{
    if (instance) {
        m_dld.init(instance, vkGetInstanceProcAddr);
    } else {
        m_dld.init(vkGetInstanceProcAddr);
    }
}

bool qzVulkanContext::initialize()
{
    if (m_initialized) {
        return true;
    }

    m_vkGetInstanceProcAddr = loadVulkanLibrary();
    if (!m_vkGetInstanceProcAddr) {
        qz::Log::warn("Failed to load Vulkan library");
        return false;
    }

    initDispatchLoaderDynamic(m_vkGetInstanceProcAddr);

    if (!createInstance()) {
        qz::Log::warn("Failed to create Vulkan instance");
        return false;
    }

    if (!selectPhysicalDevice()) {
        qz::Log::warn("Failed to select physical device");
        return false;
    }

    if (!createDevice()) {
        qz::Log::warn("Failed to create Vulkan device");
        return false;
    }

    m_initialized = true;
    return true;
}

std::vector<const char*> qzVulkanContext::getRequiredInstanceExtensions() const
{
    std::vector extensions{
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
        VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME,

#ifdef VK_KHR_WIN32_SURFACE_EXTENSION_NAME
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif

#ifdef VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME
        VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,
#endif

#ifdef USE_VALIDATION_LAYER
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
    };

#ifdef VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
    extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#endif

#ifdef VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
    extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#endif

#ifdef VK_KHR_XCB_SURFACE_EXTENSION_NAME
    extensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif

    return extensions;
}

std::vector<const char*> qzVulkanContext::getRequiredDeviceExtensions() const
{
    std::vector extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
        VK_KHR_MAINTENANCE1_EXTENSION_NAME,
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
        VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
        VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
        VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
    };

    if (m_physicalDeviceProperties.apiVersion >= VK_API_VERSION_1_1) {
        extensions.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
        extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
#ifdef VK_KHR_VIDEO_QUEUE_EXTENSION_NAME
        extensions.push_back(VK_KHR_VIDEO_QUEUE_EXTENSION_NAME);
        extensions.push_back(VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME);
#endif
#ifdef VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME
        extensions.push_back(VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME);
        extensions.push_back(VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME);
#endif
#ifdef VK_KHR_VIDEO_DECODE_AV1_EXTENSION_NAME
        extensions.push_back(VK_KHR_VIDEO_DECODE_AV1_EXTENSION_NAME);
#endif
#ifdef VK_KHR_VIDEO_DECODE_VP9_EXTENSION_NAME
        extensions.push_back(VK_KHR_VIDEO_DECODE_VP9_EXTENSION_NAME);
#endif
    }

#ifdef VK_USE_PLATFORM_WIN32_KHR
    extensions.push_back(VK_KHR_WIN32_KEYED_MUTEX_EXTENSION_NAME);
    extensions.push_back(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
    extensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
    extensions.push_back(VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME);
    extensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
#else
    extensions.push_back(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
    extensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
    extensions.push_back(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);
    extensions.push_back(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME);
#endif

    return extensions;
}

bool qzVulkanContext::createInstance()
{
    auto availableExtensions = vk::enumerateInstanceExtensionProperties(nullptr, m_dld);

    auto requiredExtensions = getRequiredInstanceExtensions();
    std::vector<const char*> enabledExtensions;

    for (const char* required : requiredExtensions) {
        auto it = std::ranges::find_if(availableExtensions,
            [required](const vk::ExtensionProperties& prop) {
                return strcmp(prop.extensionName, required) == 0;
            });
        if (it != availableExtensions.end()) {
            enabledExtensions.push_back(required);
            m_enabledInstanceExtensions.insert(required);
        }
    }

    std::vector<const char*> layers;
#ifdef USE_VALIDATION_LAYER
    auto availableLayers = vk::enumerateInstanceLayerProperties(m_dld);

    auto layerIt = std::ranges::find_if(availableLayers,
        [](const vk::LayerProperties& prop) {
            return strcmp(prop.layerName, "VK_LAYER_KHRONOS_validation") == 0;
        });
    if (layerIt != availableLayers.end()) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }
#endif

    uint32_t apiVersion = VK_API_VERSION_1_0;
    if (m_dld.vkEnumerateInstanceVersion) {
        apiVersion = vk::enumerateInstanceVersion(m_dld);
    }
    vk::ApplicationInfo appInfo(
        "qzMultimedia",
        vk::makeApiVersion(0, 1, 0, 0),
        "Qt",
        vk::makeApiVersion(0, 1, 0, 0),
        apiVersion >= VK_API_VERSION_1_1 ? VK_API_VERSION_1_1 : VK_API_VERSION_1_0
    );

    vk::InstanceCreateFlags instanceFlags{};
    if (m_enabledInstanceExtensions.count(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
        instanceFlags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
    }
    vk::InstanceCreateInfo createInfo(
        instanceFlags,
        &appInfo,
        layers,
        enabledExtensions
    );

    m_instance = vk::createInstance(createInfo, nullptr, m_dld);
    if (!m_instance) {
        qz::Log::warn("Failed to create Vulkan instance");
        return false;
    }

    initDispatchLoaderDynamic(m_vkGetInstanceProcAddr, m_instance);

#if USE_VALIDATION_LAYER
    if (m_enabledInstanceExtensions.count(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) && !layers.empty()) {
        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo;
        debugUtilsMessengerCreateInfo.messageSeverity =
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        debugUtilsMessengerCreateInfo.messageType =
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
        debugUtilsMessengerCreateInfo.pfnUserCallback = reinterpret_cast<vk::PFN_DebugUtilsMessengerCallbackEXT>(vulkanDebugCallback);

        m_debugUtilsMessenger = m_instance.createDebugUtilsMessengerEXTUnique(debugUtilsMessengerCreateInfo, nullptr, m_dld);
    }
#endif

    return true;
}

bool qzVulkanContext::selectPhysicalDevice()
{
    if (!m_instance) {
        return false;
    }

    auto devices = m_instance.enumeratePhysicalDevices(m_dld);
    if (devices.empty()) {
        qz::Log::warn("No Vulkan physical devices found");
        return false;
    }

    m_physicalDevice = vk::PhysicalDevice();
    int bestScore = -1;

    for (auto& device : devices) {
        auto properties = device.getProperties(m_dld);

        int score = 0;
        if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            score += 100;
        } else if (properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) {
            score += 50;
        }

        auto features = device.getFeatures(m_dld);
        if (features.shaderStorageImageWriteWithoutFormat) {
            score += 10;
        }

        if (score > bestScore) {
            bestScore = score;
            m_physicalDevice = device;
        }
    }

    if (!m_physicalDevice) {
        m_physicalDevice = devices[0];
    }

    queryPhysicalDeviceProperties();
    return true;
}

void qzVulkanContext::queryPhysicalDeviceProperties()
{
    if (!m_physicalDevice) {
        return;
    }

    m_physicalDeviceProperties = m_physicalDevice.getProperties(m_dld);
    m_physicalDeviceFeatures = m_physicalDevice.getFeatures(m_dld);
    m_memoryProperties = m_physicalDevice.getMemoryProperties(m_dld);

    auto queueFamilyProperties = m_physicalDevice.getQueueFamilyProperties(m_dld);

    m_queueFamilies.clear();
    for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); ++i) {
        const auto& props = queueFamilyProperties[i];
        if (props.queueCount > 0) {
            m_queueFamilies.push_back({
                i,
                props.queueCount,
                props.queueFlags,
                {}
            });
        }
    }
}

void qzVulkanContext::queryVideoCodecOperations()
{
    if (!m_physicalDevice || !hasDeviceExtension(VK_KHR_VIDEO_QUEUE_EXTENSION_NAME)) {
        return;
    }

    uint32_t queueFamilyCount = static_cast<uint32_t>(m_queueFamilies.size());
    std::vector<vk::QueueFamilyVideoPropertiesKHR> videoProps(queueFamilyCount);
    std::vector<vk::QueueFamilyProperties2> queueFamilyProps2(queueFamilyCount);

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        videoProps[i].pNext = nullptr;
        videoProps[i].videoCodecOperations = vk::VideoCodecOperationFlagsKHR();

        queueFamilyProps2[i].pNext = &videoProps[i];
    }

    m_physicalDevice.getQueueFamilyProperties2KHR(&queueFamilyCount, queueFamilyProps2.data(), m_dld);

    for (uint32_t i = 0; i < queueFamilyCount && i < m_queueFamilies.size(); ++i) {
        m_queueFamilies[i].videoCodecOperations = videoProps[i].videoCodecOperations;
    }
}

bool qzVulkanContext::createDevice()
{
    if (!m_physicalDevice) {
        return false;
    }

    auto requiredExtensions = getRequiredDeviceExtensions();

    auto availableExtensions = m_physicalDevice.enumerateDeviceExtensionProperties(nullptr, m_dld);

    std::vector<const char*> enabledExtensions;
    for (const char* required : requiredExtensions) {
        auto it = std::ranges::find_if(availableExtensions,
            [required](const vk::ExtensionProperties& prop) {
                return strcmp(prop.extensionName, required) == 0;
            });
        if (it != availableExtensions.end()) {
            enabledExtensions.push_back(required);
            m_enabledDeviceExtensions.insert(required);
        }
    }

    queryVideoCodecOperations();

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::vector<std::vector<float>> queuePriorities;

    std::unordered_map<uint32_t, uint32_t> familyQueueCounts;
    for (const auto& family : m_queueFamilies) {
        familyQueueCounts.emplace(family.familyIndex, family.queueCount);
    }

    for (const auto& pair : familyQueueCounts) {
        queuePriorities.emplace_back(pair.second, 1.0f / pair.second);
        queueCreateInfos.emplace_back(
            vk::DeviceQueueCreateFlags(),
            pair.first,
            pair.second,
            queuePriorities.back().data()
        );
    }

    m_v11f = vk::PhysicalDeviceVulkan11Features();
    m_v12f = vk::PhysicalDeviceVulkan12Features();
    m_v13f = vk::PhysicalDeviceVulkan13Features();
    m_enabledDeviceFeatures = vk::PhysicalDeviceFeatures2();

    m_enabledDeviceFeatures.features.shaderStorageImageWriteWithoutFormat = VK_TRUE;

    if (m_physicalDeviceProperties.apiVersion >= VK_API_VERSION_1_1) {
        m_enabledDeviceFeatures.pNext = &m_v11f;
        m_v11f.pNext = &m_v12f;
        m_v12f.pNext = &m_v13f;
        m_v13f.pNext = nullptr;

        m_physicalDevice.getFeatures2(&m_enabledDeviceFeatures, m_dld);
    }

    m_enabledDeviceFeatures.pNext = &m_v11f;
    m_v11f.pNext = &m_v12f;
    m_v12f.pNext = &m_v13f;
    m_v13f.pNext = nullptr;

    vk::DeviceCreateInfo deviceCreateInfo(
        vk::DeviceCreateFlags(),
        queueCreateInfos,
        {},
        enabledExtensions
    );
    deviceCreateInfo.pNext = &m_enabledDeviceFeatures;

    m_device = m_physicalDevice.createDevice(deviceCreateInfo, nullptr, m_dld);
    if (!m_device) {
        qz::Log::warn("Failed to create Vulkan device");
        return false;
    }

    m_hasYcbcr = m_v11f.samplerYcbcrConversion;
    m_hasTimelineSemaphore = m_v12f.timelineSemaphore;
    m_hasSynchronization2 = m_v13f.synchronization2;
    m_hasVideoDecode = hasDeviceExtension(VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME);

    retrieveQueues();
    return true;
}

void qzVulkanContext::retrieveQueues()
{
    if (!m_device) {
        return;
    }

    m_queues.clear();

    for (const auto& family : m_queueFamilies) {
        for (uint32_t i = 0; i < family.queueCount; ++i) {
            vk::Queue queue = m_device.getQueue(family.familyIndex, i, m_dld);
            m_queues.push_back({
                family.familyIndex,
                i,
                queue
            });
        }
    }
}

vk::Queue qzVulkanContext::queue(uint32_t familyIndex, uint32_t queueIndex) const
{
    auto it = std::ranges::find_if(m_queues,
        [familyIndex, queueIndex](const QueueInfo& info) {
            return info.familyIndex == familyIndex && info.queueIndex == queueIndex;
        });

    return it != m_queues.end() ? it->queue : vk::Queue();
}

vk::Queue qzVulkanContext::findQueueByFlag(vk::QueueFlags flag) const
{
    const auto it = std::ranges::find_if(m_queues,
        [this, flag](const QueueInfo& info) {
            const auto familyIt = std::ranges::find_if(m_queueFamilies,
                [&info](const QueueFamilyInfo& family) {
                    return family.familyIndex == info.familyIndex;
                });
            return familyIt != m_queueFamilies.end() &&
                   (familyIt->flags & flag);
        });
    return it != m_queues.end() ? it->queue : vk::Queue();
}

vk::Queue qzVulkanContext::graphicsQueue() const
{
    return findQueueByFlag(vk::QueueFlagBits::eGraphics);
}

vk::Queue qzVulkanContext::computeQueue() const
{
    return findQueueByFlag(vk::QueueFlagBits::eCompute);
}

vk::Queue qzVulkanContext::transferQueue() const
{
    return findQueueByFlag(vk::QueueFlagBits::eTransfer);
}

vk::Queue qzVulkanContext::videoDecodeQueue() const
{
#if VK_HEADER_VERSION > 237
    return findQueueByFlag(vk::QueueFlagBits::eVideoDecodeKHR);
#else
    return vk::Queue();
#endif
}

bool qzVulkanContext::hasInstanceExtension(const char* extensionName) const
{
    return m_enabledInstanceExtensions.contains(extensionName);
}

bool qzVulkanContext::hasDeviceExtension(const char* extensionName) const
{
    return m_enabledDeviceExtensions.contains(extensionName);
}

uint32_t qzVulkanContext::findMemoryType(uint32_t typeBits, vk::MemoryPropertyFlags properties) const
{
    for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; ++i) {
        if ((typeBits & (1 << i)) &&
            (m_memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    qz::Log::warn("Failed to find suitable memory type");
    return UINT32_MAX;
}

uint32_t qzVulkanContext::findQueueFamilyIndex(vk::QueueFlags flags) const
{
    auto it = std::ranges::find_if(m_queueFamilies,
        [flags](const QueueFamilyInfo& info) {
            return (info.flags & flags) == flags;
        });

    if (it != m_queueFamilies.end()) {
        return it->familyIndex;
    }

    qz::Log::warn("Failed to find suitable queue family");
    return UINT32_MAX;
}

void qzVulkanContext::lockQueue(uint32_t queueFamilyIndex, uint32_t queueIndex)
{
    uint64_t key = makeQueueKey(queueFamilyIndex, queueIndex);
    std::mutex *queueMutex = nullptr;
    {
        std::lock_guard<std::mutex> locker(m_queueMutex);
        auto &mutexPtr = m_queueLocks[key];
        if (!mutexPtr) {
            mutexPtr = std::make_unique<std::mutex>();
        }
        queueMutex = mutexPtr.get();
    }
    // Lock the per-queue mutex outside of m_queueMutex to avoid deadlock
    queueMutex->lock();
}

void qzVulkanContext::unlockQueue(uint32_t queueFamilyIndex, uint32_t queueIndex)
{
    uint64_t key = makeQueueKey(queueFamilyIndex, queueIndex);
    std::mutex *queueMutex = nullptr;
    {
        std::lock_guard<std::mutex> locker(m_queueMutex);
        auto it = m_queueLocks.find(key);
        if (it != m_queueLocks.end()) {
            queueMutex = it->second.get();
        }
    }
    if (queueMutex) {
        queueMutex->unlock();
    }
}
