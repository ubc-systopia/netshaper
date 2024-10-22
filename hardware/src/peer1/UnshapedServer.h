//
// Created by Rut Vora
//

#ifndef MINESVPN_UNSHAPED_SERVER_H
#define MINESVPN_UNSHAPED_SERVER_H

#include <algorithm>
#include <thread>
#include <chrono>
#include <queue>
#include <shared_mutex>
#include "../modules/tcp_wrapper/Server.h"
#include "../modules/lamport_queue/Cpp/LamportQueue.hpp"
#include "../modules/Common.h"
#include "../util/helpers.h"
#include "../peer2/UnshapedClient.h"

using namespace helpers;

class UnshapedServer : public Unshaped {
private:
  config::Peer1Config peer1Config;
  std::string serverAddr;

  std::unordered_map<int, QueuePair> *socketToQueues;
  std::unordered_map<QueuePair, int, QueuePairHash> *queuesToSocket;
  std::queue<QueuePair> *unassignedQueues;

  std::shared_mutex mapLock;

  TCP::Server *unshapedServer;


  /**
 * @brief assign a new queue for a new client
 * @param clientSocket The socket number of the new client
 * @param clientAddress The address:port of the new client
 * @param serverAddress The address of the server the client wants to connect to
 * @return true if queue was assigned successfully
 */
  inline QueuePair assignQueue(int clientSocket, std::string &clientAddress,
                               std::string serverAddress = "127.0.0.1:5555");

  /**
   * @brief Erase the mapping of the socket to the queues
   * @param socket The socket whose mapping has to be erased
   */
  inline void eraseMapping(int socket);

  /**
 * @brief Signal the shaped process on change of queue status
 * @param queueID The ID of the queue whose status has changed
 * @param connStatus The changed status of the given queue
 */
  void updateConnectionStatus(uint64_t queueID, connectionStatus connStatus)
  override;

  /**
 * @brief serverOnReceive function for received Data
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

  [[noreturn]] void checkQueuesForData(__useconds_t interval,
                                       size_t queueSize) override;

  void initialiseSHM(int maxClients, size_t queueSize) override;

  void log(logLevels level, const std::string &log) override;

public:
  /**
   * @brief Constructor for the Unshaped Server
   * @param peer1Config The config struct that configures this instance
   */
  explicit UnshapedServer(config::Peer1Config &peer1Config);

  [[noreturn]] void getUpdatedConnectionStatus() override;

};


#endif //MINESVPN_UNSHAPED_SERVER_H
