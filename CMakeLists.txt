cmake_minimum_required(VERSION 3.21)
project(minesVPN C CXX)

set(CMAKE_CXX_STANDARD 23)  #TODO: Add stacktrace after gcc starts supporting it
set(CMAKE_C_STANDARD 23)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif ()

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unknown-pragmas")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

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
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

find_package(nlohmann_json REQUIRED)

#Uncommenting enables the functionality
#add_compile_definitions(DEBUGGING) # Enables DEBUG logs
#add_compile_definitions(RECORD_STATS) # Enables internal timestamps
#add_compile_definitions(SHAPING) # Enables shaping (else, just proxies data)

include_directories(msquic/src/inc)

add_subdirectory(src/modules/quic_wrapper)
add_subdirectory(src/modules/tcp_wrapper)
add_subdirectory(src/modules/lamport_queue/queue)
add_subdirectory(src/modules/shaper)
add_subdirectory(src/example/util)


# Build example
add_subdirectory(src/example/peer1)
add_subdirectory(src/example/peer2)