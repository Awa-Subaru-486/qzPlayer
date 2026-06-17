# =============================================================================
# 函数: get_files_by_extensions
# 描述: 递归获取指定目录下的 C++ 源文件及头文件列表，并可过滤特定目录。
# 参数:
#   dir          : 要搜索的根目录路径
#   result       : 输出变量名，用于存储找到的文件列表
#   [exts ...]   : 要搜索的文件扩展名列表（必须的）
#   [DIRS ...]   : (可选) 要排除的目录列表，需在参数前加 `DIRS` 关键字
# 示例:
#   get_files_by_extensions (src SOURCES .cpp .hpp .h)
#   get_files_by_extensions (. MY_SRCS .cc .hh DIRS ./build)
#   get_files_by_extensions (. MY_SRCS .c .cpp .h .hpp DIRS ./third_party)
# =============================================================================
function(get_files_by_extensions dir result)
    # 初始化变量
    set(search_extensions "")
    set(exclude_dirs "")
    set(parsing_extensions TRUE)  # 初始状态为解析扩展名

    # 解析参数
    if(ARGC GREATER 2)
        foreach(arg IN LISTS ARGN)
            if(arg STREQUAL "DIRS")
                set(parsing_extensions FALSE)  # 遇到 DIRS 后开始解析排除目录
            else()
                if(parsing_extensions)
                    # 解析扩展名
                    if(NOT arg MATCHES "^\\.")
                        set(arg ".${arg}")
                    endif()
                    list(APPEND search_extensions ${arg})
                else()
                    # 解析排除目录
                    list(APPEND exclude_dirs ${arg})
                endif()
            endif()
        endforeach()
    endif()

    # 检查是否有提供扩展名
    if("${search_extensions}" STREQUAL "")
        message(FATAL_ERROR "get_files_by_extensions: 必须提供至少一个扩展名")
    endif()

    # 构建 glob 模式：dir/*.ext
    set(glob_patterns "")
    foreach(ext ${search_extensions})
        list(APPEND glob_patterns "${dir}/*${ext}")
    endforeach()

    # 自动排除常见的构建目录（使用绝对路径）
    get_filename_component(ABS_DIR "${dir}" ABSOLUTE)
    list(APPEND exclude_dirs
            "${ABS_DIR}/build"
            "${ABS_DIR}/cmake-build-debug"
            "${ABS_DIR}/cmake-build-release"
            "${ABS_DIR}/_build"
            "${ABS_DIR}/out/build"
    )

    # 执行递归搜索
    file(GLOB_RECURSE sources ${glob_patterns})
    set(SOURCE_PATHS "")
    set(EXCLUDED_DIRS_LOGGED "")

    foreach(source_file ${sources})
        file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}" "${source_file}")

        set(SHOULD_EXCLUDE FALSE)

        # 检查目录排除（支持相对和绝对路径匹配）
        foreach(exclude_dir ${exclude_dirs})
            get_filename_component(exclude_abs "${exclude_dir}" ABSOLUTE)

            # 获取 source_file 的绝对路径
            get_filename_component(source_abs "${source_file}" ABSOLUTE)

            # 检查是否在排除目录下（前缀匹配）
            string(FIND "${source_abs}" "${exclude_abs}/" POS)
            if(POS EQUAL 0 OR "${source_abs}" STREQUAL "${exclude_abs}")
                set(SHOULD_EXCLUDE TRUE)

                if(NOT exclude_abs IN_LIST EXCLUDED_DIRS_LOGGED)
                    message(STATUS "Excluding files from directory: ${exclude_abs}")
                    list(APPEND EXCLUDED_DIRS_LOGGED ${exclude_abs})
                endif()
                break()
            endif()
        endforeach()

        if(NOT SHOULD_EXCLUDE)
            list(APPEND SOURCE_PATHS ${REL_PATH})
        endif()
    endforeach()

    set(${result} ${SOURCE_PATHS} PARENT_SCOPE)
endfunction()

# =============================================================================
# 函数: add_qt_modules
# 描述: 将 Qt 模块名列表转换为 CMake 目标格式，根据 QT_VERSION 自动判断 Qt 版本
# 参数:
#   output_var : 输出变量名，用于存储转换后的目标列表
#   ...        : Qt 模块名 (如 Core, Gui, Widgets)
# 示例:
#   set(QT_VERSION 6.10.1)
#   add_qt_modules(QT_LIBS Core Gui Widgets)  # 返回 Qt6::Core Qt6::Gui Qt6::Widgets
#   target_link_libraries(my_app PRIVATE ${QT_LIBS})
#
#   set(QT_VERSION 5.15.2)
#   add_qt_modules(QT_LIBS Core Gui)  # 返回 Qt5::Core Qt5::Gui
#   target_link_libraries(my_legacy_app PRIVATE ${QT_LIBS})
# =============================================================================
function(add_qt_modules output_var)
    if(NOT DEFINED QT_VERSION)
        message(FATAL_ERROR "add_qt_modules: QT_VERSION 变量未设置。请在使用前设置 QT_VERSION")
    endif()

    # 提取 QT_VERSION 的主版本号（第一个数字）
    if(QT_VERSION MATCHES "^([0-9]+)")
        set(QT_MAJOR_VERSION ${CMAKE_MATCH_1})
    else()
        message(FATAL_ERROR "add_qt_modules: 无法从 QT_VERSION='${QT_VERSION}' 中提取主版本号")
    endif()

    if(NOT QT_MAJOR_VERSION MATCHES "^[56]$")
        message(WARNING "add_qt_modules: 不支持的 Qt 主版本: ${QT_MAJOR_VERSION}。支持 Qt5 和 Qt6。")
    endif()

    set(result_list)
    foreach(module IN LISTS ARGN)
        if(QT_MAJOR_VERSION STREQUAL "6")
            list(APPEND result_list "Qt6::${module}")
        elseif(QT_MAJOR_VERSION STREQUAL "5")
            list(APPEND result_list "Qt5::${module}")
        else()
            message(FATAL_ERROR "不支持的版本 QT_VERSION='${QT_VERSION}'")
        endif()
    endforeach()

    set(${output_var} ${result_list} PARENT_SCOPE)
endfunction()

# 可以指定多种扩展名进行收集
# 使用示例
# get_res_path(
#    ICON_RESOURCES
#    "${CMAKE_CURRENT_SOURCE_DIR}/icons"
#    "*.png" "*.svg" "*.jpg"
# )
# message(STATUS "找到的资源文件: ${ICON_RESOURCES}")
function(get_res_path VAR_NAME DIRECTORY)
    set(ALL_FILES "")
    set(RELATIVE_FILES "")

    set(EXT_LIST ${ARGN})

    foreach(EXT IN LISTS EXT_LIST)
        file(GLOB_RECURSE FILES
                "${DIRECTORY}/${EXT}"
        )
        list(APPEND ALL_FILES ${FILES})
    endforeach()

    if(ALL_FILES)
        list(REMOVE_DUPLICATES ALL_FILES)
        list(SORT ALL_FILES)

        # 转换为相对路径
        foreach(FILE_PATH ${ALL_FILES})
            file(RELATIVE_PATH REL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${FILE_PATH})
            list(APPEND RELATIVE_FILES ${REL_PATH})
        endforeach()
    endif()

    set(${VAR_NAME} ${RELATIVE_FILES} PARENT_SCOPE)
endfunction()

# 检查当前是否Debug模式
function(auto_add_debug)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
        add_compile_definitions(USE_DEBUG)
    endif()
endfunction()