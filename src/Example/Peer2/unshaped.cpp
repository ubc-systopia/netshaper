//
// Created by Rut Vora
//

// Peer 2 (server side middlebox) example for unidirectional stream

#include <sstream>
#include <algorithm>
#include "../../Modules/UnshapedTransciever/Sender.h"
#include "../../Modules/lamport_queue/queue/Cpp/LamportQueue.hpp"
#include "../../Modules/util/helpers.h"

std::string appName = "minesVPNPeer2";

UnshapedTransciever::Sender *unshapedSender;

std::unordered_map<queuePair, UnshapedTransciever::Sender *, queuePairHash>
    *queuesToSender;

// Sender to queue_in and queue_out. queue_out contains response received on
// the sending socket
std::unordered_map<UnshapedTransciever::Sender *, queuePair> *senderToQueues;


// Create numStreams number of shared memory and initialise Lamport Queues
// for each stream
inline void initialiseSHM(int numStreams) {
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

    // Data streams
    auto queue1 = new(shmAddr1) LamportQueue();
    auto queue2 = new(shmAddr2) LamportQueue();
    // TODO: Set different senders
    (*queuesToSender)[{queue1, queue2}] = nullptr;
  }
}

bool assignQueues(UnshapedTransciever::Sender *sender) {
  return std::ranges::any_of(*queuesToSender, [&](auto &iterator) {
    // iterator.first is queuePair, iterator.second is sender
    if (iterator.second == nullptr) {
      // No sender attached to this queue pair
      (*senderToQueues)[sender] = iterator.first;
      (*queuesToSender)[iterator.first] = sender;
      return true;
    }
    return false;
  });
}

// Push received response to queue dedicated to this sender
void onReceive(UnshapedTransciever::Sender *sender,
               uint8_t *buffer, size_t length) {
  (*senderToQueues)[sender].toShaped->push(buffer, length);
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

  queuesToSender =
      new std::unordered_map<queuePair,
          UnshapedTransciever::Sender *, queuePairHash>(numStreams);

  senderToQueues =
      new std::unordered_map<UnshapedTransciever::Sender *, queuePair>();

  initialiseSHM(numStreams);

  // Connect to remote Host and port to forward data
  unshapedSender = new UnshapedTransciever::Sender{remoteHost, remotePort,
                                                   onReceive};
  assignQueues(unshapedSender);

  // Dummy infinite loop that sends all received data to the single
  // unshapedSender for testing purpose only
  // TODO: Replace this with a proper per-end-host sending mechanism
  while (true) {
    usleep(100000);
    for (auto &iterator: *queuesToSender) {
      auto size = iterator.first.fromShaped->size();
      if (size > 0) {
        std::cout << "Got data in queue: " << iterator.first.fromShaped <<
                  std::endl;
        auto buffer = (uint8_t *) malloc(size);
        iterator.first.fromShaped->pop(buffer, size);
//        iterator.second->sendData(buffer, size);
        // TODO: Replace the line below with the line above
        unshapedSender->sendData(buffer, size);
      }

    }
  }
}