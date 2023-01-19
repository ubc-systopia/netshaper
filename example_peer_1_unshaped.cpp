//
// Created by Rut Vora
//

// Peer 1 (client side middle box) example for unidirectional stream
#include <sstream>
#include <csignal>
#include <cstdarg>
#include <algorithm>
#include <thread>
#include <chrono>
#include "UnshapedTransciever/Receiver.h"
#include "lamport_queue/queue/Cpp/LamportQueue.hpp"
#include "helpers.h"

std::string appName = "minesVPNPeer1";

// We fix the number of streams beforehand to avoid side-channels caused by
// the additional size of the stream header.
// Note: For completely correct information, each QUIC Frame should contain a
// fixed number of streams. MsQuic does NOT do this out of the box, and the
// current code does not implement it either.
int numStreams;

std::unordered_map<int, queuePair> *socketToQueues;
std::unordered_map<queuePair, int, queuePairHash> *queuesToSocket;

UnshapedTransciever::Receiver *unshapedReceiver;

//Temporary single client socket
// TODO: Replace this with proper socket management for multiple end-hosts
int theOnlySocket = -1;

// Testing loop that sends all responses to the only available socket
// TODO: Use proper socket management to send response to correct socket.
//  This requires that the response is received on the correct stream and
//  then put in the correct queue
[[noreturn]] void receivedResponse(__useconds_t interval) {
  while (true) {
    std::this_thread::sleep_for(std::chrono::microseconds(interval));
    for (auto &iterator: *queuesToSocket) {
      auto size = iterator.first.fromShaped->size();
      if (size > 0) {
        std::cout << "Got data in queue: " << iterator.first.fromShaped <<
                  std::endl;
        auto buffer = (uint8_t *) malloc(size);
        iterator.first.fromShaped->pop(buffer, size);
        unshapedReceiver->sendData(theOnlySocket, buffer, size);
      }
    }
  }
}

inline bool assignQueue(int fromSocket) {
  // Find an unused queue and map it
  return std::ranges::any_of(*queuesToSocket, [&](auto &iterator) {
    // iterator.first is queuePair, iterator.second is socket
    if (iterator.second == 0) {
      // No socket attached to this queue pair
      (*socketToQueues)[fromSocket] = iterator.first;
      (*queuesToSocket)[iterator.first] = fromSocket;
      theOnlySocket = fromSocket; // TODO: Remove this
      return true;
    }
    return false;
  });
}

// onReceive function for received Data
ssize_t receivedUnshapedData(int fromSocket, uint8_t *buffer, size_t length) {
  if ((*socketToQueues)[fromSocket].toShaped == nullptr) {
    if (!assignQueue(fromSocket))
      std::cerr << "More clients than expected!" << std::endl;
  }

  // TODO: Check if queue has enough space before pushing to queue
  // TODO: Send an ACK back only after pushing!
  (*socketToQueues)[fromSocket].toShaped->push(buffer, length);
  return 0;
}


// Create numStreams number of shared memory and initialise Lamport Queues
// for each stream
inline void initialiseSHM() {
  for (int i = 0; i < numStreams * 2; i += 2) {
    // String stream used to create keys for sharedMemory
    std::stringstream ss1, ss2;
    ss1 << appName << i;
    ss2 << appName << i + 1;

    // Create a shared memory
    int shmId1 = shmget((int) std::hash<std::string>()(ss1.str()),
                        sizeof(class LamportQueue), IPC_CREAT | 0644);
    int shmId2 = shmget((int) std::hash<std::string>()(ss2.str()),
                        sizeof(class LamportQueue), IPC_CREAT | 0644);
    if (shmId1 < 0 || shmId2 < 0) {
      std::cerr << "Failed to create shared memory!" << std::endl;
      exit(1);
    }

    // Attach to the given shared memory
    void *shmAddr1 = shmat(shmId1, nullptr, 0);
    void *shmAddr2 = shmat(shmId2, nullptr, 0);
    if (shmAddr1 == (void *) -1 || shmAddr2 == (void *) -1) {
      std::cerr << "Failed to attach shared memory!" << std::endl;
      exit(1);
    }

    // Initialise a queue class at that shared memory and put it in the maps
    auto queue1 = new(shmAddr1) LamportQueue();
    auto queue2 = new(shmAddr2) LamportQueue();
    (*queuesToSocket)[{queue1, queue2}] = 0;
  }
}

void addSignal(sigset_t *set, int numSignals, ...) {
  va_list args;
  va_start(args, numSignals);
  for (int i = 0; i < numSignals; i++) {
    sigaddset(set, va_arg(args, int));
  }

}

void waitForSignal() {
  sigset_t set;
  int sig;
  int ret_val;
  sigemptyset(&set);

  addSignal(&set, 6, SIGINT, SIGKILL, SIGTERM, SIGABRT, SIGSTOP,
            SIGTSTP);
  sigprocmask(SIG_BLOCK, &set, nullptr);

  ret_val = sigwait(&set, &sig);
  if (ret_val == -1)
    perror("The signal wait failed\n");
  else {
    if (sigismember(&set, sig)) {
      std::cout << "\nExiting with signal " << sig << std::endl;
      exit(0);
    }
  }
}

int main() {
  std::cout << "Enter the maximum number of clients that should be supported:"
               " " << std::endl;
  std::cin >> numStreams;
  socketToQueues = new std::unordered_map<int, queuePair>(numStreams);
  queuesToSocket = new std::unordered_map<queuePair,
      int, queuePairHash>(numStreams);

  initialiseSHM();

  // Start listening for unshaped traffic
  unshapedReceiver = new UnshapedTransciever::Receiver{"", 8000,
                                                       receivedUnshapedData};
  unshapedReceiver->startListening();

  std::thread responseLoop(receivedResponse, 100000);
  responseLoop.detach();

  // Wait for signal to exit
  waitForSignal();
}