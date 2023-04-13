//
// Created by Rut Vora
//

#include "UnshapedSender.h"
#include "ShapedReceiver.h"
#include <sys/prctl.h>
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

NLOHMANN_JSON_SERIALIZE_ENUM(logLevels, {
  { DEBUG, "DEBUG" },
  { WARNING, "WARNING" },
  { ERROR, "ERROR" },
})

UnshapedSender *unshapedSender = nullptr;
ShapedReceiver *shapedReceiver = nullptr;
// Load the API table. Necessary before any calls to MsQuic
// It is defined as an extern const in "msquic.hpp"
// This needs to be here (on the heap)
const MsQuicApi *MsQuic;

void handleQueueSignal(int signum) {
  if (unshapedSender != nullptr && shapedReceiver == nullptr) {
    unshapedSender->handleQueueSignal(signum);
  } else if (unshapedSender == nullptr && shapedReceiver != nullptr) {
    shapedReceiver->handleQueueSignal(signum);
  } else {
    std::cerr << "Peer2: Issue with handling queue signal!" << std::endl;
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
  auto maxStreamsPerPeer =
      static_cast<json>(config.value("maxStreamsPerPeer",
                                     40)).get<int>();

  json shapedReceiverConfig = config.value("shapedReceiver",
                                           json::parse(R"({})"));
  json unshapedSenderConfig = config.value("unshapedSender",
                                           json::parse(R"({})"));

  // Load shapedReceiverConfig
  std::string serverCert = shapedReceiverConfig.value("serverCert",
                                                      "server.cert");
  std::string serverKey = shapedReceiverConfig.value("serverKey", "server.key");
  auto listeningPort =
      static_cast<json>(shapedReceiverConfig.value("listeningPort",
                                                   4567)).get<uint16_t>();
  auto noiseMultiplier =
      static_cast<json>(shapedReceiverConfig.value("noiseMultiplier",
                                                   38)).get<double>();
  auto sensitivity =
      static_cast<json>(shapedReceiverConfig.value("sensitivity",
                                                   500000)).get<double>();
  auto maxDecisionSize =
      static_cast<json>(shapedReceiverConfig.value("maxDecisionSize",
                                                   500000)).get<uint64_t>();
  auto minDecisionSize =
      static_cast<json>(shapedReceiverConfig.value("minDecisionSize",
                                                   0)).get<uint64_t>();
  std::string appName = shapedReceiverConfig.value("appName", "minesVPNPeer1");
  auto DPCreditorLoopInterval =
      static_cast<json>(shapedReceiverConfig.value("DPCreditorLoopInterval",
                                                   50000)).get<__useconds_t>();
  auto senderLoopInterval =
      static_cast<json>(shapedReceiverConfig.value("senderLoopInterval",
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

  // Load unshapedSenderConfig
  auto checkQueuesInterval =
      static_cast<json>(unshapedSenderConfig.value(
          "checkQueuesInterval", 50000)).get<uint16_t>();

//  std::cout << "Enter the maximum number of streams per peer that should be "
//               "supported:"
//               " " << std::endl;
//  std::cin >> maxStreamsPerPeer;
//
//  std::string appName = "minesVPNPeer2";

  std::signal(SIGUSR1, handleQueueSignal);

  if (fork() == 0) {
    // Child process - Unshaped Sender

    // This process should get a SIGHUP when it's parent (the shaped
    // receiver) dies
    prctl(PR_SET_PDEATHSIG, SIGHUP);

    unshapedSender = new UnshapedSender{appName, 1, maxStreamsPerPeer,
                                        checkQueuesInterval,
                                        senderLoopInterval, logLevel};
  } else {
    // Parent Process - Shaped Receiver
    sleep(2); // Wait for unshapedSender to initialise
    MsQuic = new MsQuicApi{};
    shapedReceiver = new ShapedReceiver{appName, serverCert, serverKey,
                                        1, maxStreamsPerPeer, listeningPort,
                                        noiseMultiplier, sensitivity,
                                        maxDecisionSize, minDecisionSize,
                                        DPCreditorLoopInterval,
                                        senderLoopInterval,
                                        checkQueuesInterval, logLevel};
    sleep(1);
    std::cout << "Peer is ready!" << std::endl;
  }

  // Wait for signal to exit
  waitForSignal();
}