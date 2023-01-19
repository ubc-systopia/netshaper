//
// Created by Rut Vora
//

#ifndef MINESVPN_HELPERS_H
#define MINESVPN_HELPERS_H

#include "../lamport_queue/queue/Cpp/LamportQueue.hpp"

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


#endif //MINESVPN_HELPERS_H
