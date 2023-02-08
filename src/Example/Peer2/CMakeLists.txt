add_executable(peer_2 main.cpp ShapedReceiver.cpp UnshapedSender.cpp)
target_link_libraries(peer_2 TCPWrapper QUICWrapper lamportQueue helpers
    DPShaper)