//
// Created by Rut Vora
//

// Peer 2 (server side middlebox) example for unidirectional stream

#include <sstream>
#include "UnshapedTransciever/Sender.h"
#include "lamport_queue/queue/Cpp/LamportQueue.hpp"

std::string appName = "minesVPNPeer2";

UnshapedTransciever::Sender *unshapedSender;

std::unordered_map<LamportQueue *, UnshapedTransciever::Sender *>
    *queueToSender;

// Create numStreams number of shared memory and initialise Lamport Queues
// for each stream
inline void initialiseSHM(int numStreams) {
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

    // Data streams
    auto queue = new(shmAddr) LamportQueue();
    (*queueToSender)[queue] = unshapedSender;
  }
}

int main() {
  std::string remoteHost;
  int remotePort;
  int numStreams; // Max number of streams each client can initiate

  // Start listening (unshaped data)
  std::cout << "Enter the IP address of the remote Host you want to proxy " <<
            "to:" << std::endl;
  std::cin >> remoteHost;

  std::cout << "Enter the port of the remote Host you want to proxy " <<
            "to:" << std::endl;
  std::cin >> remotePort;

  std::cout << "Enter the maximum number of streams each client can initiate "
               "(does not include dummy streams): "
            << std::endl;
  std::cin >> numStreams;

  queueToSender = new std::unordered_map<LamportQueue *,
      UnshapedTransciever::Sender *>(numStreams);

  // Connect to remote Host and port to forward data
  unshapedSender = new UnshapedTransciever::Sender{remoteHost, remotePort};

  initialiseSHM(numStreams);

  while (true) {
    usleep(100000);
    for (auto &iterator: *queueToSender) {
      auto size = iterator.first->size();
      if (size > 0) {
        auto buffer = (uint8_t *) malloc(size);
        iterator.first->pop(buffer, size);
        iterator.second->sendData(buffer, size);
      }
    }
  }
}