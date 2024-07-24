cmake_minimum_required(VERSION 3.22)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/bin)

file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/co_async/*.cc")

add_compile_options(-Wall -Wextra -Werror=return-type -g)

add_executable(main ${SOURCES})

target_include_directories(co_async PUBLIC)
