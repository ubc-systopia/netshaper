#add_executable(sender Sender.cpp main.cpp)
#target_link_libraries(sender PRIVATE msquic)

add_executable(receiver Receiver.cpp main.cpp)
target_link_libraries(receiver PRIVATE msquic)
