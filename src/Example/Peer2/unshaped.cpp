//
// Created by Rut Vora
//

// Peer 2 (server side middlebox) example for unidirectional stream

#include <sstream>
#include <algorithm>
#include "../../Modules/UnshapedTransciever/Sender.h"
#include "../../Modules/lamport_queue/queue/Cpp/LamportQueue.hpp"
#include "../util/helpers.h"

std::string appName = "minesVPNPeer2";

UnshapedTransciever::Sender *unshapedSender;

std::unordered_map<queuePair, UnshapedTransciever::Sender *, queuePairHash>
    *queuesToSender;

// Sender to queue_in and queue_out. queue_out contains response received on
// the sending socket
std::unordered_map<UnshapedTransciever::Sender *, queuePair> *senderToQueues;


/**
 * @brief Create numStreams number of shared memory streams and initialise
 * Lamport Queues for each stream
 */
inline void initialiseSHM(int numStreams) {
  int shmId = shmget((int) std::hash<std::string>()(appName),
                     numStreams * 2 * sizeof(class LamportQueue),
                     IPC_CREAT | 0644);
  if (shmId < 0) {
    std::cerr << "Failed to create shared memory!" << std::endl;
    exit(1);
  }
  auto shmAddr = static_cast<uint8_t *>(shmat(shmId, nullptr, 0));
  if (shmAddr == (void *) -1) {
    std::cerr << "Failed to attach shared memory!" << std::endl;
    exit(1);
  }
  for (int i = 0; i < numStreams * 2; i += 2) {
    auto queue1 =
        new(shmAddr + (i * sizeof(class LamportQueue))) LamportQueue();
    auto queue2 =
        new(shmAddr + ((i + 1) * sizeof(class LamportQueue))) LamportQueue();
    // TODO: Set different senders
    (*queuesToSender)[{queue1, queue2}] = nullptr;
  }
}

/**
 * @brief assign a new queue for a new client
 * @param sender The instance of Sender to assign
 * @return true if queue was assigned successfully
 */
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