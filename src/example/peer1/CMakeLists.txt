add_executable(peer_1 main.cpp UnshapedReceiver.cpp ShapedSender.cpp)
target_link_libraries(peer_1 TCPWrapper QUICWrapper DPShaper lamportQueue
    helpers nlohmann_json::nlohmann_json)