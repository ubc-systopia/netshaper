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
#include "../util/config.h"

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
 * @return The prepared buffer (stream, buffer and size)
 */
  PreparedBuffer prepareDummy(size_t dummySize);

/**
 * @brief Send data to the other middlebox
 * @param dataSize The number of bytes to send to the other middlebox
 * @return Vector containing the prepared buffer (stream, buffer and size)
 */
  std::vector<PreparedBuffer> prepareData(size_t dataSize);

/**
 * @brief Log the comments passed by various functions
 * @param level The level of the comment passed by the function
 * @param log The string containing the actual log
 */
  void log(logLevels level, const std::string &log);

public:

  /**
   * @brief Constructor for shapedServer
   * @param appName The name of this application. Used as a key to initialise
   * shared memory with the unshaped process
   * @param maxPeers The maximum number of peers that should connect to this
   * middlebox (more connections will be rejected)
   * @param maxStreamsPerPeer The maximum number of TCP flows to support
   * @param logLevel The level of logging you want (NOTE: For DEBUG logs, the
   * code has to be compiled with the flag DEBUGGING)
   * @param unshapedClientLoopInterval The interval (in microseconds) with which
   * the queues will be read from the unshaped side (used for optimising wait
   * time when queues are full)
   * @param config The config struct that configures this instance
   */
  ShapedServer(std::string appName, int maxPeers, int maxStreamsPerPeer,
               logLevels logLevel, __useconds_t unshapedClientLoopInterval,
               const config::ShapedServer &config);

  /**
 * @brief Handle the queue status change signal sent by the unshaped process
 * @param signal The signal that was received
 */
  void handleQueueSignal(int signum);
};


#endif //MINESVPN_SHAPED_SERVER_H
