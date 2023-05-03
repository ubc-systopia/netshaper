add_library(QUICWrapper STATIC Client.cpp Server.cpp)
target_link_libraries(QUICWrapper PRIVATE msquic)

# Build example
#add_executable(exampleQUICWrapper example.cpp)
#target_link_libraries(exampleQUICWrapper QUICWrapper)