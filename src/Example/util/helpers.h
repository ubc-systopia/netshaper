//
// Created by Rut Vora
//

#ifndef MINESVPN_HELPERS_H
#define MINESVPN_HELPERS_H

#include "../../Modules/lamport_queue/queue/Cpp/LamportQueue.hpp"

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


#endif //MINESVPN_HELPERS_H
