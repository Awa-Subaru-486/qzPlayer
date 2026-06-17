#ifndef QZVULKANCONTEXT_PLATFORM_HPP
#define QZVULKANCONTEXT_PLATFORM_HPP

#include <qglobal.h>
#include <vulkan/vulkan.hpp>

#ifdef Q_OS_WIN
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#elif defined(Q_OS_ANDROID)
#include <vulkan/vulkan_android.h>
#elif defined(Q_OS_LINUX)
#include <vulkan/vulkan_linux.h>
#endif

#endif
