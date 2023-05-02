//
// Created by Rut Vora
//

#ifndef MINESVPN_SHAPED_RECEIVER_H
#define MINESVPN_SHAPED_RECEIVER_H

#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <queue>
#include <shared_mutex>
#include "../../modules/quic_wrapper/Receiver.h"
#include "../../modules/lamport_queue/queue/Cpp/LamportQueue.hpp"
#include "../util/helpers.h"
#include "../../modules/shaper/NoiseGenerator.h"

using namespace helpers;

class ShapedReceiver {
private:
  std::string appName;
  const logLevels logLevel;
  QUIC::Receiver *shapedReceiver;
  std::mutex logWriter;
  __useconds_t unshapedSenderLoopInterval;

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

  // The credit for sender such that it is the credit is maximum data it can send at any given time.
  std::atomic<size_t> sendingCredit;
  NoiseGenerator *noiseGenerator;


/**
 * @brief Create numStreams number of shared memory streams and initialise
 * Lamport Queues for each stream
 */
  inline void initialiseSHM(int numStreams);

/**
 * @brief Send response to the other middleBox
 * @param stream The stream to send the response on
 * @param data The buffer which holds the data to be sent
 * @param length The length of the data to be sent
 * @return
 */
  static bool sendResponse(MsQuicStream *stream, uint8_t *data, size_t length);

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

  inline void eraseMapping(MsQuicStream *stream);

/**
 * @brief Handle receiving control messages from the other middleBox
 * @param ctrlStream The control stream this message was received on
 * @param buffer The buffer containing the messages
 * @Param length The length of the buffer
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

  void log(logLevels level, const std::string &log);

public:
  /**
   * @brief Constructor for ShapedReceiver
   * @param appName The name of this application. Used as a key to initialise
   * shared memory with the unshaped process
   * @param maxStreamsPerPeer The maximum number of TCP flows to support
   * @param serverCert The SSL Certificate of the server (path to file)
   * @param serverKey The SSL key of the server (path to file)
   * @param bindPort The port to listen to
   * @param idleTimeout The time (in milliseconds) after which an idle
   * connection between the middleboxes will be terminated
   * @param noiseMultiplier The privacy budget (lower means more privacy but also
   * more overhead)
   * @param sensitivity The max "distance" between 2 queues that we want to
   * hide
   * @param DPCreditorLoopInterval The interval (in microseconds) with which
   * the DP Creditor will credit the tokens
   * @param senderLoopInterval The interval (in microseconds) with which the
   * sender will read the tokens and send the shaped data
   */
  ShapedReceiver(std::string appName,
                 const std::string &serverCert, const std::string &serverKey,
                 int maxPeers = 1, int maxStreamsPerPeer = 10,
                 uint16_t bindPort = 4567,
                 double noiseMultiplier = 0.01, double sensitivity = 100,
                 uint64_t maxDecisionSize = 500000,
                 uint64_t minDecisionSize = 0,
                 __useconds_t DPCreditorLoopInterval = 50000,
                 __useconds_t senderLoopInterval = 50000,
                 __useconds_t unshapedSenderLoopInterval = 50000,
                 logLevels logLevel = WARNING, sendingStrategy strategy = BURST,
                 uint64_t idleTimeout = 100000);

  /**
 * @brief Handle the queue status change signal sent by the unshaped process
 * @param signal The signal that was received
 */
  void handleQueueSignal(int signum);
};


#endif //MINESVPN_SHAPED_RECEIVER_H
