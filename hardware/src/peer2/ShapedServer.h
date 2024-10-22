//
// Created by Rut Vora
//

#ifndef MINESVPN_SHAPED_SERVER_H
#define MINESVPN_SHAPED_SERVER_H

#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <queue>
#include <shared_mutex>
#include "../modules/quic_wrapper/Server.h"
#include "../modules/lamport_queue/Cpp/LamportQueue.hpp"
#include "../util/helpers.h"
#include "../modules/shaper/NoiseGenerator.h"
#include "../util/config.h"
#include "../util/Shaped.h"

using namespace helpers;

class ShapedServer : Shaped {
private:
  QUIC::Server *shapedServer;
  std::queue<QueuePair> *unassignedQueues;

  // Map that stores streamIDs for which client information is received (but
  // the stream has not yet begun).
  std::unordered_map<QUIC_UINT62, struct ControlMessage> streamIDtoCtrlMsg;

  // dummyStreamID is needed because ctrlMsg of dummyStream is received
  // before the dummy stream begins.
  QUIC_UINT62 dummyStreamID;

/**
 * @brief Signal the shaped process on change of queue status
 * @param queueID The ID of the queue whose status has changed
 * @param connStatus The changed status of the given queue
 */
  void updateConnectionStatus(uint64_t queueID, connectionStatus connStatus)
  override;

/**
 * @brief Copies the client and server info from the control message to the
 * queues
 * @param queues The queues to copy the data to
 * @param ctrlMsg The control message to copy the data from
 */
  static void copyClientInfo(QueuePair queues, struct ControlMessage *ctrlMsg);

/**
 * @brief assign a new queue for a new client
 * @param stream The new stream (representing a new client)
 * @return true if queue was assigned successfully
 */
  inline bool assignQueues(MsQuicStream *stream);

  /**
   * @brief Erase mapping once the stream finishes sending
   * @param stream The stream pointer to erase the mapping of
   */
  inline void eraseMapping(MsQuicStream *stream);

  void initialiseSHM(int numStreams, size_t queueSize) override;

  MsQuicStream *findStreamByID(QUIC_UINT62 ID) override;

  void handleControlMessages(MsQuicStream *ctrlStream,
                             uint8_t *buffer, size_t length) override;

  void receivedShapedData(MsQuicStream *stream, uint8_t *buffer, size_t
  length) override;

  PreparedBuffer prepareDummy(size_t dummySize) override;

  std::vector<PreparedBuffer> prepareData(size_t dataSize) override;

  void log(logLevels level, const std::string &log) override;

public:

  /**
   * @brief Constructor for shapedServer
   * @param peer2Config The config struct that configures this instance
   */
  explicit ShapedServer(config::Peer2Config &peer2Config);

  [[noreturn]] void getUpdatedConnectionStatus() override;
};


#endif //MINESVPN_SHAPED_SERVER_H
