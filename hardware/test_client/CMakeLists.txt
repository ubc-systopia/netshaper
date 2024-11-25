cmake_minimum_required(VERSION 3.21)
project(test_client CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unknown-pragmas")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

add_subdirectory(../src/modules/tcp_wrapper src/modules/tcp_wrapper)

add_subdirectory(../src/test_client src/test_client)
