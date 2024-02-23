add_library(QUICWrapper STATIC Client.cpp Server.cpp)
target_link_libraries(QUICWrapper PRIVATE msquic_static)

