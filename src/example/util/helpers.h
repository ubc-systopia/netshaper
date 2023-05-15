//
// Created by Rut Vora
//

#ifndef MINESVPN_HELPERS_H
#define MINESVPN_HELPERS_H

#include "../../modules/lamport_queue/queue/Cpp/LamportQueue.hpp"
#include "../../modules/Common.h"
#include "msquic.hpp"
#include "../../modules/shaper/NoiseGenerator.h"
#include <csignal>
#include <cstdarg>
#include <unistd.h>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace helpers {
  /**
   * @brief Stores the queue pair (toShaped and fromShaped)
   */
  struct QueuePair {
    LamportQueue *fromShaped;
    LamportQueue *toShaped;

    bool operator==(const QueuePair &pair2) const {
      return fromShaped == pair2.fromShaped && toShaped == pair2.toShaped;
    }
  };

// Simple hash function for QueuePair to use it as a key in std::unordered_map
  struct QueuePairHash {
    std::size_t operator()(const QueuePair &pair) const {
      return std::hash<LamportQueue *>()(pair.fromShaped) ^
             std::hash<LamportQueue *>()(pair.toShaped);
    }
  };

  /**
   * @brief Class that stores signal information (which queue has a new
   * client or which queue's client disconnected
   */
  class SignalInfo {
  private:
    LamportQueue signalQueueToShaped{UINT64_MAX};
    LamportQueue signalQueueFromShaped{UINT64_MAX};

  public:
    pid_t unshaped;
    pid_t shaped;
    enum Direction {
      toShaped, fromShaped
    };

    struct queueInfo {
      uint64_t queueID; // The ID of the queue
      enum connectionStatus connStatus;
    };

    /**
     * @brief Dequeue the signal (SYN or FIN) from given direction (to or
     * from shaped)
     * @param direction The direction of the signal
     * @param info The signal and the queue associated with it
     * @return
     */
    bool dequeue(Direction direction, SignalInfo::queueInfo &info);

    /**
     * @brief Enqueue the signal (SYN or FIN) in given direction (to or from
     * shaped)
     * @param direction The direction of the signal
     * @param info The signal and the queue associated with it
     * @return
     */
    ssize_t enqueue(Direction direction, queueInfo &info);
  };


  /**
   * @brief Types of stream we support
   */
  enum StreamType {
    Control, Dummy, Data
  };

  /**
   * @brief The control message sent from one peer to the other peer
   */
  struct ControlMessage {
    uint64_t streamID{QUIC_UINT62_MAX};
    enum StreamType streamType{};
    enum connectionStatus connStatus{ONGOING};
    addressPair addrPair{};
  };

  struct PreparedBuffer {
    MsQuicStream *stream = nullptr;
    uint8_t *buffer = nullptr;
    size_t length = 0;
  };

/**
 * @brief Add given signal to the signal set
 * @param set The signal set to add the signal in
 * @param numSignals The number of signal that are to be added
 * @param ... The signals to be added
 */
  void addSignal(sigset_t *set, int numSignals, ...);

/**
 * @brief Waits for signal and then processes it
 * @param isShapedProcess Used to identify whether the process is the shaped
 * or the unshaped process (only used when compiled with RECORD_STATS) to
 * print/save the relevant statistics
 */
  void waitForSignal(bool isShapedProcess);

  /**
   * @brief Initialise Shared Memory in the given process
   * @param numStreams The number of streams supported (sets the size of SHM)
   * @param appName The unique key to use when creating/attaching to SHM
   * @param markForDeletion Whether the SHM should be marked for deletion
   * (deletes when all attached processes exit)
   * @return pointer to the shared memory (uint8_t * is used so that C++
   * allows pointer arithmetic later)
   */
  uint8_t *initialiseSHM(int numStreams, std::string &appName,
                         bool markForDeletion = false);

  /**
   * @brief Get the total data available to be sent out. We currently assume
   * only one end server
   * @param queuesToStream The unordered map to iterate over
   * @return The aggregated size of all queues
   */
  inline size_t getAggregatedQueueSize(std::unordered_map<QueuePair,
      MsQuicStream *,
      QueuePairHash> *queuesToStream) {
    size_t aggregatedSize = 0;
    for (auto &iterator: *queuesToStream) {
      if (!iterator.first.toShaped->inUse) continue;
      aggregatedSize += iterator.first.toShaped->size();
    }
    return aggregatedSize;
  }

  /**
   * @brief DP Decision function (runs in a separate thread at decisionInterval interval)
   * @param queuesToStream The map containing all queues to get the
   * aggregated size of
   * @param noiseGenerator The configured noise generator instance
   * @param sendDummy The function to call when the decision is made to send
   * dummy bytes
   * @param sendData The function to call when the decision is made to send
   * actual data
   * @param sendingInterval The interval with which this loop should iterate
   * @param decisionInterval The interval with which this loop will run
   * @param strategy The sending strategy (when decisionInterval >= 2 *
   * sendingInterval). Can be "BURST" or "UNIFORM"
   * @param mapLock The mapLock shared mutex used for locking the
   * queuesToStream map
   */
  [[noreturn]]
  void shaperLoop(std::unordered_map<QueuePair, MsQuicStream *,
      QueuePairHash> *queuesToStream,
                  NoiseGenerator *noiseGenerator,
                  const std::function<PreparedBuffer(size_t)>
                  &prepareDummy,
                  const std::function<std::vector<PreparedBuffer>(size_t)>
                  &prepareData,
                  const std::function<void(MsQuicStream *, uint8_t *, size_t)>
                  &placeInQuicQueues,
                  __useconds_t sendingInterval, __useconds_t decisionInterval,
                  sendingStrategy strategy, std::shared_mutex &mapLock);
}
#endif //MINESVPN_HELPERS_H
