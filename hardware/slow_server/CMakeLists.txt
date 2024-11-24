cmake_minimum_required(VERSION 3.21)
project(slowTestServer CXX)

set(CMAKE_CXX_STANDARD 23)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unknown-pragmas")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

add_compile_definitions(RECORD_STATS)
add_compile_definitions(SHAPING) # Needed but we really shouldn't

include_directories(../msquic/src/inc)

add_subdirectory(../src/modules/tcp_wrapper src/modules/tcp_wrapper)

add_subdirectory(../src/slow_server src/slow_server)
