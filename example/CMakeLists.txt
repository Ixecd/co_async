cmake_minimum_required(VERSION 3.22)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/bin)

include_directories(${CMAKE_SOURCE_DIR}/../)

add_compile_options(-Wall -Wextra -Werror=return-type -g)

#add_executable(task task.cc)
#add_executable(recursiveTask recursiveTask.cc)
#add_executable(exception exception.cc)
#add_executable(sleep sleep.cc)

file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/*.cc")
foreach (SOURCES ${SOURCES})
string(REGEX REPLACE ".+/(.+)\\..*" "\\1" exe ${SOURCES})
add_executable (${exe} ${SOURCES})
message (\ \ \ \ --\ example/${exe}.cc\ will\ be\ compiled\ to\ bin/${exe})
endforeach ()
