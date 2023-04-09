//
// Created by Rut Vora
//

#include <sys/prctl.h>
#include "UnshapedReceiver.h"
#include "ShapedSender.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

NLOHMANN_JSON_SERIALIZE_ENUM(logLevels, {
  { DEBUG, "DEBUG" },
  { WARNING, "WARNING" },
  { ERROR, "ERROR" },
})

UnshapedReceiver *unshapedReceiver = nullptr;
ShapedSender *shapedSender = nullptr;
// Load the API table. Necessary before any calls to MsQuic
// It is defined as an extern const in "msquic.hpp"
// This needs to be here (on the heap)
const MsQuicApi *MsQuic;

void handleQueueSignal(int signum) {
//  if (unshapedReceiver != nullptr && shapedSender == nullptr) {
//    unshapedReceiver->handleQueueSignal(signum);
//  } else
  if (shapedSender != nullptr && unshapedReceiver == nullptr) {
    shapedSender->handleQueueSignal(signum);
  } else if (shapedSender == nullptr && unshapedReceiver != nullptr) {
    unshapedReceiver->handleQueueSignal(signum);
  } else {
    if (unshapedReceiver != nullptr && shapedSender != nullptr)
      std::cerr << "Peer1: Both pointers present! " << getpid() << std::endl;
    else if (unshapedReceiver == nullptr && shapedSender == nullptr)
      std::cerr << "Peer1: Neither pointers present! " << getpid() << std::endl;
    else
      std::cerr << "Peer1: Huh? " << getpid() << std::endl;
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
  auto maxClients =
      static_cast<json>(config.value("maxClients", 40)).get<int>();

  json shapedSenderConfig = config.value("shapedSender", json::parse(R"({})"));
  json unshapedReceiverConfig = config.value("unshapedReceiver",
                                             json::parse(R"({})"));

  // Load shapedSenderConfig
  std::string peer2Addr = shapedSenderConfig.value("peer2Addr", "localhost");
  auto peer2Port =
      static_cast<json>(shapedSenderConfig.value("peer2Port",
                                                 4567)).get<uint16_t>();
  auto noiseMultiplier =
      static_cast<json>(shapedSenderConfig.value("noiseMultiplier",
                                                 38)).get<double>();
  auto sensitivity =
      static_cast<json>(shapedSenderConfig.value("sensitivity",
                                                 500000)).get<double>();
  auto maxDecisionSize =
      static_cast<json>(shapedSenderConfig.value("maxDecisionSize",
                                                 500000)).get<uint64_t>();
  auto minDecisionSize =
      static_cast<json>(shapedSenderConfig.value("minDecisionSize",
                                                 0)).get<uint64_t>();
  std::string appName = shapedSenderConfig.value("appName", "minesVPNPeer1");
  auto DPCreditorLoopInterval =
      static_cast<json>(shapedSenderConfig.value("DPCreditorLoopInterval",
                                                 50000)).get<__useconds_t>();
  auto senderLoopInterval =
      static_cast<json>(shapedSenderConfig.value("senderLoopInterval",
                                                 50000)).get<__useconds_t>();

  if (DPCreditorLoopInterval < senderLoopInterval) {
    std::cerr
        << "DPCreditorLoopInterval should be a multiple of senderLoopInterval"
        << std::endl;
    exit(1);
  } else {
    auto division = DPCreditorLoopInterval / senderLoopInterval;
    if (division * senderLoopInterval != DPCreditorLoopInterval) {
      std::cerr
          << "DPCreditorLoopInterval should be a multiple of senderLoopInterval"
          << std::endl;
      exit(1);
    }
  }

  // Load unshapedReceiverConfig
  std::string bindAddr = unshapedReceiverConfig.value("bindAddr", "");
  auto bindPort =
      static_cast<json>(unshapedReceiverConfig.value("bindPort",
                                                     8000)).get<uint16_t>();
  auto checkResponseLoopInterval =
      static_cast<json>(unshapedReceiverConfig.value(
          "checkResponseLoopInterval", 50000)).get<uint16_t>();

  std::string serverAddr = unshapedReceiverConfig.value("serverAddr",
                                                        "localhost:5555");

//  std::cout << "Enter the maximum number of clients that should be supported:"
//               " " << std::endl;
//  std::cin >> maxClients;
//
//  std::cout << "Enter peer2's Address" << std::endl;
//  std::cin >> peer2Addr;
//

  std::signal(SIGUSR1, handleQueueSignal);

  if (fork() == 0) {
    // Child process - Unshaped Receiver

    // This process should get a SIGHUP when it's parent (the shaped
    // sender) dies
    prctl(PR_SET_PDEATHSIG, SIGHUP);
    unshapedReceiver = new UnshapedReceiver{appName, maxClients, bindAddr,
                                            bindPort,
                                            checkResponseLoopInterval,
                                            logLevel, serverAddr};
  } else {
    // Parent Process - Shaped Sender
    sleep(2); // Wait for unshapedReceiver to initialise
    MsQuic = new MsQuicApi{};
    shapedSender = new ShapedSender{appName, maxClients, noiseMultiplier,
                                    sensitivity, maxDecisionSize,
                                    minDecisionSize, peer2Addr, peer2Port,
                                    DPCreditorLoopInterval,
                                    senderLoopInterval, logLevel};
    sleep(2);
    std::cout << "Peer is ready!" << std::endl;
  }

  // Wait for signal to exit
  waitForSignal();
}