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

using namespace helpers;

class UnshapedClient {
private:
  std::string appName;
  const logLevels logLevel;
  std::unordered_map<QueuePair, TCP::Client *, QueuePairHash> *queuesToClient;
  std::unordered_map<uint64_t, connectionStatus> *pendingSignal;
  std::mutex logWriter;
  __useconds_t checkQueuesInterval;
  __useconds_t shapedServerLoopInterval;

  // Client to queue_in and queue_out. queue_out contains response received on
  // the sending socket
  std::unordered_map<TCP::Client *, QueuePair> *clientToQueues;

  class SignalInfo *sigInfo;

  std::mutex readLock;
  std::mutex writeLock;

  /**
 * @brief Create numStreams number of shared memory streams and initialise
 * Lamport Queues for each stream
 */
  inline void initialiseSHM(int numStreams);

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

  /**
   * @brief Thread that reads the queue and forwards the data
   * @param queues The queuePair to check the data from
   * @param client The client instance to send the data via
   * @param interval The interval with which this loop should run
   */
  void forwardData(QueuePair queues, TCP::Client *client,
                   __useconds_t interval);

  /**
   * @brief Signal the shaped process
   * @param queueID The queue to send the signal about
   * @param connStatus The connection status (should be FIN)
   */
  void signalShapedProcess(uint64_t queueID, connectionStatus connStatus);

  /**
   * @brief Log the comments passed by various functions
   * @param level The level of the comment passed by the function
   * @param log The string containing the actual log
   */
  void log(logLevels level, const std::string &log);

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
  UnshapedClient(std::string appName, int maxPeers,
                 int maxStreamsPerPeer,
                 logLevels logLevel,
                 __useconds_t shapedServerLoopInterval,
                 config::UnshapedClient &config);

  /**
* @brief Handle signal from the shaped process regarding a new client
* @param signum The signal number of the signal (== SIGUSR1)
*/
  void handleQueueSignal(int signum);
};


#endif //MINESVPN_UNSHAPED_CLIENT_H
