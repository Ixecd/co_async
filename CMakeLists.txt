cmake_minimum_required(VERSION 3.28)


if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 设置项目名称
project(co_async)


# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/lib)


# 添加头文件搜索路径
include_directories(${CMAKE_SOURCE_DIR}/co_async)
include_directories(${CMAKE_SOURCE_DIR}/utilities)

# 将 src 目录中的源文件添加到工程中
#file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/src/*.cc")
file(GLOB_RECURSE srcs CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/co_async/*.hpp ${CMAKE_SOURCE_DIR}/utilities/*.hpp)


# 创建静态库
add_library(co_async STATIC ${srcs})

# 由于只有头文件,CMake无法确定目标文件的链接器语言
set_target_properties(co_async PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(co_async PUBLIC)

