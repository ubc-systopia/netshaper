//
// Created by Rut Vora
//

#ifndef MINESVPN_UNSHAPED_CLIENT_H
#define MINESVPN_UNSHAPED_CLIENT_H

#include <thread>
#include <algorithm>
#include "../../modules/tcp_wrapper/Client.h"
#include "../../modules/lamport_queue/queue/Cpp/LamportQueue.hpp"
#include "../util/helpers.h"
#include "../util/config.h"
#include "../util/Unshaped.h"

using namespace helpers;

class UnshapedClient : Unshaped {
private:
  std::unordered_map<QueuePair, TCP::Client *, QueuePairHash> *queuesToClient;

  // Client to queue_in and queue_out. queue_out contains response received on
  // the sending socket
  std::unordered_map<TCP::Client *, QueuePair> *clientToQueues;


  /**
 * @brief Handle responses received on the sockets
 * @param client The UnshapedClient (socket) on which the response was received
 * @param buffer The buffer where the response is stored
 * @param length The length of the response
 */
  void onResponse(TCP::Client *client,
                  uint8_t *buffer, size_t length, connectionStatus connStatus);

  /**
   * @brief Erase the mapping of the given client once both sides are done
   * @param client The client whose mapping has to be erased
   */
  inline void eraseMapping(TCP::Client *client);

  /**
 * @brief Find a queue pair by the ID of it's "fromShaped" queue
 * @param queueID The ID of the "fromShaped" queue
 * @return The QueuePair this ID belongs to
 */
  QueuePair findQueuesByID(uint64_t queueID);

  inline void initialiseSHM(int numStreams, size_t queueSize) override;

  [[noreturn]] void checkQueuesForData(__useconds_t interval) override;

  void
  updateConnectionStatus(uint64_t queueID,
                         connectionStatus connStatus) override;

  void log(logLevels level, const std::string &log) override;

public:

  /**
   * @brief Constructor for the UnshapedClient
   * @param appName The name of this application. Used as a key to initialise
   * shared memory with the shaped process
   * @param maxPeers The max number of peers that will be expected
   * @param maxStreamsPerPeer The max number of streams permitted to be
   * received from each peer
   * @param shapedServerLoopInterval The interval with which the shaped
   * component will send out data (used for optimising wait time when queues
   * are full)
   * @param logLevel The level of logs required
   * @param config The configuration for this instance
   */
  explicit UnshapedClient(config::Peer2Config &peer2Config);

  [[noreturn]] void getUpdatedConnectionStatus() override;
};


#endif //MINESVPN_UNSHAPED_CLIENT_H
