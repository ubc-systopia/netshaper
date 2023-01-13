cmake_minimum_required(VERSION 3.16)
project(minesVPN C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)

set(CMAKE_BUILD_TYPE Debug)

link_directories(/snap/cmake/current/usr/lib/x86_64-linux-gnu)

# Disable in-source builds to prevent source tree corruption.
if ("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "
FATAL: In-source builds are not allowed.
       You should create a separate directory for build files.
")
endif ()

if (UNIX)
    set(CX_PLATFORM "linux")
else ()
    message(FATAL_ERROR "FATAL: Only Linux is supported at this time")
endif ()
message(STATUS "QUIC Platform: ${CX_PLATFORM}")

include_directories(msquic/src/inc)

add_subdirectory(ShapedTransciever)

add_subdirectory(UnshapedTransciever)

add_subdirectory(lamport_queue/queue)
add_subdirectory(shaper)

# Build example
add_executable(example_peer_1_unshaped example_peer_1_unshaped.cpp)
target_link_libraries(example_peer_1_unshaped unshapedTransciever lamportQueue)

add_executable(example_peer_1_shaped example_peer_1_shaped.cpp)
target_link_libraries(example_peer_1_shaped shapedTransciever lamportQueue
        DPShaper)

add_executable(example_peer_2 example_peer_2.cpp)
target_link_libraries(example_peer_2 shapedTransciever unshapedTransciever)