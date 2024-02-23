//
// Created by Rut Vora
//

#ifndef MINESVPN_CONFIG_H
#define MINESVPN_CONFIG_H

#include "../modules/Common.h"
#include <string>
#include <nlohmann/json.hpp>

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

namespace config {

/**
   * @brief << operator overload for vectors
   * @tparam T The type that is contained in the vector
   * @param os The output stream
   * @param vec The vector to print from
   * @return The same output stream
   */
  template<class T>
  inline std::ostream &operator<<(std::ostream &os, const std::vector<T> &v) {
    os << "[";
    for (auto &elem: v) {
      os << elem << ", ";
    }
    os << "]";
    return os;
  }

  /**
   * @param bindAddr The address to listen to TCP traffic on
   * @param bindPort The port to listen to TCP traffic on
   * @param checkResponseInterval The interval with which to check the queues
   * containing data received from the other middlebox
   * @param serverAddr The server (on the other side of the 2nd middlebox)
   * you want to connect to
   * @param cores The cores on which this process should run
   */
  struct UnshapedServer {
    std::string bindAddr;
    uint16_t bindPort = 8000;
    __useconds_t checkQueuesInterval = 50000;
    std::string serverAddr = "localhost:5555";
    std::vector<int> cores{};
  };
  /**
   * @param peer2Addr The address of the other middlebox
   * @param peer2Port The port the other middlebox is listening on for shaped
   * traffic
   * @param noiseMultiplier The privacy budget (lower means more privacy but also
   * more overhead)
   * @param sensitivity The max "distance" between 2 queues that we want to
   * hide
   * @param maxDecisionSize The maximum decision that the DP Decision
   * algorithm should generate
   * @param minDecisionSize The minimum decision that the DP Decision
   * algorithm should generate
   * @param DPCreditorLoopInterval The interval (in microseconds) with which
   * the DP Creditor will credit the tokens
   * @param sendingLoopInterval The interval (in microseconds) with which the
   * sending loop will read the tokens and send the shaped data
   * @param strategy The sending strategy when the DP decision interval >= 2
   * * sending interval. Can be UNIFORM or BURST
   * @param idleTimeout The time (in milliseconds) after which an idle
   * connection between the middleboxes will be terminated
   * @param shaperCores The core/s on which the shaper thread should run
   * @param workerCores The core/s on which the QUIC worker thread/s should run
   */
  struct ShapedClient {
    std::string peer2Addr = "localhost";
    uint16_t peer2Port = 4567;
    double noiseMultiplier = 38;
    double sensitivity = 500000;
    uint64_t maxDecisionSize = 500000;
    uint64_t minDecisionSize = 0;
    __useconds_t DPCreditorLoopInterval = 50000;
    __useconds_t sendingLoopInterval = 50000;
    sendingStrategy strategy = BURST;
    uint64_t idleTimeout = 100000;
    std::vector<int> shaperCores{};
    std::vector<int> workerCores{};
  };
  /**
   * @param logLevel The level of logging required. For DEBUG, the program
   * has to be compiled with the DEBUGGING flag on
   * @param maxClients The maximum number of clients we will support
   * @param appName The name of this application instance. Used as key to
   * create the shared memory between the shaped and the unshaped processes
   */
  struct Peer1Config {
    logLevels logLevel = WARNING;
    int maxClients = 40;
    std::string appName = "minesVPNPeer1";
    size_t queueSize = 2097152;
    struct UnshapedServer unshapedServer;
    struct ShapedClient shapedClient;
  };

  /**
   * @param serverCert The SSL Certificate of the server (path to file)
   * @param serverKey The SSL key of the server (path to file)
   * @param listeningPort The port to listen to
   * @param noiseMultiplier The privacy budget (lower means more privacy but also
   * more overhead)
   * @param sensitivity The max "distance" between 2 queues that we want to
   * hide
   * @param maxDecisionSize The maximum decision that the DP Decision
   * algorithm should generate
   * @param minDecisionSize The minimum decision that the DP Decision
   * algorithm should generate
   * @param DPCreditorLoopInterval The interval (in microseconds) with which
   * the DP Creditor will credit the tokens
   * @param sendingLoopInterval The interval (in microseconds) with which the
   * sending loop will read the tokens and send the shaped data
   * @param strategy The sending strategy when the DP decision interval >= 2
   * * sending interval. Can be UNIFORM or BURST
   * @param idleTimeout The time (in milliseconds) after which an idle
   * connection between the middleboxes will be terminated
   * @param shaperCores The core/s on which the shaper thread should run
   * @param workerCores The core/s on which the QUIC worker thread/s should run
   */
  struct ShapedServer {
    std::string serverCert = "server.cert";
    std::string serverKey = "server.key";
    uint16_t listeningPort = 4567;
    double noiseMultiplier = 38;
    double sensitivity = 500000;
    uint64_t maxDecisionSize = 500000;
    uint64_t minDecisionSize = 0;
    __useconds_t DPCreditorLoopInterval = 50000;
    __useconds_t sendingLoopInterval = 50000;
    sendingStrategy strategy = BURST;
    uint64_t idleTimeout = 100000;
    std::vector<int> shaperCores{};
    std::vector<int> workerCores{};
  };
  /**
   * @param checkQueuesInterval The interval with which to check the queues
   * for data to be forwarded
   * @param cores The cores on which this process should run
   */
  struct UnshapedClient {
    __useconds_t checkQueuesInterval = 50000;
    std::vector<int> cores{};
  };
  /**
   * @param logLevel The level of logging required. For DEBUG, the program
   * has to be compiled with the DEBUGGING flag on
   * @param maxPeers The maximum number of peers we will support
   * @param maxStreamsPerPeer The maximum number of clients/streams each peer
   * on the other side supports
   * @param appName The name of this application instance. Used as key to
   * create the shared memory between the shaped and the unshaped processes
   */
  struct Peer2Config {
    logLevels logLevel = WARNING;
    int maxPeers = 1;
    int maxStreamsPerPeer = 40;
    std::string appName = "minesVPNPeer2";
    size_t queueSize = 2097152;
    struct ShapedServer shapedServer;
    struct UnshapedClient unshapedClient;
  };

  [[maybe_unused]] inline void from_json(const json &j, Peer1Config &config) {
    // Deserialize JSON values if present
    if (j.contains("logLevel")) {
      config.logLevel = j["logLevel"].get<logLevels>();
    }
    if (j.contains("maxClients")) {
      config.maxClients = j["maxClients"].get<int>();
    }
    if (j.contains("appName")) {
      config.appName = j["appName"].get<std::string>();
    }
    if (j.contains("queueSize")) {
      config.queueSize = j["queueSize"].get<size_t>();
    }
    if (j.contains("shapedClient")) {
      const auto &shapedClientJson = j["shapedClient"];
      if (shapedClientJson.contains("peer2Addr")) {
        config.shapedClient.peer2Addr = shapedClientJson["peer2Addr"].get<std::string>();
      }
      if (shapedClientJson.contains("peer2Port")) {
        config.shapedClient.peer2Port = shapedClientJson["peer2Port"]
            .get<uint16_t>();
      }
      if (shapedClientJson.contains("noiseMultiplier")) {
        config.shapedClient.noiseMultiplier =
            shapedClientJson["noiseMultiplier"].get<double>();
      }
      if (shapedClientJson.contains("sensitivity")) {
        config.shapedClient.sensitivity =
            shapedClientJson["sensitivity"].get<double>();
      }
      if (shapedClientJson.contains("maxDecisionSize")) {
        config.shapedClient.maxDecisionSize =
            shapedClientJson["maxDecisionSize"].get<uint64_t>();
      }
      if (shapedClientJson.contains("minDecisionSize")) {
        config.shapedClient.minDecisionSize =
            shapedClientJson["minDecisionSize"].get<uint64_t>();
      }
      if (shapedClientJson.contains("DPCreditorLoopInterval")) {
        config.shapedClient.DPCreditorLoopInterval =
            shapedClientJson["DPCreditorLoopInterval"].get<__useconds_t>();
      }
      if (shapedClientJson.contains("sendingLoopInterval")) {
        config.shapedClient.sendingLoopInterval =
            shapedClientJson["sendingLoopInterval"].get<__useconds_t>();
      }
      if (shapedClientJson.contains("sendingStrategy")) {
        config.shapedClient.strategy =
            shapedClientJson["sendingStrategy"].get<sendingStrategy>();
      }
      if (shapedClientJson.contains("shaperCores")) {
        config.shapedClient.shaperCores =
            shapedClientJson["shaperCores"].get<std::vector<int>>();
      }
      if (shapedClientJson.contains("workerCores")) {
        config.shapedClient.workerCores =
            shapedClientJson["workerCores"].get<std::vector<int>>();
      }
    }
    if (j.contains("unshapedServer")) {
      const auto &unshapedServerJson = j["unshapedServer"];
      if (unshapedServerJson.contains("bindAddr")) {
        config.unshapedServer.bindAddr =
            unshapedServerJson["bindAddr"].get<std::string>();
      }
      if (unshapedServerJson.contains("bindPort")) {
        config.unshapedServer.bindPort =
            unshapedServerJson["bindPort"].get<uint16_t>();
      }
      if (unshapedServerJson.contains("checkQueuesInterval")) {
        config.unshapedServer.checkQueuesInterval =
            unshapedServerJson["checkQueuesInterval"].get<__useconds_t>();
      }
      if (unshapedServerJson.contains("serverAddr")) {
        config.unshapedServer.serverAddr =
            unshapedServerJson["serverAddr"].get<std::string>();
      }
      if (unshapedServerJson.contains("cores")) {
        config.unshapedServer.cores =
            unshapedServerJson["cores"].get<std::vector<int>>();
      }
    }
  }

  [[maybe_unused]] inline void from_json(const json &j, Peer2Config &config) {
    // Deserialize JSON values if present
    if (j.contains("logLevel")) {
      config.logLevel = j["logLevel"].get<logLevels>();
    }
    if (j.contains("maxPeers")) {
      config.maxPeers = j["maxPeers"].get<int>();
    }
    if (j.contains("maxStreamsPerPeer")) {
      config.maxStreamsPerPeer = j["maxStreamsPerPeer"].get<int>();
    }
    if (j.contains("appName")) {
      config.appName = j["appName"].get<std::string>();
    }
    if (j.contains("queueSize")) {
      config.queueSize = j["queueSize"].get<size_t>();
    }
    if (j.contains("shapedServer")) {
      const auto &shapedServerJson = j["shapedServer"];
      if (shapedServerJson.contains("serverCert")) {
        config.shapedServer.serverCert =
            shapedServerJson["serverCert"].get<std::string>();
      }
      if (shapedServerJson.contains("serverKey")) {
        config.shapedServer.serverKey =
            shapedServerJson["serverKey"].get<std::string>();
      }
      if (shapedServerJson.contains("listeningPort")) {
        config.shapedServer.listeningPort =
            shapedServerJson["listeningPort"].get<uint16_t>();
      }
      if (shapedServerJson.contains("noiseMultiplier")) {
        config.shapedServer.noiseMultiplier =
            shapedServerJson["noiseMultiplier"].get<double>();
      }
      if (shapedServerJson.contains("sensitivity")) {
        config.shapedServer.sensitivity =
            shapedServerJson["sensitivity"].get<double>();
      }
      if (shapedServerJson.contains("maxDecisionSize")) {
        config.shapedServer.maxDecisionSize =
            shapedServerJson["maxDecisionSize"].get<uint64_t>();
      }
      if (shapedServerJson.contains("minDecisionSize")) {
        config.shapedServer.minDecisionSize =
            shapedServerJson["minDecisionSize"].get<uint64_t>();
      }
      if (shapedServerJson.contains("DPCreditorLoopInterval")) {
        config.shapedServer.DPCreditorLoopInterval =
            shapedServerJson["DPCreditorLoopInterval"].get<__useconds_t>();
      }
      if (shapedServerJson.contains("sendingLoopInterval")) {
        config.shapedServer.sendingLoopInterval =
            shapedServerJson["sendingLoopInterval"].get<__useconds_t>();
      }
      if (shapedServerJson.contains("sendingStrategy")) {
        config.shapedServer.strategy =
            shapedServerJson["sendingStrategy"].get<sendingStrategy>();
      }
      if (shapedServerJson.contains("shaperCores")) {
        config.shapedServer.shaperCores =
            shapedServerJson["shaperCores"].get<std::vector<int>>();
      }
      if (shapedServerJson.contains("workerCores")) {
        config.shapedServer.workerCores =
            shapedServerJson["workerCores"].get<std::vector<int>>();
      }
    }
    if (j.contains("unshapedClient")) {
      const auto &unshapedClientJson = j["unshapedClient"];
      if (unshapedClientJson.contains("checkQueuesInterval")) {
        config.unshapedClient.checkQueuesInterval =
            unshapedClientJson["checkQueuesInterval"].get<__useconds_t>();
      }
      if (unshapedClientJson.contains("cores")) {
        config.unshapedClient.cores =
            unshapedClientJson["cores"].get<std::vector<int>>();
      }
    }
  }

  inline std::ostream &
  operator<<(std::ostream &os, const UnshapedServer &unshapedServer) {
    os << "Bind Address: " << unshapedServer.bindAddr << "\n";
    os << "Bind Port: " << unshapedServer.bindPort << "\n";
    os << "Check Response Loop Interval: "
       << unshapedServer.checkQueuesInterval << "\n";
    os << "Server Address: " << unshapedServer.serverAddr << "\n";
    os << "Cores: " << unshapedServer.cores << "\n";
    return os;
  }

  inline std::ostream &
  operator<<(std::ostream &os, const ShapedClient &shapedClient) {
    os << "Peer2 Address: " << shapedClient.peer2Addr << "\n";
    os << "Peer2 Port: " << shapedClient.peer2Port << "\n";
    os << "Noise Multiplier: " << shapedClient.noiseMultiplier << "\n";
    os << "Sensitivity: " << shapedClient.sensitivity << "\n";
    os << "Max Decision Size: " << shapedClient.maxDecisionSize << "\n";
    os << "Min Decision Size: " << shapedClient.minDecisionSize << "\n";
    os << "DPCreditor Loop Interval: " << shapedClient.DPCreditorLoopInterval
       << "\n";
    os << "Sending Loop Interval: " << shapedClient.sendingLoopInterval
       << "\n";
    os << "Sending Strategy: " << shapedClient.strategy << "\n";
    os << "Idle Timeout: " << shapedClient.idleTimeout << "\n";
    os << "Shaper Cores: " << shapedClient.shaperCores << "\n";
    os << "Worker Cores: " << shapedClient.workerCores << "\n";
    return os;
  }

  inline std::ostream &
  operator<<(std::ostream &os, const Peer1Config &peer1Config) {
    os << "Log Level: " << peer1Config.logLevel << "\n";
    os << "Max Clients: " << peer1Config.maxClients << "\n";
    os << "App Name: " << peer1Config.appName << "\n";
    os << "Queue Size: " << peer1Config.queueSize << "\n";
    os << "\nUnshaped Server: \n" << peer1Config.unshapedServer << "\n";
    os << "\nShaped Client: \n" << peer1Config.shapedClient << "\n";
    return os;
  }

  inline std::ostream &
  operator<<(std::ostream &os, const ShapedServer &shapedServer) {
    os << "Server Cert: " << shapedServer.serverCert << "\n";
    os << "Server Key: " << shapedServer.serverKey << "\n";
    os << "Listening Port: " << shapedServer.listeningPort << "\n";
    os << "Noise Multiplier: " << shapedServer.noiseMultiplier << "\n";
    os << "Sensitivity: " << shapedServer.sensitivity << "\n";
    os << "Max Decision Size: " << shapedServer.maxDecisionSize << "\n";
    os << "Min Decision Size: " << shapedServer.minDecisionSize << "\n";
    os << "DPCreditor Loop Interval: " << shapedServer.DPCreditorLoopInterval
       << "\n";
    os << "Sending Loop Interval: " << shapedServer.sendingLoopInterval
       << "\n";
    os << "Sending Strategy: " << shapedServer.strategy << "\n";
    os << "Idle Timeout: " << shapedServer.idleTimeout << "\n";
    os << "Shaper Cores: " << shapedServer.shaperCores << "\n";
    os << "Worker Cores: " << shapedServer.workerCores << "\n";
    return os;
  }

  inline std::ostream &
  operator<<(std::ostream &os, const UnshapedClient &unshapedClient) {
    os << "Check Queues Interval: " << unshapedClient.checkQueuesInterval
       << "\n";
    os << "Cores: " << unshapedClient.cores << "\n";
    return os;
  }

  inline std::ostream &
  operator<<(std::ostream &os, const Peer2Config &peer2Config) {
    os << "Log Level: " << peer2Config.logLevel << "\n";
    os << "Max Peers: " << peer2Config.maxPeers << "\n";
    os << "Max Streams Per Peer: " << peer2Config.maxStreamsPerPeer
       << "\n";
    os << "App Name: " << peer2Config.appName << "\n";
    os << "Queue Size: " << peer2Config.queueSize << "\n";
    os << "\nUnshaped Client: \n" << peer2Config.unshapedClient << "\n";
    os << "\nShaped Server: \n" << peer2Config.shapedServer << "\n";
    return os;
  }
}
#endif //MINESVPN_CONFIG_H
