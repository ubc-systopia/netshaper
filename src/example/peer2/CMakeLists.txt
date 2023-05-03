add_executable(peer_2 main.cpp ShapedServer.cpp UnshapedClient.cpp)
target_link_libraries(peer_2 TCPWrapper QUICWrapper lamportQueue helpers
    DPShaper nlohmann_json::nlohmann_json)