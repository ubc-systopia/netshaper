//
// Created by Rut Vora
//

#ifndef MINESVPN_HELPERS_H
#define MINESVPN_HELPERS_H

#include "../../Modules/lamport_queue/queue/Cpp/LamportQueue.hpp"
#include "../../Modules/Common.h"
#include <csignal>
#include <cstdarg>
#include <unistd.h>
#include <mutex>

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
  uint64_t streamID;
  enum StreamType streamType;
  enum connectionStatus connStatus;
  char srcIP[16];
  char srcPort[6];
  char destIP[16];
  char destPort[6];
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


#endif //MINESVPN_HELPERS_H
