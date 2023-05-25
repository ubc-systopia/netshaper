//
// Created by Rut Vora
//

#include <sys/prctl.h>
#include "UnshapedServer.h"
#include "ShapedClient.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include "../../modules/PerfEval.h"
#include "../../../msquic/src/inc/external_sync.h"

pthread_rwlock_t quicSendLock;

UnshapedServer *unshapedServer = nullptr;
ShapedClient *shapedClient = nullptr;
// Load the API table. Necessary before any calls to MsQuic
// It is defined as an extern const in "msquic.hpp"
// This needs to be here (on the heap)
const MsQuicApi *MsQuic;

inline config::Peer1Config loadConfig(char *configFileName) {
  std::ifstream configFile(configFileName);
  json config;
  try {
    config = json::parse(configFile);
  } catch (...) {
    std::cout << "Could not load/parse config. Using default values" <<
              std::endl;
    config = json::parse(R"({})");
  }
  config::Peer1Config peer1Config = config.get<config::Peer1Config>();
  // Check DPDecisionLoop is a multiple of sender loop
  {
    auto DPLoopInterval = peer1Config.shapedClient.DPCreditorLoopInterval;
    auto sendingLoopInterval = peer1Config.shapedClient.sendingLoopInterval;
    if (sendingLoopInterval == 0) {
      if (peer1Config.shapedClient.strategy != BURST) {
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
    if (peer1Config.shapedClient.shaperCores.empty()
        || peer1Config.shapedClient.workerCores.empty()) {
      std::cerr << "shaperCores and workerCores should not be !" << std::endl;
      exit(1);
    }
    std::vector<int> commonElements(peer1Config.shapedClient.shaperCores.size()
                                    +
                                    peer1Config.shapedClient.workerCores.size());
    auto end =
        std::set_intersection(peer1Config.shapedClient.shaperCores.begin(),
                              peer1Config.shapedClient.shaperCores.end(),
                              peer1Config.shapedClient.workerCores.begin(),
                              peer1Config.shapedClient.workerCores.end(),
                              commonElements.begin());
    if (end != commonElements.begin()) {
      std::cerr << "shaperCores and workerCores should not be the same!"
                << std::endl;
      exit(1);
    }

  }
  std::cout << "Config:" << peer1Config << std::endl;
  return peer1Config;
}

int main(int argc, char *argv[]) {
  pthread_rwlock_init(&quicSendLock, nullptr);
  // Load configurations
  if (argc != 2) {
    std::cerr <<
              "No config file entered! Please call this using `./peer_1 "
              "config.json`" << std::endl;
    exit(1);
  }
  auto config = loadConfig(argv[1]);

  if (fork() == 0) {
    // Child process - Unshaped Server
    if (!config.unshapedServer.cores.empty())
      setCPUAffinity(config.unshapedServer.cores);

    unshapedServer = new UnshapedServer{config.appName, config.maxClients,
                                        config.logLevel,
                                        config.shapedClient.strategy == UNIFORM
                                        ? config.shapedClient.sendingLoopInterval
                                        : config.shapedClient.DPCreditorLoopInterval,
                                        config.unshapedServer};
    // Wait for signal to exit
    waitForSignal(false);
  } else {
    // Parent Process - Shaped Client
    // Set CPU affinity of this process to worker cores.
    // The instantiation of ShapedClient will set the shaper thread affinity
    // separately
    setCPUAffinity(config.shapedClient.workerCores);
    sleep(2); // Wait for unshapedServer to initialise
    MsQuic = new MsQuicApi{};
    shapedClient = new ShapedClient{config.appName, config.maxClients,
                                    config.logLevel,
                                    config.unshapedServer
                                        .checkQueuesInterval,
                                    config.shapedClient};
    sleep(2);
    std::cout << "Peer is ready!" << std::endl;
    // Wait for signal to exit
    waitForSignal(true);
  }
}