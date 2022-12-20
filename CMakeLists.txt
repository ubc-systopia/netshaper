cmake_minimum_required(VERSION 3.24)
project(proxy_cpp)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(proxy_cpp
        ProxyListener.cpp
        ProxyListener.h main.cpp)
