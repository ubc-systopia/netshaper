add_library(shapedTransciever STATIC Sender.cpp Receiver.cpp)
target_link_libraries(shapedTransciever PRIVATE msquic)

# Build example
add_executable(exampleShapedTransciever example.cpp)
target_link_libraries(exampleShapedTransciever shapedTransciever)