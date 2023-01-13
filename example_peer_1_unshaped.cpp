//
// Created by Rut Vora
//

// Peer 1 (client side middle box) example for unidirectional stream
#include <sstream>
#include "UnshapedTransciever/Receiver.h"
#include "lamport_queue/queue/Cpp/LamportQueue.hpp"

std::string appName = "minesVPN";

UnshapedTransciever::Receiver *unshapedReceiver;
int clientSocket;

LamportQueue **queues;  // Receive buffer (queue) for each client

// onReceive function for received Data
ssize_t receivedUnshapedData(int fromSocket, uint8_t *buffer, size_t length) {
  clientSocket = fromSocket;
  // TODO: Check if queue has enough space before pushing to queue
  // TODO: Send an ACK back only after pushing!
  queues[0]->push(buffer, length);
  return 0;
}

int main() {
  // Initialise a fixed number of buffers/queues at the beginning (as shared
  // memory)
  int numStreams;
  std::cout << "Enter the maximum number of clients that should be supported:"
               " " << std::endl;
  std::cin >> numStreams;
  queues = (LamportQueue **) malloc(numStreams * sizeof(LamportQueue *));

  // Create numStreams number of shared memory and initialise Lamport Queues
  // for each stream
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

    // Initialise a queue class at that shared memory
    queues[i] = new(shmAddr) LamportQueue();
  }

  // Start listening for unshaped traffic
  unshapedReceiver = new UnshapedTransciever::Receiver{"", 8000,
                                                       receivedUnshapedData};
  unshapedReceiver->startListening();

  // Dummy blocking function
  std::string s;
  std::cin >> s;
}