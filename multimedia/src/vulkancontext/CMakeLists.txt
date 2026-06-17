find_package(Vulkan)

if(Vulkan_FOUND)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_CXX_SCAN_FOR_MODULES ON)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    qt_add_library(qzVulkanContext SHARED)

    target_sources(qzVulkanContext PUBLIC FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES
            vulkancontext.cppm
    )
    target_sources(qzVulkanContext PRIVATE
        core/vulkancontext_impl.cpp
    )

    target_link_libraries(qzVulkanContext PUBLIC
        Qt6::Core
        Qt6::Gui
        Vulkan::Vulkan
        qzLog
    )

    # On Android, Vulkan SDK provides C++ headers (vulkan.hpp) that are not in the NDK
    if(ANDROID AND DEFINED ENV{VULKAN_SDK})
        target_include_directories(qzVulkanContext SYSTEM PUBLIC
            $ENV{VULKAN_SDK}/include
        )
    endif()

    if(WIN32)
        target_compile_definitions(qzVulkanContext
            PUBLIC -DVK_USE_PLATFORM_WIN32_KHR
        )
    elseif(ANDROID)
        target_compile_definitions(qzVulkanContext
            PUBLIC -DVK_USE_PLATFORM_ANDROID_KHR
        )
    endif()

    target_compile_features(qzVulkanContext PUBLIC cxx_std_23)

    if(ANDROID)
        target_compile_definitions(qzVulkanContext PRIVATE VK_USE_PLATFORM_ANDROID_KHR)
        target_link_libraries(qzVulkanContext PRIVATE android)
    endif()

    # 生成模块初始化器 workaround（解决 Windows DLL + C++20/23 模块符号导出问题）
    add_cpp_module_consumer(qzVulkanContext
        MODULES qzVulkanContext
    )
endif()
