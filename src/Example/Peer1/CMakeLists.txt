add_executable(peer_1_shaped shaped.cpp)
target_link_libraries(peer_1_shaped shapedTransciever lamportQueue DPShaper
    helpers)

add_executable(peer_1_unshaped unshaped.cpp)
target_link_libraries(peer_1_unshaped unshapedTransciever lamportQueue helpers)