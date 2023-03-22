//
// Created by Rut Vora
//

#ifndef MINESVPN_HELPERS_H
#define MINESVPN_HELPERS_H

#include "../../Modules/lamport_queue/queue/Cpp/LamportQueue.hpp"
#include "../../Modules/Common.h"
#include "msquic.hpp"
#include "../../Modules/shaper/NoiseGenerator.h"
#include <csignal>
#include <cstdarg>
#include <unistd.h>
#include <mutex>
#include <unordered_map>

namespace helpers {
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

    bool dequeue(Direction direction, SignalInfo::queueInfo &info);

    ssize_t enqueue(Direction direction, queueInfo &info);
  };


  enum StreamType {
    Control, Dummy, Data
  };
  struct ControlMessage {
    uint64_t streamID{QUIC_UINT62_MAX};
    enum StreamType streamType{};
    enum connectionStatus connStatus{ONGOING};
    addressPair addrPair{};
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
 */
  void waitForSignal();

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
   * only one receiver
   * @param queuesToStream The unordered map to iterate over
   * @return The aggregated size of all queues
   */
  inline size_t getAggregatedQueueSize(std::unordered_map<QueuePair,
      MsQuicStream *,
      QueuePairHash> *queuesToStream) {
    size_t aggregatedSize = 0;
    for (auto &iterator: *queuesToStream) {
      aggregatedSize += iterator.first.toShaped->size();
    }
    return aggregatedSize;
  }

  /**
   * @brief DP Decision function (runs in a separate thread at decisionInterval interval)
   * @param sendingCredit The atomic variable to add to
   * @param queuesToStream The map containing all queues to get the
   * aggregated size of
   * @param noiseGenerator The configured noise generator instance
   * @param decisionInterval The interval with which this loop will run
   */
  [[noreturn]] void DPCreditor(std::atomic<size_t> *sendingCredit,
                               std::unordered_map<QueuePair, MsQuicStream *,
                                   QueuePairHash> *queuesToStream,
                               NoiseGenerator *noiseGenerator,
                               __useconds_t decisionInterval);

  /**
   * @brief Loop which sends Shaped Data at sendingInterval
   * @param sendingCredit The atomic variable that contains the available
   * sending credit
   * @param queuesToStream The map containing all queues
   * @param sendDummy The function to call when the decision is made to send
   * dummy bytes
   * @param sendData The function to call when the decision is made to send
   * actual data
   * @param sendingInterval The interval with which this loop should iterate
   */
  [[noreturn]]
  void sendShapedData(std::atomic<size_t> *sendingCredit,
                      std::unordered_map<QueuePair, MsQuicStream *,
                          QueuePairHash> *queuesToStream,
                      const std::function<void(size_t)> &sendDummy,
                      const std::function<void(size_t)> &sendData,
                      __useconds_t sendingInterval);
}
#endif //MINESVPN_HELPERS_H
