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
#include "../../modules/quic_wrapper/Server.h"
#include "../../modules/lamport_queue/queue/Cpp/LamportQueue.hpp"
#include "../util/helpers.h"
#include "../../modules/shaper/NoiseGenerator.h"

using namespace helpers;

class ShapedServer {
private:
  std::string appName;
  const logLevels logLevel;
  QUIC::Server *shapedServer;
  std::mutex logWriter;
  __useconds_t unshapedClientLoopInterval;

  std::unordered_map<MsQuicStream *, QueuePair> *streamToQueues;
  std::unordered_map<QueuePair, MsQuicStream *, QueuePairHash>
      *queuesToStream;
  std::unordered_map<uint64_t, connectionStatus> *pendingSignal;
  std::queue<QueuePair> *unassignedQueues;

  class SignalInfo *sigInfo;

  std::mutex writeLock;
  std::mutex readLock;
  std::shared_mutex mapLock;

  // Map that stores streamIDs for which client information is received (but
  // the stream has not yet begun).
  std::unordered_map<QUIC_UINT62, struct ControlMessage> streamIDtoCtrlMsg;

  MsQuicStream *controlStream;
  MsQuicStream *dummyStream;
  // dummyStreamID is needed because ctrlMsg of dummyStream is received
  // before the dummy stream begins.
  QUIC_UINT62 dummyStreamID;

  NoiseGenerator *noiseGenerator;


/**
 * @brief Create numStreams number of shared memory streams and initialise
 * Lamport Queues for each stream
 */
  inline void initialiseSHM(int numStreams);

/**
 * @brief Finds a stream by it's ID
 * @param ID The ID of the stream to match
 * @return The stream pointer of the stream the ID matches with, or nullptr
 */
  MsQuicStream *findStreamByID(QUIC_UINT62 ID);

/**
 * @brief Signal the shaped process on change of queue status
 * @param queueID The ID of the queue whose status has changed
 * @param connStatus The changed status of the given queue
 */
  void signalUnshapedProcess(uint64_t queueID, connectionStatus connStatus);

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

/**
 * @brief Handle receiving control messages from the other middleBox
 * @param ctrlStream The control stream this message was received on
 * @param buffer The buffer containing the messages
 * @param length The length of the buffer
 */
  void handleControlMessages(MsQuicStream *ctrlStream,
                             uint8_t *buffer, size_t length);

/**
 * @brief The function that is called when shaped data is received from the
 * other middleBox.
 * @param stream The stream on which the data was received
 * @param buffer The buffer where the received data is stored
 * @param length The length of the received data
 */
  void receivedShapedData(MsQuicStream *stream, uint8_t *buffer, size_t
  length);

/**
 * @brief Send dummy bytes on the dummy stream
 * @param dummySize The number of bytes to be sent out
 */
  void sendDummy(size_t dummySize);

/**
 * @brief Send data to the other middlebox
 * @param dataSize The number of bytes to send to the other middlebox
 * @return The number of bytes sent out
 */
  size_t sendData(size_t dataSize);

/**
 * @brief Log the comments passed by various functions
 * @param level The level of the comment passed by the function
 * @param log The string containing the actual log
 */
  void log(logLevels level, const std::string &log);

public:
  /**
   * @brief Constructor for ShapedServer
   * @param appName The name of this application. Used as a key to initialise
   * shared memory with the unshaped process
   * @param serverCert The SSL Certificate of the server (path to file)
   * @param serverKey The SSL key of the server (path to file)
   * @param maxPeers The maximum number of peers that should connect to this
   * middlebox (more connections will be rejected)
   * @param maxStreamsPerPeer The maximum number of TCP flows to support
   * @param bindPort The port to listen to
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
   * @param unshapedClientLoopInterval The interval (in microseconds) with which
   * the queues will be read from the unshaped side (used for optimising wait
   * time when queues are full)
   * @param logLevel The level of logging you want (NOTE: For DEBUG logs, the
   * code has to be compiled with the flag DEBUGGING)
   * @param strategy The sending strategy when the DP decision interval >= 2
   * * sending interval. Can be UNIFORM or BURST
   * @param idleTimeout The time (in milliseconds) after which an idle
   * connection between the middleboxes will be terminated
   */
  ShapedServer(std::string appName,
               const std::string &serverCert, const std::string &serverKey,
               int maxPeers = 1, int maxStreamsPerPeer = 10,
               uint16_t bindPort = 4567,
               double noiseMultiplier = 0.01, double sensitivity = 100,
               uint64_t maxDecisionSize = 500000,
               uint64_t minDecisionSize = 0,
               __useconds_t DPCreditorLoopInterval = 50000,
               __useconds_t sendingLoopInterval = 50000,
               __useconds_t unshapedClientLoopInterval = 50000,
               logLevels logLevel = WARNING, sendingStrategy strategy = BURST,
               uint64_t idleTimeout = 100000);

  /**
 * @brief Handle the queue status change signal sent by the unshaped process
 * @param signal The signal that was received
 */
  void handleQueueSignal(int signum);
};


#endif //MINESVPN_SHAPED_SERVER_H
