add_library(unshapedTransciever STATIC Sender.cpp Receiver.cpp)

# Build example
add_executable(exampleUnshapedTransciever example.cpp)
target_link_libraries(exampleUnshapedTransciever unshapedTransciever)