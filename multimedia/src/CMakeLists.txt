cmake_policy(SET CMP0144 NEW)

# Android: FFmpeg 8.0.2 uses per-ABI flat layout (<abi>/include/, <abi>/lib/)
# If FFMPEG_DIR is not set or does not have include/lib, auto-detect from 3rdparty
if(ANDROID AND ENABLE_FFMPEG_PLUGIN AND ANDROID_ABI)
    if(NOT FFMPEG_DIR OR NOT EXISTS "${FFMPEG_DIR}/include" OR NOT EXISTS "${FFMPEG_DIR}/lib")
        set(_ffmpeg_abi_root "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ffmpeg/ffmpeg_android_8.0.2/${ANDROID_ABI}")
        if(EXISTS "${_ffmpeg_abi_root}/include" AND EXISTS "${_ffmpeg_abi_root}/lib")
            set(FFMPEG_DIR "${_ffmpeg_abi_root}" CACHE PATH "FFmpeg root directory" FORCE)
            message(STATUS "FFmpeg: auto-detected for ${ANDROID_ABI} -> ${FFMPEG_DIR}")
        endif()
    endif()
endif()

if(ENABLE_FFMPEG_PLUGIN)
    find_package(FFmpeg REQUIRED COMPONENTS AVCODEC AVFORMAT AVUTIL SWRESAMPLE SWSCALE)
else()
    find_package(FFmpeg QUIET COMPONENTS AVCODEC AVFORMAT AVUTIL SWRESAMPLE SWSCALE)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../qzLog ${CMAKE_BINARY_DIR}/qzLog)

add_subdirectory(3rdparty/tlsf)
add_subdirectory(multimedia)

if(TARGET Qt6::Quick)
    add_subdirectory(multimediaquick)
endif()

add_subdirectory(plugins)
