//
// Created by Rut Vora
//

#ifndef MINESVPN_HELPERS_H
#define MINESVPN_HELPERS_H

#include "../../Modules/lamport_queue/queue/Cpp/LamportQueue.hpp"
#include <csignal>
#include <cstdarg>

struct queuePair {
  LamportQueue *fromShaped;
  LamportQueue *toShaped;

  bool operator==(const queuePair &pair2) const {
    return fromShaped == pair2.fromShaped && toShaped == pair2.toShaped;
  }
};

// Simple hash function for queuePair to use it as a key in std::unordered_map
struct queuePairHash {
  std::size_t operator()(const queuePair &pair) const {
    return std::hash<LamportQueue *>()(pair.fromShaped) ^
           std::hash<LamportQueue *>()(pair.toShaped);
  }
};


enum StreamType {
  Control, Dummy, Data
};
struct controlMessage {
  uint64_t streamID;
  enum StreamType streamType;
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
