find_package(Boost REQUIRED COMPONENTS program_options)

include_directories(${Boost_INCLUDE_DIR})
add_executable(test_client main.cpp)
target_link_libraries(test_client ${Boost_LIBRARIES} TCPWrapper)
