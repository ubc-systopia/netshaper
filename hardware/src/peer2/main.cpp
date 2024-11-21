//
// Created by Rut Vora
//

#include "UnshapedClient.h"
#include "ShapedServer.h"
#include <fstream>
#include "../../../msquic/src/inc/external_sync.h"

pthread_rwlock_t quicSendLock;

UnshapedClient *unshapedClient = nullptr;
ShapedServer *shapedServer = nullptr;
// Load the API table. Necessary before any calls to MsQuic
// It is defined as an extern const in "msquic.hpp"
// This needs to be here (on the heap)
const MsQuicApi *MsQuic;

inline config::Peer2Config loadConfig(char *configFileName) {
  std::ifstream configFile(configFileName);
  json config;
  try {
    config = json::parse(configFile);
  } catch (...) {
    std::cout << "Could not load/parse config. Using default values" <<
              std::endl;
    config = json::parse(R"({})");
  }
  config::Peer2Config peer2Config = config.get<config::Peer2Config>();
  // Check DPDecisionLoop is a multiple of sender loop
  {
    auto DPLoopInterval = peer2Config.shapedServer.DPCreditorLoopInterval;
    auto sendingLoopInterval = peer2Config.shapedServer.sendingLoopInterval;
    if (sendingLoopInterval == 0) {
      if (peer2Config.shapedServer.strategy != BURST) {
        std::cerr << "sendingLoopInterval is 0 with strategy != BURST. This "
                     "is not allowed!" << std::endl;
        exit(1);
      }
    } else {
      auto division = DPLoopInterval / sendingLoopInterval;
      if (DPLoopInterval < sendingLoopInterval
          || division * sendingLoopInterval != DPLoopInterval) {
        std::cerr
            << "DPCreditorLoopInterval should be a multiple of sendingLoopInterval"
            << std::endl;
        exit(1);
      }
    }
  }
  // Check that shaperCore and workerCore exist and are different
  {
    if (peer2Config.shapedServer.shaperCores.empty()
        || peer2Config.shapedServer.workerCores.empty()) {
      std::cerr << "Shaper and Worker should ideally be on different cores!"
                << std::endl;
    } else if (!peer2Config.shapedServer.shaperCores.empty()
               && !peer2Config.shapedServer.workerCores.empty()) {
      std::vector<int> commonElements(
          peer2Config.shapedServer.shaperCores.size() +
          peer2Config.shapedServer.workerCores.size());
      auto end =
          std::set_intersection(peer2Config.shapedServer.shaperCores.begin(),
                                peer2Config.shapedServer.shaperCores.end(),
                                peer2Config.shapedServer.workerCores.begin(),
                                peer2Config.shapedServer.workerCores.end(),
                                commonElements.begin());

      if (end != commonElements.begin()) {
        std::cerr << "shaperCores and workerCores should not be the same!"
                  << std::endl;
        exit(1);
      }
    }
  }
  std::cout << "Config:" << peer2Config << std::endl;
  return peer2Config;
}

int main(int argc, char *argv[]) {
  pthread_rwlock_init(&quicSendLock, nullptr);
  // Load configurations
  if (argc != 2) {
    std::cerr <<
              "No config file entered! Please call this using `./peer_2 "
              "config.json`" << std::endl;
    exit(1);
  }
  auto config = loadConfig(argv[1]);

  if (fork() == 0) {
    // Child process - Unshaped Client
    unshapedClient = new UnshapedClient{config};
    std::vector<std::function<void()>> callbacks;
    // Wait for signal to exit
    waitForSignal(false, callbacks);
  } else {
    // Parent Process - Shaped Server
    // Set CPU affinity of this process to worker cores.
    // The instantiation of ShapedServer will set the shaper thread affinity
    // separately
    if (!config.shapedServer.workerCores.empty())
      setCPUAffinity(config.shapedServer.workerCores);
    sleep(2); // Wait for unshapedClient to initialise
    MsQuic = new MsQuicApi{};
    shapedServer = new ShapedServer{config};
    sleep(1);
    std::cout << "Peer is ready!" << std::endl;
    std::vector<std::function<void()>> callbacks = {
        []() {
            shapedServer->printStats();
        }};
    // Wait for signal to exit
    waitForSignal(true, callbacks);
  }
}
