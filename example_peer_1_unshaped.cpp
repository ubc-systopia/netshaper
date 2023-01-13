//
// Created by Rut Vora
//

// Peer 1 (client side middle box) example for unidirectional stream
#include <sstream>
#include "UnshapedTransciever/Receiver.h"
#include "lamport_queue/queue/Cpp/LamportQueue.hpp"

std::string appName = "minesVPN";

// We fix the number of streams beforehand to avoid side-channels caused by
// the additional size of the stream header.
// Note: For completely correct information, each QUIC Frame should contain a
// fixed number of streams. MsQuic does NOT do this out of the box, and the
// current code does not implement it either.
int numStreams;

std::unordered_map<int, LamportQueue *> *socketToQueue;
std::unordered_map<LamportQueue *, int> *queueToSocket;

UnshapedTransciever::Receiver *unshapedReceiver;

inline bool assignQueue(int fromSocket) {
  // Find an unused queue and map it
  for (auto &iterator: *queueToSocket) {
    if (iterator.second == 0) {
      // No socket attached to this queue
      (*socketToQueue)[fromSocket] = iterator.first;
      (*queueToSocket)[iterator.first] = fromSocket;
      return true;
    }
  }
  return false;
}

// onReceive function for received Data
ssize_t receivedUnshapedData(int fromSocket, uint8_t *buffer, size_t length) {
  if ((*socketToQueue)[fromSocket] == nullptr) {
    if (!assignQueue(fromSocket))
      std::cerr << "More clients than expected!" << std::endl;
  }

  // TODO: Check if queue has enough space before pushing to queue
  // TODO: Send an ACK back only after pushing!
  (*socketToQueue)[fromSocket]->push(buffer, length);
  return 0;
}


// Create numStreams number of shared memory and initialise Lamport Queues
// for each stream
inline void initialiseSHM() {
  for (int i = 0; i < numStreams; i++) {
    // String stream used to create keys for sharedMemory
    std::stringstream ss;
    ss << appName << i;

    // Create a shared memory
    int shmId = shmget((int) std::hash<std::string>()(ss.str()),
                       sizeof(class LamportQueue), IPC_CREAT | 0644);
    if (shmId < 0) {
      std::cerr << "Failed to create shared memory!" << std::endl;
      exit(1);
    }

    // Attach to the given shared memory
    void *shmAddr = shmat(shmId, nullptr, 0);
    if (shmAddr == (void *) -1) {
      std::cerr << "Failed to attach shared memory!" << std::endl;
      exit(1);
    }

    // Initialise a queue class at that shared memory and put it in the maps
    auto queue = new(shmAddr) LamportQueue();
    (*queueToSocket)[queue] = 0;
  }
}

int main() {
  std::cout << "Enter the maximum number of clients that should be supported:"
               " " << std::endl;
  std::cin >> numStreams;
  socketToQueue = new std::unordered_map<int, LamportQueue *>(numStreams);
  queueToSocket = new std::unordered_map<LamportQueue *, int>(numStreams);

  initialiseSHM();

  // Start listening for unshaped traffic
  unshapedReceiver = new UnshapedTransciever::Receiver{"", 8000,
                                                       receivedUnshapedData};
  unshapedReceiver->startListening();

  // Dummy blocking function
  std::string s;
  std::cin >> s;
}