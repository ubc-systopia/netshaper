//
// Created by Rut Vora
//

#ifndef MINESVPN_UNSHAPED_RECEIVER_H
#define MINESVPN_UNSHAPED_RECEIVER_H

#include <algorithm>
#include <thread>
#include <chrono>
#include "../../Modules/TCPWrapper/Receiver.h"
#include "../../Modules/lamport_queue/queue/Cpp/LamportQueue.hpp"
#include "../util/helpers.h"

using namespace helpers;

class UnshapedReceiver {
private:
  std::string appName;

// We fix the number of streams beforehand to avoid side-channels caused by
// the additional size of the stream header.
// Note: For completely correct information, each QUIC Frame should contain a
// fixed number of streams. MsQuic does NOT do this out of the box, and the
// current code does not implement it either.
  int maxClients;

  std::unordered_map<int, QueuePair> *socketToQueues;
  std::unordered_map<QueuePair, int, QueuePairHash> *queuesToSocket;

  class SignalInfo *sigInfo;

  std::mutex readLock;
  std::mutex writeLock;

  TCP::Receiver *unshapedReceiver;


  /**
 * @brief Read queues periodically and send the responses to the
 * corresponding sockets
 * @param interval The interval at which the queues will be checked
 */
  [[noreturn]] void receivedResponse(__useconds_t interval);

// TODO: Replace hard-coded value of serverAddress
  /**
 * @brief assign a new queue for a new client
 * @param fromSocket The socket number of the new client
 * @param clientAddress The address:port of the new client
 * @return true if queue was assigned successfully
 */
  inline bool assignQueue(int fromSocket, std::string &clientAddress,
                          std::string serverAddress = "127.0.0.1:5555");

  /**
 * @brief Find a queue pair by the ID of it's "toShaped" queue
 * @param queueID The ID of the "toShaped" queue to find
 * @return The QueuePair this ID belongs to
 */
  QueuePair findQueuesByID(uint64_t queueID);

  /**
 * @brief Signal the shaped process on change of queue status
 * @param queueID The ID of the queue whose status has changed
 * @param connStatus The changed status of the given queue
 */
  void signalShapedProcess(uint64_t queueID, connectionStatus connStatus);

  /**
 * @brief onReceive function for received Data
 * @param fromSocket The socket on which the data was received
 * @param clientAddress The address of the client from which the data was
 * received
 * @param buffer The buffer in which the received data was put
 * @param length The length of the received data
 * @param connStatus If it's a new or existing connection or if the
 * connection was just terminated
 * @return true if data was successfully pushed to assigned queue
 */
  bool receivedUnshapedData(int fromSocket, std::string &clientAddress,
                            uint8_t *buffer, size_t length, enum
                                connectionStatus connStatus);

  /**
 * @brief Create numStreams number of shared memory streams and initialise
 * Lamport Queues for each stream
 */
  inline void initialiseSHM();

public:
  /**
   * @brief Constructor for the Unshaped Sender
   * @param appName The name of this application. Used as a key to initialise
   * shared memory with the shaped process
   * @param maxClients The maximum number of TCP flows to support
   * @param bindAddr The address to listen to TCP traffic on
   * @param bindPort The port to listen to TCP traffic on
   * @param checkResponseInterval The interval with which to check the queues
   * containing data received from the other middlebox
   */
  UnshapedReceiver(std::string &appName, int maxClients,
                   std::string bindAddr = "", uint16_t bindPort = 8000,
                   __useconds_t checkResponseInterval = 100000);

  /**
 * @brief Handle the queue status change signal sent by the shaped process
 * @param signal The signal that was received
 */
  void handleQueueSignal(int signum);

};


#endif //MINESVPN_UNSHAPED_RECEIVER_H
