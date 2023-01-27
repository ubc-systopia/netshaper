//
// Created by Rut Vora
//

// Peer 1 (client side middle box) example for unidirectional stream

#include <algorithm>
#include <thread>
#include <chrono>
#include "../../Modules/UnshapedTransciever/Receiver.h"
#include "../../Modules/lamport_queue/queue/Cpp/LamportQueue.hpp"
#include "../util/helpers.h"

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

/**
 * @brief assign a new queue for a new client
 * @param fromSocket The socket number of the new client
 * @param clientAddress The address:port of the new client
 * @return true if queue was assigned successfully
 */
inline bool assignQueue(int fromSocket, std::string &clientAddress) {
  // Find an unused queue and map it
  return std::ranges::any_of(*queuesToSocket, [&](auto &iterator) {
    // iterator.first is queuePair, iterator.second is socket
    if (iterator.second == 0) {
      // No socket attached to this queue pair
      (*socketToQueues)[fromSocket] = iterator.first;
      (*queuesToSocket)[iterator.first] = fromSocket;
      // Set client of queue to current (new) client
      auto address = clientAddress.substr(0, clientAddress.find(':'));
      auto port = clientAddress.substr(address.size() + 1);
      std::strcpy(iterator.first.fromShaped->clientAddress, address.c_str());
      std::strcpy(iterator.first.fromShaped->clientPort, port.c_str());
      std::strcpy(iterator.first.toShaped->clientAddress, address.c_str());
      std::strcpy(iterator.first.toShaped->clientPort, port.c_str());

      theOnlySocket = fromSocket; // TODO: Remove this
      return true;
    }
    return false;
  });
}

/**
 * @brief onReceive function for received Data
 * @param fromSocket The socket on which the data was received
 * @param clientAddress The address of the client from which the data was
 * received
 * @param buffer The buffer in which the received data was put
 * @param length The length of the received data
 * @return true if data was successfully pushed to assigned queue
 */
ssize_t receivedUnshapedData(int fromSocket, std::string &clientAddress,
                             uint8_t *buffer, size_t length) {
  if ((*socketToQueues)[fromSocket].toShaped == nullptr) {
    if (!assignQueue(fromSocket, clientAddress))
      std::cerr << "More clients than expected!" << std::endl;
  }

  // TODO: Check if queue has enough space before pushing to queue
  // TODO: Send an ACK back only after pushing!
  (*socketToQueues)[fromSocket].toShaped->push(buffer, length);
  return 0;
}

/**
 * @brief Create numStreams number of shared memory streams and initialise
 * Lamport Queues for each stream
 */
inline void initialiseSHM() {
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
    // Initialise a queue class at that shared memory and put it in the maps
    auto queue1 =
        new(shmAddr + (i * sizeof(class LamportQueue))) LamportQueue();
    auto queue2 =
        new(shmAddr + ((i + 1) * sizeof(class LamportQueue))) LamportQueue();
    (*queuesToSocket)[{queue1, queue2}] = 0;
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