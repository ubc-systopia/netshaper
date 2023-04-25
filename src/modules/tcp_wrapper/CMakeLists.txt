add_library(TCPWrapper STATIC Sender.cpp Receiver.cpp)

# Build example
#add_executable(exampleTCPWrapper example.cpp)
#target_link_libraries(exampleTCPWrapper TCPWrapper)