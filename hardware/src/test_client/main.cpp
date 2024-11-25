#include "../modules/tcp_wrapper/Client.h"

#include <boost/program_options.hpp>
#include <thread>

void runTest(TCP::Client& client, int numIterations, std::size_t numBytes, int timeBetweenMessagesS) {
    std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::vector<uint8_t> buffer;
    for (std::size_t i = 0; i < numBytes; i++) {
        buffer.push_back(charset[rand() % charset.size()]);
    }
    for (int i = 0; i < numIterations; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(timeBetweenMessagesS));
        client.sendData(buffer.data(), buffer.size());
    }
}

int main(int argc, char **argv) {
    using namespace boost::program_options;

    try {
        boost::program_options::options_description desc{"Options for client"};
        desc.add_options()
            ("help,h", "Help screen")
            ("remoteAddress", value<std::string>()->required(), "The remote address to connect to")
            ("remotePort", value<int>()->default_value(8000)->required(), "The remote port to connect to")
            ("numIterations", value<int>()->default_value(100)->required(), "The number of iterations to run")
            ("numBytes", value<std::size_t>()->default_value(1000)->required(), "The number of bytes to send")
            ("timeBetweenMessagesS", value<int>()->default_value(10)->required(), "The time between messages in seconds");

        variables_map vm;
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
        boost::program_options::notify(vm);

        std::string remoteAddress = vm["remoteAddress"].as<std::string>();
        int remotePort = vm["remotePort"].as<int>();
        int numIterations = vm["numIterations"].as<int>();
        std::size_t numBytes = vm["numBytes"].as<std::size_t>();
        int timeBetweenMessagesS = vm["timeBetweenMessagesS"].as<int>();

        std::cout << "Running client, connecting to: " << remoteAddress << ":" << remotePort << std::endl;
        std::cout << "Sending " << numIterations << " messages of " << numBytes << " bytes with " << timeBetweenMessagesS
            << " seconds between messages" << std::endl;

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }

        TCP::Client client(remoteAddress, remotePort);
        runTest(client, numIterations, numBytes, timeBetweenMessagesS);

    } catch (const boost::program_options::error &e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
