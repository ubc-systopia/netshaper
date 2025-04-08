#include "../modules/tcp_wrapper/Server.h"

#include <thread>

constexpr std::chrono::seconds SERVER_SLEEP_POLL = std::chrono::seconds(1);

int main(int, char *[]) {
    TCP::Server server{};
    server.startListening();

    while (true) {
        std::this_thread::sleep_for(SERVER_SLEEP_POLL);
    }
    return 0;
}
