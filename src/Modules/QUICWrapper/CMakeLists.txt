add_library(QUICWrapper STATIC Sender.cpp Receiver.cpp)
target_link_libraries(QUICWrapper PRIVATE msquic)

# Build example
add_executable(exampleQUICWrapper example.cpp)
target_link_libraries(exampleQUICWrapper QUICWrapper)