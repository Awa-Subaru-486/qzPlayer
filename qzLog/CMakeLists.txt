# === fmt library (static, used by spdlog and consumers) ===
set(FMT_MASTER_PROJECT OFF CACHE BOOL "" FORCE)
set(FMT_DOC OFF CACHE BOOL "" FORCE)
set(FMT_TEST OFF CACHE BOOL "" FORCE)
set(FMT_FUZZ OFF CACHE BOOL "" FORCE)
set(FMT_INSTALL OFF CACHE BOOL "" FORCE)
set(FMT_MODULE OFF CACHE BOOL "" FORCE)
set(FMT_CUDA_TEST OFF CACHE BOOL "" FORCE)
set(FMT_SHARED OFF CACHE BOOL "" FORCE)
set(FMT_SYSTEM_HEADERS ON CACHE BOOL "Expose headers with marking them as system" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/fmt-12.1.0)

# === spdlog library (uses external fmt) ===
set(SPDLOG_MASTER_PROJECT OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_ALL OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE_HO OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS_HO OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "Use external fmt library instead of bundled" FORCE)
set(SPDLOG_FMT_EXTERNAL_HO OFF CACHE BOOL "" FORCE)
set(SPDLOG_ENABLE_PCH OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/spdlog-1.17.0)

# 隐藏 spdlog 静态库的所有符号，防止从 qzLog DLL 中泄露
set_target_properties(spdlog PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# === qzLog shared library (C++20 module) ===
add_library(qzLog SHARED)

target_sources(qzLog
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/qzLog.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/qzLogQt.cppm
)

target_compile_features(qzLog PUBLIC cxx_std_23)

# spdlog 静态链接进 qzLog 动态库（PRIVATE），符号不对外暴露
# fmt 静态库 PUBLIC 供消费者使用 fmt::（头文件 + 链接）
# Qt6::Core PUBLIC 供 Qt 类型 fmt 格式化
# Threads::Threads 必须 PUBLIC，否则消费者导入模块 PCM 时配置不匹配
target_link_libraries(qzLog
    PRIVATE spdlog::spdlog
    PUBLIC fmt::fmt Threads::Threads Qt6::Core Qt6::Gui
)

# qzLog 不设置 visibility hidden，确保模块 export 的符号正常导出
# spdlog 已单独设置 hidden，其符号不会从 DLL 泄露

# Android log support (android_sink 内部依赖)
if(ANDROID)
    find_library(ANDROID_LOG log)
    target_link_libraries(qzLog PUBLIC ${ANDROID_LOG})
endif()
