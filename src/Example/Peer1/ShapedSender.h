//
// Created by Rut Vora
//

#ifndef MINESVPN_SHAPED_SENDER_H
#define MINESVPN_SHAPED_SENDER_H

#include <thread>
#include <unordered_map>
#include <csignal>
#include "../../Modules/QUICWrapper/Sender.h"
#include "../../Modules/lamport_queue/queue/Cpp/LamportQueue.hpp"
#include "../util/helpers.h"

using namespace helpers;

class ShapedSender {
private:
  std::string appName;
  const logLevels logLevel;
  std::mutex logWriter;

  QUIC::Sender *shapedSender;

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

  MsQuicStream *dummyStream;
  MsQuicStream *controlStream;
  // Control and Dummy stream IDs
  QUIC_UINT62 dummyStreamID;
  QUIC_UINT62 controlStreamID;

  NoiseGenerator *noiseGenerator;
  // Max data that can be sent out now
  std::atomic<size_t> sendingCredit;


/**
 * @brief Find a queue pair by the ID of it's "toShaped" queue
 * @param queueID The ID of the "toShaped" queue to find
 * @return The QueuePair this ID belongs to
 */
  QueuePair findQueuesByID(uint64_t queueID);

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
  void handleControlMessage(uint8_t *buffer, size_t length);

  void log(logLevels level, const std::string &log);

public:
  /**
   * @brief Constructor for the Shaped Sender
   * @param appName The name of this application. Used as a key to initialise
   * shared memory with the unshaped process
   * @param maxClients The maximum number of TCP flows to support
   * @param epsilon The privacy budget (lower means more privacy but also
   * more overhead)
   * @param sensitivity The max "distance" between 2 queues that we want to
   * hide
   * @param peer2IP The IP address of the other middlebox
   * @param peer2Port The port of the other middlebox
   * @param idleTimeout The time (in milliseconds) after which an idle
   * connection between the middleboxes will be terminated
   * @param DPCreditorLoopInterval The interval (in microseconds) with which
   * the DP Creditor will credit the tokens
   * @param senderLoopInterval The interval (in microseconds) with which the
   * sender will read the tokens and send the shaped data
   */
  ShapedSender(std::string &appName, int maxClients,
               double epsilon = 0.01, double sensitivity = 100,
               const std::string &peer2IP = "localhost",
               uint16_t peer2Port = 4567,
               uint64_t idleTimeout = 100000,
               __useconds_t DPCreditorLoopInterval = 50000,
               __useconds_t senderLoopInterval = 50000);

  void sendDummy(size_t dummySize);

/**
 * @brief Send data to the receiving middleBox
 * @param dataSize The number of bytes to send out
 */
  void sendData(size_t dataSize);

/**
 * @brief Handle the queue status change signal sent by the unshaped process
 * @param signal The signal that was received
 */
  void handleQueueSignal(int signum);

};


#endif //MINESVPN_SHAPED_SENDER_H
