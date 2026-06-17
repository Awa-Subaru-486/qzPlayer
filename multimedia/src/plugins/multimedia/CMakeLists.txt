if(ENABLE_FFMPEG_PLUGIN)
    add_subdirectory(ffmpeg)
endif()

if(WIN32 AND ENABLE_WINDOWS_PLUGIN)
    add_subdirectory(windows)
endif()
