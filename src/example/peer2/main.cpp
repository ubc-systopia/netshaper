//
// Created by Rut Vora
//

#include "UnshapedClient.h"
#include "ShapedServer.h"
#include <sys/prctl.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sched.h>
#include "../../modules/PerfEval.h"
#include "../../../msquic/src/inc/external_sync.h"

pthread_rwlock_t quicSendLock;

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

std::vector<int> unshapedCores{8, 9, 10, 11};
std::vector<int> shapedCores{12, 13, 14, 15};

UnshapedClient *unshapedClient = nullptr;
ShapedServer *shapedServer = nullptr;
// Load the API table. Necessary before any calls to MsQuic
// It is defined as an extern const in "msquic.hpp"
// This needs to be here (on the heap)
const MsQuicApi *MsQuic;

void handleQueueSignal(int signum) {
  if (unshapedClient != nullptr && shapedServer == nullptr) {
    unshapedClient->handleQueueSignal(signum);
  } else if (unshapedClient == nullptr && shapedServer != nullptr) {
    shapedServer->handleQueueSignal(signum);
  } else {
    std::cerr << "Peer2: Issue with handling queue signal!" << std::endl;
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
  pthread_rwlock_init(&quicSendLock, nullptr);
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
  std::string appName = config.value("appName", "minesVPNPeer2");
  auto maxStreamsPerPeer =
      static_cast<json>(config.value("maxStreamsPerPeer",
                                     40)).get<int>();

  json shapedServerConfig = config.value("shapedServer",
                                         json::parse(R"({})"));
  json unshapedClientConfig = config.value("unshapedClient",
                                           json::parse(R"({})"));

  // Load shapedServerConfig
  std::string serverCert = shapedServerConfig.value("serverCert",
                                                    "server.cert");
  std::string serverKey = shapedServerConfig.value("serverKey", "server.key");
  auto listeningPort =
      static_cast<json>(shapedServerConfig.value("listeningPort",
                                                 4567)).get<uint16_t>();
  auto noiseMultiplier =
      static_cast<json>(shapedServerConfig.value("noiseMultiplier",
                                                 38)).get<double>();
  auto sensitivity =
      static_cast<json>(shapedServerConfig.value("sensitivity",
                                                 500000)).get<double>();
  auto maxDecisionSize =
      static_cast<json>(shapedServerConfig.value("maxDecisionSize",
                                                 500000)).get<uint64_t>();
  auto minDecisionSize =
      static_cast<json>(shapedServerConfig.value("minDecisionSize",
                                                 0)).get<uint64_t>();

  auto DPCreditorLoopInterval =
      static_cast<json>(shapedServerConfig.value("DPCreditorLoopInterval",
                                                 50000)).get<__useconds_t>();
  auto sendingLoopInterval =
      static_cast<json>(shapedServerConfig.value("sendingLoopInterval",
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
      static_cast<json>(shapedServerConfig.value("sendingStrategy",
                                                 "BURST")).get<sendingStrategy>();

  // Load unshapedClientConfig
  auto checkQueuesInterval =
      static_cast<json>(unshapedClientConfig.value(
          "checkQueuesInterval", 50000)).get<uint16_t>();

  std::signal(SIGUSR1, handleQueueSignal);

  if (fork() == 0) {
    // Child process - Unshaped Client
    setCPUAffinity(unshapedCores);
    // This process should get a SIGHUP when it's parent (the shaped
    // server) dies
    prctl(PR_SET_PDEATHSIG, SIGHUP);

    unshapedClient = new UnshapedClient{appName, 1, maxStreamsPerPeer,
                                        checkQueuesInterval,
                                        sendingLoopInterval, logLevel};
    // Wait for signal to exit
    waitForSignal(false);
  } else {
    // Parent Process - Shaped Server
    setCPUAffinity(shapedCores);
    sleep(2); // Wait for unshapedClient to initialise
    MsQuic = new MsQuicApi{};
    shapedServer = new ShapedServer{appName, serverCert, serverKey,
                                    1, maxStreamsPerPeer, listeningPort,
                                    noiseMultiplier, sensitivity,
                                    maxDecisionSize, minDecisionSize,
                                    DPCreditorLoopInterval,
                                    sendingLoopInterval,
                                    checkQueuesInterval, logLevel,
                                    strategy};
    sleep(1);
    std::cout << "Peer is ready!" << std::endl;
    // Wait for signal to exit
    waitForSignal(true);
  }
}