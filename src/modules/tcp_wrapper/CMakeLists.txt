add_library(TCPWrapper STATIC Client.cpp Server.cpp)

# Build example
add_executable(TCPProxy example.cpp)
target_link_libraries(TCPProxy TCPWrapper)