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
#include <shared_mutex>


using namespace helpers;

class ShapedClient {
private:
  std::string appName;
  const logLevels logLevel;
  std::mutex logWriter;
  __useconds_t unshapedResponseLoopInterval;

  QUIC::Client *shapedClient;

// We fix the number of streams beforehand to avoid side-channels caused by
// the additional size of the stream header.
// Note: For completely correct information, each QUIC Frame should contain a
// fixed number of streams. MsQuic does NOT do this out of the box, and the
// current code does not implement it either.
  int maxClients;

  std::unordered_map<QueuePair, MsQuicStream *, QueuePairHash> *queuesToStream;
  std::unordered_map<MsQuicStream *, QueuePair> *streamToQueues;
  std::unordered_map<uint64_t, connectionStatus> *pendingSignal;

  class SignalInfo *sigInfo;

  std::mutex readLock;
  std::mutex writeLock;
  // Map lock is required as a param in the senderLoop. Otherwise, the map in
  // the client never changes and hence no locking is required.
  std::shared_mutex mapLock;

  MsQuicStream *dummyStream;
  MsQuicStream *controlStream;

  NoiseGenerator *noiseGenerator;


/**
 * @brief Find a queue pair by the ID of it's "toShaped" queue
 * @param queueID The ID of the "toShaped" queue to find
 * @return The QueuePair this ID belongs to
 */
  QueuePair findQueuesByID(uint64_t queueID);

  /**
   * @brief Find a stream by it's ID
   * @param ID The ID to look for
   * @return The stream pointer corresponding to that ID
   */
  MsQuicStream *findStreamByID(QUIC_UINT62 ID);


/**
 * @brief Create numStreams number of shared memory streams and initialise
 * Lamport Queues for each stream
 */
  inline void initialiseSHM();

/**
 * @brief Signal the shaped process on change of queue status
 * @param queueID The ID of the queue whose status has changed
 * @param connStatus The changed status of the given queue
 */
  void signalUnshapedProcess(uint64_t ID, connectionStatus connStatus);

/**
 * @brief Function that is called when a response is received
 * @param stream The stream on which the response was received
 * @param buffer The buffer where the response is stored
 * @param length The length of the received response
 */
  void onResponse(MsQuicStream *stream, uint8_t *buffer, size_t length);

/**
 * @brief Starts the control stream
 */
  inline void startControlStream();

/**
 * @brief Starts the dummy stream
 */
  inline void startDummyStream();

  /**
   * @brief Handle received control messages
   * @param buffer The buffer which stores the control messages
   * @param length Length of the buffer
   */
  void handleControlMessages(uint8_t *buffer, size_t length);

  /**
   * @brief Log the comments passed by various functions
   * @param level The level of the comment passed by the function
   * @param log The string containing the actual log
   */
  void log(logLevels level, const std::string &log);

public:
  /**
   * @brief Constructor for the Shaped Client
   * @param appName The name of this application. Used as a key to initialise
   * shared memory with the unshaped process
   * @param maxClients The maximum number of TCP flows to support
   * @param noiseMultiplier The privacy budget (lower means more privacy but also
   * more overhead)
   * @param sensitivity The max "distance" between 2 queues that we want to
   * hide
   * @param maxDecisionSize The maximum decision that the DP Decision
   * algorithm should generate
   * @param minDecisionSize The minimum decision that the DP Decision
   * algorithm should generate
   * @param peer2IP The IP address of the other middlebox
   * @param peer2Port The port of the other middlebox
   * @param DPCreditorLoopInterval The interval (in microseconds) with which
   * the DP Creditor will credit the tokens
   * @param sendingLoopInterval The interval (in microseconds) with which the
   * sending loop will read the tokens and send the shaped data
   * @param unshapedResponseLoopInterval The interval (in microseconds) with which the
   * queues will be read from the unshaped side (used for optimising wait
   * time when queues are full)
   * @param logLevel The level of logging you want (NOTE: For DEBUG logs, the
   * code has to be compiled with the flag DEBUGGING)
   * @param strategy The sending strategy when the DP decision interval >= 2
   * * sending interval. Can be UNIFORM or BURST
   * @param idleTimeout The time (in milliseconds) after which an idle
   * connection between the middleboxes will be terminated
   */
  ShapedClient(std::string &appName, int maxClients,
               double noiseMultiplier = 38, double sensitivity = 500000,
               uint64_t maxDecisionSize = 500000, uint64_t minDecisionSize = 0,
               const std::string &peer2IP = "localhost",
               uint16_t peer2Port = 4567,
               __useconds_t DPCreditorLoopInterval = 50000,
               __useconds_t sendingLoopInterval = 50000,
               __useconds_t unshapedResponseLoopInterval = 50000,
               logLevels logLevel = WARNING, sendingStrategy strategy = BURST,
               uint64_t idleTimeout = 100000);

  /**
   * @brief Send dummy of given size on the dummy stream
   * @param dummySize The #bytes to send
   * @return The prepared buffer (stream, buffer and size)
   */
  PreparedBuffer prepareDummy(size_t dummySize);

/**
 * @brief Send data to the receiving middleBox
 * @param dataSize The number of bytes to send out
 * @return Vector containing the prepared buffer (stream, buffer and size)
 */
  std::vector<PreparedBuffer> prepareData(size_t dataSize);

/**
 * @brief Handle the queue status change signal sent by the unshaped process
 * @param signal The signal that was received
 */
  void handleQueueSignal(int signum);

};


#endif //MINESVPN_SHAPED_CLIENT_H
