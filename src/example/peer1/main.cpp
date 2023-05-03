//
// Created by Rut Vora
//

#include <sys/prctl.h>
#include "UnshapedServer.h"
#include "ShapedClient.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sched.h>
#include "../../modules/PerfEval.h"

using json = nlohmann::json;

NLOHMANN_JSON_SERIALIZE_ENUM(logLevels, {
  { DEBUG, "DEBUG" },
  { WARNING, "WARNING" },
  { ERROR, "ERROR" },
})

NLOHMANN_JSON_SERIALIZE_ENUM(sendingStrategy, {
  { BURST, "BURST" },
  { UNIFORM, "UNIFORM" },
})

std::vector<int> unshapedCores{0, 1, 2, 3};
std::vector<int> shapedCores{4, 5, 6, 7};

UnshapedServer *unshapedServer = nullptr;
ShapedClient *shapedClient = nullptr;
// Load the API table. Necessary before any calls to MsQuic
// It is defined as an extern const in "msquic.hpp"
// This needs to be here (on the heap)
const MsQuicApi *MsQuic;

void handleQueueSignal(int signum) {
//  if (unshapedServer != nullptr && shapedClient == nullptr) {
//    unshapedServer->handleQueueSignal(signum);
//  } else
  if (shapedClient != nullptr && unshapedServer == nullptr) {
    shapedClient->handleQueueSignal(signum);
  } else if (shapedClient == nullptr && unshapedServer != nullptr) {
    unshapedServer->handleQueueSignal(signum);
  } else {
    if (unshapedServer != nullptr && shapedClient != nullptr)
      std::cerr << "Peer1: Both pointers present! " << getpid() << std::endl;
    else if (unshapedServer == nullptr && shapedClient == nullptr)
      std::cerr << "Peer1: Neither pointers present! " << getpid() << std::endl;
    else
      std::cerr << "Peer1: Huh? " << getpid() << std::endl;
    exit(1);
  }
}

void setCPUAffinity(std::vector<int> &cpus) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  for (int i: cpus) {
    CPU_SET(i, &mask);
  }
  int result = sched_setaffinity(0, sizeof(mask), &mask);
  if (result == -1) {
    std::cout << "Could not set CPU affinity" << std::endl;
    exit(1);
  }
}

int main() {
  // Load configurations
  std::string configFileName;
  std::cout << "Enter the config file name/path" << std::endl;
  std::cin >> configFileName;
  std::ifstream configFile(configFileName);

  json config;
  try {
    config = json::parse(configFile);
  } catch (...) {
    std::cout << "Could not load/parse config. Using default values" <<
              std::endl;
    config = json::parse(R"({})");
  }
  auto logLevel =
      static_cast<json>(config.value("logLevel", "WARNING")).get<logLevels>();
  std::string appName = config.value("appName", "minesVPNPeer1");
  auto maxClients =
      static_cast<json>(config.value("maxClients", 40)).get<int>();

  json shapedClientConfig = config.value("shapedClient", json::parse(R"({})"));
  json unshapedServerConfig = config.value("unshapedServer",
                                           json::parse(R"({})"));

  // Load shapedClientConfig
  std::string peer2Addr = shapedClientConfig.value("peer2Addr", "localhost");
  auto peer2Port =
      static_cast<json>(shapedClientConfig.value("peer2Port",
                                                 4567)).get<uint16_t>();
  auto noiseMultiplier =
      static_cast<json>(shapedClientConfig.value("noiseMultiplier",
                                                 38)).get<double>();
  auto sensitivity =
      static_cast<json>(shapedClientConfig.value("sensitivity",
                                                 500000)).get<double>();
  auto maxDecisionSize =
      static_cast<json>(shapedClientConfig.value("maxDecisionSize",
                                                 500000)).get<uint64_t>();
  auto minDecisionSize =
      static_cast<json>(shapedClientConfig.value("minDecisionSize",
                                                 0)).get<uint64_t>();

  auto DPCreditorLoopInterval =
      static_cast<json>(shapedClientConfig.value("DPCreditorLoopInterval",
                                                 50000)).get<__useconds_t>();
  auto sendingLoopInterval =
      static_cast<json>(shapedClientConfig.value("sendingLoopInterval",
                                                 50000)).get<__useconds_t>();

  if (DPCreditorLoopInterval < sendingLoopInterval) {
    std::cerr
        << "DPCreditorLoopInterval should be a multiple of sendingLoopInterval"
        << std::endl;
    exit(1);
  } else {
    auto division = DPCreditorLoopInterval / sendingLoopInterval;
    if (division * sendingLoopInterval != DPCreditorLoopInterval) {
      std::cerr
          << "DPCreditorLoopInterval should be a multiple of sendingLoopInterval"
          << std::endl;
      exit(1);
    }
  }
  auto strategy =
      static_cast<json>(shapedClientConfig.value("sendingStrategy",
                                                 "BURST")).get<sendingStrategy>();

  // Load unshapedServerConfig
  std::string bindAddr = unshapedServerConfig.value("bindAddr", "");
  auto bindPort =
      static_cast<json>(unshapedServerConfig.value("bindPort",
                                                   8000)).get<uint16_t>();
  auto checkResponseLoopInterval =
      static_cast<json>(unshapedServerConfig.value(
          "checkResponseLoopInterval", 50000)).get<uint16_t>();

  std::string serverAddr = unshapedServerConfig.value("serverAddr",
                                                      "localhost:5555");

  std::signal(SIGUSR1, handleQueueSignal);

  if (fork() == 0) {
    // Child process - Unshaped Server
    setCPUAffinity(unshapedCores);
    // This process should get a SIGHUP when it's parent (the shaped
    // client) dies
    prctl(PR_SET_PDEATHSIG, SIGHUP);
    unshapedServer = new UnshapedServer{appName, maxClients, bindAddr,
                                        bindPort,
                                        checkResponseLoopInterval,
                                        sendingLoopInterval,
                                        logLevel, serverAddr};
    // Wait for signal to exit
    waitForSignal(false);
  } else {
    // Parent Process - Shaped Client
    setCPUAffinity(shapedCores);
    sleep(2); // Wait for unshapedServer to initialise
    MsQuic = new MsQuicApi{};
    shapedClient = new ShapedClient{appName, maxClients, noiseMultiplier,
                                    sensitivity, maxDecisionSize,
                                    minDecisionSize, peer2Addr, peer2Port,
                                    DPCreditorLoopInterval,
                                    sendingLoopInterval,
                                    checkResponseLoopInterval,
                                    logLevel, strategy};
    sleep(2);
    std::cout << "Peer is ready!" << std::endl;
    // Wait for signal to exit
    waitForSignal(true);
  }
}