//
// Created by Rut Vora
//

#ifndef MINESVPN_SHAPED_CLIENT_H
#define MINESVPN_SHAPED_CLIENT_H

#include <thread>
#include <unordered_map>
#include <csignal>
#include "../../modules/quic_wrapper/Client.h"
#include "../../modules/lamport_queue/Cpp/LamportQueue.hpp"
#include "../util/helpers.h"
#include "../../modules/shaper/NoiseGenerator.h"
#include "../util/config.h"
#include "../util/Shaped.h"
#include <shared_mutex>


using namespace helpers;

class ShapedClient : Shaped {
private:
  QUIC::Client *shapedClient;

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

  void initialiseSHM(int maxClients, size_t queueSize) override;

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
   * @param peer1Config The configuration for this instance
   */
  explicit ShapedClient(config::Peer1Config &peer1Config);

  PreparedBuffer prepareDummy(size_t dummySize) override;

  std::vector<PreparedBuffer> prepareData(size_t dataSize) override;

  [[noreturn]] void getUpdatedConnectionStatus() override;

};


#endif //MINESVPN_SHAPED_CLIENT_H
