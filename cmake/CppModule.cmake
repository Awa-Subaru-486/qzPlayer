# CMake 模块辅助函数
# 用于自动生成 C++20/23 模块 DLL 的初始化器 workaround
# 解决 Windows DLL + 模块的兼容性问题

include_guard()

# 计算模块初始化器符号名
# 格式: _ZGIW<长度><模块名>  (主模块)
#       _ZGIW<主模块名长度><主模块名>WP<分区名长度><分区名>  (子模块分区)
function(_cpp_module_get_initializer_symbol MODULE_NAME OUTPUT_VAR)
    if(MODULE_NAME MATCHES "^([^:]+):(.+)$")
        # 子模块分区: Module:Partition
        set(MAIN_NAME "${CMAKE_MATCH_1}")
        set(PARTITION_NAME "${CMAKE_MATCH_2}")
        string(LENGTH "${MAIN_NAME}" MAIN_LENGTH)
        string(LENGTH "${PARTITION_NAME}" PARTITION_LENGTH)
        set(${OUTPUT_VAR} "_ZGIW${MAIN_LENGTH}${MAIN_NAME}WP${PARTITION_LENGTH}${PARTITION_NAME}" PARENT_SCOPE)
    else()
        # 主模块
        string(LENGTH "${MODULE_NAME}" NAME_LENGTH)
        set(${OUTPUT_VAR} "_ZGIW${NAME_LENGTH}${MODULE_NAME}" PARENT_SCOPE)
    endif()
endfunction()

# 生成模块初始化器源文件
function(_cpp_module_generate_initializer_source MODULE_NAME OUTPUT_FILE)
    _cpp_module_get_initializer_symbol("${MODULE_NAME}" SYMBOL_NAME)

    set(CONTENT "// 自动生成的模块初始化器 workaround
// 用于解决 Windows DLL + C++20/23 模块的兼容性问题
// 模块: ${MODULE_NAME}
// 符号: ${SYMBOL_NAME}

extern \"C\" {

void ${SYMBOL_NAME}()
{
    // 空实现 - 仅用于满足链接器符号需求
}

} // extern \"C\"
")

    file(WRITE "${OUTPUT_FILE}" "${CONTENT}")
endfunction()

# 添加 C++ 模块共享库
#
# 用法:
#   add_cpp_module_library(<库名>
#       SOURCES <源文件...>
#       MODULE_SOURCES <模块源文件...>
#       [DEPENDS <依赖库...>]
#       [INCLUDE_DIRS <包含目录...>]
#   )
#
# 参数:
#   SOURCES        - 实现源文件 (.cpp)
#   MODULE_SOURCES - 模块接口源文件 (.cppm)
#   DEPENDS        - 依赖的其他库
#   INCLUDE_DIRS   - 额外的包含目录
#
# 功能:
#   1. 创建共享库
#   2. 自动生成模块初始化器 workaround
#   3. 创建消费者接口库，自动链接初始化器
#
function(add_cpp_module_library LIB_NAME)
    cmake_parse_arguments(ARG
        ""
        ""
        "SOURCES;MODULE_SOURCES;DEPENDS;INCLUDE_DIRS"
        ${ARGN}
    )

    # 创建共享库
    add_library(${LIB_NAME} SHARED)

    # 添加模块源文件
    if(ARG_MODULE_SOURCES)
        target_sources(${LIB_NAME}
            PUBLIC
                FILE_SET CXX_MODULES
                FILES ${ARG_MODULE_SOURCES}
        )
    endif()

    # 添加实现源文件
    if(ARG_SOURCES)
        target_sources(${LIB_NAME} PRIVATE ${ARG_SOURCES})
    endif()

    # 设置包含目录
    if(ARG_INCLUDE_DIRS)
        target_include_directories(${LIB_NAME} PUBLIC ${ARG_INCLUDE_DIRS})
    endif()

    # 设置编译选项
    set_target_properties(${LIB_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )

    # 链接依赖
    if(ARG_DEPENDS)
        target_link_libraries(${LIB_NAME} PUBLIC ${ARG_DEPENDS})
    endif()

    # 生成模块初始化器源文件
    set(INITIALIZER_FILE "${CMAKE_CURRENT_BINARY_DIR}/${LIB_NAME}_module_init.cpp")
    _cpp_module_generate_initializer_source("${LIB_NAME}" "${INITIALIZER_FILE}")

    # 创建消费者接口库
    add_library(${LIB_NAME}_consumer INTERFACE)
    target_sources(${LIB_NAME}_consumer INTERFACE
        $<BUILD_INTERFACE:${INITIALIZER_FILE}>
    )
    target_link_libraries(${LIB_NAME}_consumer INTERFACE ${LIB_NAME})

    # 导出消费者库
    set_target_properties(${LIB_NAME}_consumer PROPERTIES
        EXPORT_NAME ${LIB_NAME}_consumer
    )

    # 打印信息
    message(STATUS "C++ Module Library: ${LIB_NAME}")
    message(STATUS "  - Module sources: ${ARG_MODULE_SOURCES}")
    message(STATUS "  - Implementation sources: ${ARG_SOURCES}")
    message(STATUS "  - Initializer file: ${INITIALIZER_FILE}")
endfunction()

# 创建 C++ 模块消费者接口库
#
# 用法:
#   add_cpp_module_consumer(<库名>
#       MODULES <模块名列表...>
#       [INCLUDE_DIRS <包含目录...>]
#   )
#
# 参数:
#   LIB_NAME     - 主库目标名
#   MODULES      - 需要生成初始化器的模块名列表（支持 Module:Partition 格式）
#   INCLUDE_DIRS - 额外的包含目录
#
# 功能:
#   1. 为每个模块生成初始化器源文件
#   2. 创建 <库名>_consumer 接口库
#   3. 自动链接初始化器和主库
#
function(add_cpp_module_consumer LIB_NAME)
    cmake_parse_arguments(ARG
        ""
        ""
        "MODULES;INCLUDE_DIRS"
        ${ARGN}
    )

    # 生成模块初始化器源文件
    set(_INIT_SOURCES "")
    foreach(_mod ${ARG_MODULES})
        string(REPLACE ":" "_" _mod_safe "${_mod}")
        set(_init_file "${CMAKE_CURRENT_BINARY_DIR}/${_mod_safe}_module_init.cpp")
        _cpp_module_generate_initializer_source("${_mod}" "${_init_file}")
        list(APPEND _INIT_SOURCES "${_init_file}")
    endforeach()

    # 创建消费者接口库
    add_library(${LIB_NAME}_consumer INTERFACE)
    target_sources(${LIB_NAME}_consumer INTERFACE
        ${_INIT_SOURCES}
    )
    if(ARG_INCLUDE_DIRS)
        target_include_directories(${LIB_NAME}_consumer INTERFACE ${ARG_INCLUDE_DIRS})
    endif()
    target_link_libraries(${LIB_NAME}_consumer INTERFACE ${LIB_NAME})

    # 导出消费者库
    set_target_properties(${LIB_NAME}_consumer PROPERTIES
        EXPORT_NAME ${LIB_NAME}_consumer
    )
endfunction()

# 链接 C++ 模块库（消费者使用）
#
# 用法:
#   target_link_cpp_module(<目标> <模块库...>)
#
# 功能:
#   自动链接模块库及其初始化器
#
function(target_link_cpp_module TARGET)
    foreach(LIB_NAME ${ARGN})
        # 链接模块库
        target_link_libraries(${TARGET} PRIVATE ${LIB_NAME})

        # 链接消费者接口（包含初始化器）
        if(TARGET ${LIB_NAME}_consumer)
            target_link_libraries(${TARGET} PRIVATE ${LIB_NAME}_consumer)
        endif()

        # 复制 DLL 到输出目录
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${LIB_NAME}>"
            "$<TARGET_FILE_DIR:${TARGET}>"
        )
    endforeach()
endfunction()
