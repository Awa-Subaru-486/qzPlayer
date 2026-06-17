module;

#include "core/vulkancontext_platform.hpp"

#include <vector>
#include <unordered_set>
#include <mutex>
#include <map>
#include <cstdint>
#include <string>
#include <algorithm>
#include <memory>
#include <utility>

export module qzVulkanContext;

export class qzVulkanContext
{
public:
    struct QueueFamilyInfo {
        uint32_t familyIndex;
        uint32_t queueCount;
        vk::QueueFlags flags;
        vk::VideoCodecOperationFlagsKHR videoCodecOperations;
    };

    struct QueueInfo {
        uint32_t familyIndex;
        uint32_t queueIndex;
        vk::Queue queue;
    };

    static qzVulkanContext* instance();
    static void destroy();

    bool initialize();
    bool isInitialized() const { return m_initialized; }

    [[nodiscard]] vk::Instance vkInstance() const { return m_instance; }
    [[nodiscard]] vk::PhysicalDevice physicalDevice() const { return m_physicalDevice; }
    [[nodiscard]] vk::Device device() const { return m_device; }

    const vk::PhysicalDeviceProperties& physicalDeviceProperties() const { return m_physicalDeviceProperties; }
    const vk::PhysicalDeviceFeatures& physicalDeviceFeatures() const { return m_physicalDeviceFeatures; }
    const vk::PhysicalDeviceMemoryProperties& memoryProperties() const { return m_memoryProperties; }
    const vk::PhysicalDeviceFeatures2& enabledDeviceFeatures() const { return m_enabledDeviceFeatures; }

    const std::vector<QueueFamilyInfo>& queueFamilies() const { return m_queueFamilies; }
    const std::vector<QueueInfo>& queues() const { return m_queues; }

    vk::Queue queue(uint32_t familyIndex, uint32_t queueIndex) const;
    vk::Queue graphicsQueue() const;
    vk::Queue computeQueue() const;
    vk::Queue transferQueue() const;
    vk::Queue videoDecodeQueue() const;

    const std::unordered_set<std::string>& enabledInstanceExtensions() const { return m_enabledInstanceExtensions; }
    const std::unordered_set<std::string>& enabledDeviceExtensions() const { return m_enabledDeviceExtensions; }

    bool hasInstanceExtension(const char* extensionName) const;
    bool hasDeviceExtension(const char* extensionName) const;

    [[nodiscard]] uint32_t findMemoryType(uint32_t typeBits, vk::MemoryPropertyFlags properties) const;
    [[nodiscard]] uint32_t findQueueFamilyIndex(vk::QueueFlags flags) const;

    [[nodiscard]] bool hasYcbcrSupport() const { return m_hasYcbcr; }
    [[nodiscard]] bool hasTimelineSemaphore() const { return m_hasTimelineSemaphore; }
    [[nodiscard]] bool hasSynchronization2() const { return m_hasSynchronization2; }
    [[nodiscard]] bool hasVideoDecodeSupport() const { return m_hasVideoDecode; }

    void lockQueue(uint32_t queueFamilyIndex, uint32_t queueIndex);
    void unlockQueue(uint32_t queueFamilyIndex, uint32_t queueIndex);

    static uint64_t makeQueueKey(uint32_t queueFamilyIndex, uint32_t queueIndex) {
        uint64_t key = queueFamilyIndex;
        key <<= 32;
        key |= queueIndex;
        return key;
    }

    const vk::detail::DispatchLoaderDynamic& dld() const { return m_dld; }
    [[nodiscard]] PFN_vkGetInstanceProcAddr getVkGetInstanceProcAddr() const { return m_vkGetInstanceProcAddr; }

private:
    qzVulkanContext();
    ~qzVulkanContext();
    qzVulkanContext(const qzVulkanContext&) = delete;
    qzVulkanContext& operator=(const qzVulkanContext&) = delete;

    PFN_vkGetInstanceProcAddr loadVulkanLibrary(const std::string& vulkanLibrary = {});
    void initDispatchLoaderDynamic(PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr, vk::Instance instance = {});

    bool createInstance();
    bool selectPhysicalDevice();
    bool createDevice();

    [[nodiscard]] std::vector<const char*> getRequiredInstanceExtensions() const;
    [[nodiscard]] std::vector<const char*> getRequiredDeviceExtensions() const;
    void queryPhysicalDeviceProperties();
    void queryVideoCodecOperations();
    void retrieveQueues();

    [[nodiscard]] vk::Queue findQueueByFlag(vk::QueueFlags flag) const;

    bool m_initialized = false;

    vk::detail::DispatchLoaderDynamic m_dld;
    PFN_vkGetInstanceProcAddr m_vkGetInstanceProcAddr = nullptr;

    vk::Instance m_instance;
    vk::PhysicalDevice m_physicalDevice;
    vk::Device m_device;

    vk::PhysicalDeviceProperties m_physicalDeviceProperties;
    vk::PhysicalDeviceFeatures m_physicalDeviceFeatures;
    vk::PhysicalDeviceMemoryProperties m_memoryProperties;

    std::vector<QueueFamilyInfo> m_queueFamilies;
    std::vector<QueueInfo> m_queues;

    std::unordered_set<std::string> m_enabledInstanceExtensions;
    std::unordered_set<std::string> m_enabledDeviceExtensions;

    bool m_hasYcbcr = false;
    bool m_hasTimelineSemaphore = false;
    bool m_hasSynchronization2 = false;
    bool m_hasVideoDecode = false;

    vk::PhysicalDeviceVulkan11Features m_v11f;
    vk::PhysicalDeviceVulkan12Features m_v12f;
    vk::PhysicalDeviceVulkan13Features m_v13f;
    vk::PhysicalDeviceFeatures2 m_enabledDeviceFeatures;

    std::mutex m_queueMutex;
    std::map<uint64_t, std::unique_ptr<std::mutex>> m_queueLocks;

    vk::UniqueHandle<vk::DebugUtilsMessengerEXT, vk::detail::DispatchLoaderDynamic> m_debugUtilsMessenger;
};
