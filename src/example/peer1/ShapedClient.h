//
// Created by Rut Vora
//

#ifndef MINESVPN_SHAPED_CLIENT_H
#define MINESVPN_SHAPED_CLIENT_H

#include <thread>
#include <unordered_map>
#include <csignal>
#include "../../modules/quic_wrapper/Client.h"
#include "../../modules/lamport_queue/queue/Cpp/LamportQueue.hpp"
#include "../util/helpers.h"
#include "../../modules/shaper/NoiseGenerator.h"
#include "../util/config.h"
#include "../util/Shaped.h"
#include <shared_mutex>


using namespace helpers;

class ShapedClient : Shaped {
private:
  QUIC::Client *shapedClient;
  LamportQueue controlMessageQueue{INT_MAX};

  /**
 * @brief Find a queue pair by the ID of it's "toShaped" queue
 * @param queueID The ID of the "toShaped" queue to find
 * @return The QueuePair this ID belongs to
 */
  QueuePair findQueuesByID(uint64_t queueID);


  /**
 * @brief Starts the control stream
 */
  inline void startControlStream();

  /**
 * @brief Starts the dummy stream
 */
  inline void startDummyStream();

  MsQuicStream *findStreamByID(QUIC_UINT62 ID) override;

  void initialiseSHM(int maxClients) override;

  void
  updateConnectionStatus(uint64_t ID, connectionStatus connStatus) override;

  void receivedShapedData(MsQuicStream *stream, uint8_t *buffer, size_t
  length) override;

  void handleControlMessages(MsQuicStream *ctrlStream,
                             uint8_t *buffer, size_t length) override;

  void log(logLevels level, const std::string &log) override;

public:
  /**
   * @brief Constructor for the Shaped Client
   * @param appName The name of this application. Used as a key to initialise
   * shared memory with the unshaped process
   * @param maxClients The maximum number of TCP flows to support
   * @param logLevel The level of logging you want (NOTE: For DEBUG logs, the
   * code has to be compiled with the flag DEBUGGING)
   * @param unshapedResponseLoopInterval The interval (in microseconds) with which the
   * queues will be read from the unshaped side (used for optimising wait
   * time when queues are full)
   * @param config The configuration for this instance
   */
  ShapedClient(std::string &appName, int maxClients,
               logLevels logLevel, __useconds_t unshapedResponseLoopInterval,
               config::ShapedClient &config);

  PreparedBuffer prepareDummy(size_t dummySize) override;

  std::vector<PreparedBuffer> prepareData(size_t dataSize) override;

  [[noreturn]] void getUpdatedConnectionStatus() override;

};


#endif //MINESVPN_SHAPED_CLIENT_H
