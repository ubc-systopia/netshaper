//
// Created by Rut Vora
//

// Peer 1 (client side middle box) example for unidirectional stream

#include <algorithm>
#include <thread>
#include <chrono>
#include "../../Modules/TCPWrapper/Receiver.h"
#include "../../Modules/lamport_queue/queue/Cpp/LamportQueue.hpp"
#include "../util/helpers.h"

std::string appName = "minesVPNPeer1";

// We fix the number of streams beforehand to avoid side-channels caused by
// the additional size of the stream header.
// Note: For completely correct information, each QUIC Frame should contain a
// fixed number of streams. MsQuic does NOT do this out of the box, and the
// current code does not implement it either.
static int numStreams;

std::unordered_map<int, QueuePair> *socketToQueues;
std::unordered_map<QueuePair, int, QueuePairHash> *queuesToSocket;

class SignalInfo *sigInfo;

static std::mutex readLock;
static std::mutex writeLock;

static TCP::Receiver *unshapedReceiver;

/**
 * @brief Read queues periodically and send the responses to the
 * corresponding sockets
 * @param interval The interval at which the queues will be checked
 */
[[noreturn]] void receivedResponse(__useconds_t interval) {
  while (true) {
    std::this_thread::sleep_for(std::chrono::microseconds(interval));
    for (auto &iterator: *queuesToSocket) {
      auto size = iterator.first.fromShaped->size();
      if (size > 0) {
        std::cout << "Peer1:Unshaped: Got data in queue: " <<
                  iterator.first.fromShaped << std::endl;
        auto buffer = reinterpret_cast<uint8_t *>(malloc(size));
        iterator.first.fromShaped->pop(buffer, size);
        auto sentBytes =
            unshapedReceiver->sendData(iterator.second, buffer, size);
        if (sentBytes == size) free(buffer);
      }
    }
  }
}

// TODO: Replace hard-coded value of serverAddress
/**
 * @brief assign a new queue for a new client
 * @param fromSocket The socket number of the new client
 * @param clientAddress The address:port of the new client
 * @return true if queue was assigned successfully
 */
inline bool assignQueue(int fromSocket, std::string &clientAddress,
                        std::string serverAddress = "127.0.0.1:5555") {
  // Find an unused queue and map it
  return std::ranges::any_of(*queuesToSocket, [&](auto &iterator) {
    // iterator.first is QueuePair, iterator.second is socket
    if (iterator.second == 0) {
      // No socket attached to this queue pair
      (*socketToQueues)[fromSocket] = iterator.first;
      (*queuesToSocket)[iterator.first] = fromSocket;
      // Set client of queue to the new client
      auto address = clientAddress.substr(0, clientAddress.find(':'));
      auto port = clientAddress.substr(address.size() + 1);
      QueuePair queues = iterator.first;
      std::strcpy(queues.fromShaped->clientAddress, address.c_str());
      std::strcpy(queues.fromShaped->clientPort, port.c_str());
      std::strcpy(queues.toShaped->clientAddress, address.c_str());
      std::strcpy(queues.toShaped->clientPort, port.c_str());

      // Set server of queue to given server
      address = serverAddress.substr(0, serverAddress.find(':'));
      port = serverAddress.substr((address.size() + 1));
      std::strcpy(queues.fromShaped->serverAddress, address.c_str());
      std::strcpy(queues.fromShaped->serverPort, port.c_str());
      std::strcpy(queues.toShaped->serverAddress, address.c_str());
      std::strcpy(queues.toShaped->serverPort, port.c_str());
      return true;
    }
    return false;
  });
}

/**
 * @brief Find a queue pair by the ID of it's "toShaped" queue
 * @param queueID The ID of the "toShaped" queue to find
 * @return The QueuePair this ID belongs to
 */
QueuePair findQueuesByID(uint64_t queueID) {
  for (auto &iterator: *queuesToSocket) {
    if (iterator.first.toShaped->queueID == queueID) {
      return iterator.first;
    }
  }
  return {nullptr, nullptr};
}

/**
 * @brief Signal the shaped process on change of queue status
 * @param queueID The ID of the queue whose status has changed
 * @param connStatus The changed status of the given queue
 */
void signalShapedProcess(uint64_t queueID, connectionStatus connStatus) {
  std::scoped_lock lock(writeLock);
  struct SignalInfo::queueInfo queueInfo{queueID, connStatus};
  sigInfo->enqueue(SignalInfo::toShaped, queueInfo);
  //TODO: Handle case when queue is full

  // Signal the other process (does not actually kill the shaped process)
  kill(sigInfo->shaped, SIGUSR1);
}

/**
 * @brief Handle the queue status change signal sent by the shaped process
 * @param signal The signal that was received
 */
void handleQueueSignal(int signum) {
  if (signum == SIGUSR1) {
    std::scoped_lock lock(readLock);
    struct SignalInfo::queueInfo queueInfo{};
    while (sigInfo->dequeue(SignalInfo::fromShaped, queueInfo)) {
      if (queueInfo.connStatus != TERMINATED)
        std::cerr << "Peer1:Unshaped: Wrong signal from Shaped process" <<
                  std::endl;
      auto queues = findQueuesByID(queueInfo.queueID);
      queues.toShaped->clear();
      queues.fromShaped->clear();
      (*socketToQueues).erase((*queuesToSocket)[queues]);
      (*queuesToSocket)[queues] = 0;
      std::cout << "Peer1:Unshaped: Mapping removed on termination" <<
                std::endl;
      queues.toShaped->markedForDeletion = false;
      queues.fromShaped->markedForDeletion = false;

    }
  }
}

/**
 * @brief onReceive function for received Data
 * @param fromSocket The socket on which the data was received
 * @param clientAddress The address of the client from which the data was
 * received
 * @param buffer The buffer in which the received data was put
 * @param length The length of the received data
 * @param connStatus If it's a new or existing connection or if the
 * connection was just terminated
 * @return true if data was successfully pushed to assigned queue
 */
bool receivedUnshapedData(int fromSocket, std::string &clientAddress,
                          uint8_t *buffer, size_t length, enum
                              connectionStatus connStatus) {

  switch (connStatus) {
    case NEW: {
      if (!assignQueue(fromSocket, clientAddress)) {
        std::cerr << "More clients than expected!" << std::endl;
        return false;
      }
      signalShapedProcess((*socketToQueues)[fromSocket].toShaped->queueID, NEW);
      return true;
    }
    case ONGOING: {
      // TODO: Send an ACK back only after pushing!
      auto toShaped = (*socketToQueues)[fromSocket].toShaped;
      while (toShaped->push(buffer, length) == -1) {
        std::cerr << "Queue for client " << toShaped->clientAddress
                  << ":" << toShaped->clientPort
                  << " is full. Waiting for it to be empty"
                  << std::endl;
        // Sleep for some time. For performance reasons, this is the same as
        // the interval with which DP Logic thread runs in Shaped component.
        std::this_thread::sleep_for(std::chrono::microseconds(500000));
      }
      return true;
    }
    case TERMINATED: {
      auto toShaped = (*socketToQueues)[fromSocket].toShaped;
      auto fromShaped = (*socketToQueues)[fromSocket].fromShaped;
      toShaped->markedForDeletion = true;
      fromShaped->markedForDeletion = true;
      signalShapedProcess(toShaped->queueID, TERMINATED);
      return true;
    }

    default:
      return false; // Just to satisfy the compiler
  }
}

/**
 * @brief Create numStreams number of shared memory streams and initialise
 * Lamport Queues for each stream
 */
inline void initialiseSHM() {
  size_t shmSize =
      sizeof(class SignalInfo) +
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

  // The beginning of the SHM contains the signalStruct struct
  sigInfo = new(shmAddr) SignalInfo{};
  sigInfo->unshaped = getpid();

  // The rest of the SHM contains the queues
  shmAddr += sizeof(class SignalInfo);
  for (int i = 0; i < numStreams * 2; i += 2) {
    // Initialise a queue class at that shared memory and put it in the maps
    auto queue1 =
        new(shmAddr + (i * sizeof(class LamportQueue))) LamportQueue(i);
    auto queue2 =
        new(shmAddr + ((i + 1) * sizeof(class LamportQueue)))
            LamportQueue(i + 1);
    (*queuesToSocket)[{queue1, queue2}] = 0;
  }
}

int main() {
  std::cout << "Enter the maximum number of clients that should be supported:"
               " " << std::endl;
  std::cin >> numStreams;
  socketToQueues = new std::unordered_map<int, QueuePair>(numStreams);
  queuesToSocket = new std::unordered_map<QueuePair,
      int, QueuePairHash>(numStreams);

  initialiseSHM();

  // Start listening for unshaped traffic
  unshapedReceiver = new TCP::Receiver{"", 8000,
                                       receivedUnshapedData};
  unshapedReceiver->startListening();

  std::thread responseLoop(receivedResponse, 100000);
  responseLoop.detach();

  std::signal(SIGUSR1, handleQueueSignal);

  // Wait for signal to exit
  waitForSignal();
}