//
// Created by Rut Vora
//

#ifndef MINESVPN_UNSHAPED_SENDER_H
#define MINESVPN_UNSHAPED_SENDER_H

#include <thread>
#include <algorithm>
#include "../../modules/tcp_wrapper/Sender.h"
#include "../../modules/lamport_queue/queue/Cpp/LamportQueue.hpp"
#include "../util/helpers.h"

using namespace helpers;

class UnshapedSender {
private:
  std::string appName;
  const logLevels logLevel;
  std::unordered_map<QueuePair, TCP::Sender *, QueuePairHash> *queuesToSender;
  std::unordered_map<uint64_t, connectionStatus> *pendingSignal;
  std::mutex logWriter;
  __useconds_t shapedReceiverLoopInterval;

  // Sender to queue_in and queue_out. queue_out contains response received on
  // the sending socket
  std::unordered_map<TCP::Sender *, QueuePair> *senderToQueues;

  class SignalInfo *sigInfo;

  std::mutex readLock;
  std::mutex writeLock;

  /**
 * @brief Create numStreams number of shared memory streams and initialise
 * Lamport Queues for each stream
 */
  inline void initialiseSHM(int numStreams);

// Push received response to queue dedicated to this sender
  /**
 * @brief Handle responses received on the sockets
 * @param sender The UnshapedSender (socket) on which the response was received
 * @param buffer The buffer where the response is stored
 * @param length The length of the response
 */
  void onResponse(TCP::Sender *sender,
                  uint8_t *buffer, size_t length, connectionStatus connStatus);

  inline void eraseMapping(TCP::Sender *sender);

  /**
 * @brief Find a queue pair by the ID of it's "fromShaped" queue
 * @param queueID The ID of the "fromShaped" queue
 * @return The QueuePair this ID belongs to
 */
  QueuePair findQueuesByID(uint64_t queueID);

  /**
 * @brief Check queues for data periodically and send it to corresponding socket
 * @param interval The interval at which the queues are checked
 */
  [[noreturn]] void checkQueuesForData(__useconds_t interval);

  /**
   * @brief Signal the shaped process
   * @param queueID The queue to send the signal about
   * @param connStatus The connection status (should be FIN)
   */
  void signalShapedProcess(uint64_t queueID, connectionStatus connStatus);

  void log(logLevels level, const std::string &log);

public:

  explicit UnshapedSender(std::string appName, int maxPeers = 1,
                          int maxStreamsPerPeer = 10,
                          __useconds_t checkQueuesInterval = 50000,
                          __useconds_t shapedReceiverLoopInterval = 50000,
                          logLevels logLevel = WARNING);

  /**
* @brief Handle signal from the shaped process regarding a new client
* @param signum The signal number of the signal (== SIGUSR1)
*/
  void handleQueueSignal(int signum);
};


#endif //MINESVPN_UNSHAPED_SENDER_H
