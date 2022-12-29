add_executable(shapedTransciever Sender.cpp Receiver.cpp main.cpp)
target_link_libraries(shapedTransciever PRIVATE msquic)