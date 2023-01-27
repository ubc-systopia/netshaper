//
// Created by Rut Vora
//

// Peer 2 (server side middlebox) example for unidirectional stream

#include <thread>
#include <algorithm>
#include "../../Modules/UnshapedTransciever/Sender.h"
#include "../../Modules/lamport_queue/queue/Cpp/LamportQueue.hpp"
#include "../util/helpers.h"

std::string appName = "minesVPNPeer2";

std::unordered_map<queuePair, UnshapedTransciever::Sender *, queuePairHash>
    *queuesToSender;

// Sender to queue_in and queue_out. queue_out contains response received on
// the sending socket
std::unordered_map<UnshapedTransciever::Sender *, queuePair> *senderToQueues;

struct queueSignal *queueSig;

/**
 * @brief Create numStreams number of shared memory streams and initialise
 * Lamport Queues for each stream
 */
inline void initialiseSHM(int numStreams) {
  size_t shmSize =
      sizeof(struct queueSignal) +
      numStreams * 2 * sizeof(class LamportQueue);

  int shmId = shmget((int) std::hash<std::string>()(appName),
                     shmSize,
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

  // The beginning of the SHM contains the queueSignal struct
  queueSig = reinterpret_cast<struct queueSignal *>(shmAddr);
  queueSig->unshaped = getpid();
  queueSig->ack = true;

  // The rest of the SHM contains the queues
  shmAddr += sizeof(struct queueSignal);
  for (int i = 0; i < numStreams * 2; i += 2) {
    auto queue1 =
        new(shmAddr + (i * sizeof(class LamportQueue))) LamportQueue(i);
    auto queue2 =
        new(shmAddr + ((i + 1) * sizeof(class LamportQueue)))
            LamportQueue(i + 1);
    (*queuesToSender)[{queue1, queue2}] = nullptr;
  }
}

// Push received response to queue dedicated to this sender
/**
 * @brief Handle responses received on the sockets
 * @param sender The UnshapedSender (socket) on which the response was received
 * @param buffer The buffer where the response is stored
 * @param length The length of the response
 */
void onReceive(UnshapedTransciever::Sender *sender,
               uint8_t *buffer, size_t length) {
  (*senderToQueues)[sender].toShaped->push(buffer, length);
}

/**
 * @brief Find a queue pair by the ID of it's "fromShaped" queue
 * @param queueID The ID of the "fromShaped" queue
 * @return The queuePair this ID belongs to
 */
queuePair findQueuesByID(uint64_t queueID) {
  for (auto &iterator: *queuesToSender) {
    if (iterator.first.fromShaped->queueID == queueID) {
      return iterator.first;
    }
  }
  return {nullptr, nullptr};
}

/**
 * @brief Handle signal from the shaped process regarding a new client
 * @param signum The signal number of the signal (== SIGUSR1)
 */
void handleQueueSignal(int signum) {
  if (signum == SIGUSR1) {
    auto queues = findQueuesByID(queueSig->queueID);
    if (queueSig->connStatus == NEW) {
      std::cout << "Peer2:Unshaped: New Connection" << std::endl;
      auto unshapedSender = new UnshapedTransciever::Sender{
          queues.fromShaped->serverAddress,
          std::stoi(queues.fromShaped->serverPort),
          onReceive};

      (*queuesToSender)[queues] = unshapedSender;
      (*senderToQueues)[unshapedSender] = queues;
    } else if (queueSig->connStatus == TERMINATED) {
      std::cout << "Peer2:Unshaped: Connection Terminated" << std::endl;
      (*senderToQueues).erase((*queuesToSender)[queues]);
      delete (*queuesToSender)[queues];
      (*queuesToSender)[queues] = nullptr;
    }
    queueSig->ack = true;
  }
}

/**
 * @brief Check queues for data periodically and send it to corresponding socket
 * @param interval The interval at which the queues are checked
 */
[[noreturn]] void checkQueuesForData(__useconds_t interval) {
  // TODO: Replace this with signalling from shaped side (would it be more
  //  efficient?)
  while (true) {
    std::this_thread::sleep_for(std::chrono::microseconds(interval));
//    usleep(100000);
    for (auto &iterator: *queuesToSender) {
      auto size = iterator.first.fromShaped->size();
      if (size > 0) {
        std::cout << "Got data in queue: " << iterator.first.fromShaped <<
                  std::endl;
        auto buffer = (uint8_t *) malloc(size);
        iterator.first.fromShaped->pop(buffer, size);
        while (iterator.second == nullptr);
        iterator.second->sendData(buffer, size);
      }
    }
  }
}

int main() {
  int numStreams; // Max number of streams each client can initiate

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

  std::signal(SIGUSR1, handleQueueSignal);

  std::thread checkQueuesLoop(checkQueuesForData, 50000);
  checkQueuesLoop.detach();

  // Wait for signal to exit
  waitForSignal();
}