//
// Created by Rut Vora
//

// Peer 1 (client side middle box) example for unidirectional stream

#include <sstream>
#include "ShapedTransciever/Sender.h"
#include "lamport_queue/queue/Cpp/LamportQueue.hpp"

// Load the API table. Necessary before any calls to MsQuic
// It is defined as an extern const in "msquic.hpp"
// This needs to be here (on the heap)
const MsQuicApi *MsQuic = new MsQuicApi();

ShapedTransciever::Sender *shapedSender;
MsQuicStream *stream;

int *shmIds;
LamportQueue **queues;

std::string appName = "minesVPN";

ssize_t sendShapedData(size_t length) {
  size_t availableLength = queues[0]->size();
  if (availableLength == 0) return 0;

  size_t lengthToSend = length < availableLength ? length : availableLength;
  auto data = (uint8_t *) malloc(lengthToSend);
  queues[0]->pop(data, lengthToSend);
  return shapedSender->send(stream, lengthToSend, data);
}

int main() {
  // Initialise a fixed number of buffers/queues at the beginning (as shared
  // memory)
  int numStreams;
  std::cout << "Enter the maximum number of clients that should be supported:"
               " " << std::endl;
  std::cin >> numStreams;
  shmIds = (int *) malloc(numStreams * sizeof(int));
  queues = (LamportQueue **) malloc(numStreams * sizeof(LamportQueue *));

  // Create numStreams number of shared memory and initialise Lamport Queues
  // for each stream
  for (int i = 0; i < numStreams; i++) {
    // String stream used to create keys for sharedMemory
    std::stringstream ss;
    ss << appName << i;

    // Create a shared memory
    shmIds[i] = shmget((int) std::hash<std::string>()(ss.str()),
                       sizeof(class LamportQueue), IPC_CREAT | 0644);
    if (shmIds[i] < 0) {
      std::cerr << "Failed to create shared memory!" << std::endl;
      exit(1);
    }

    // Attach to the given shared memory
    void *shmAddr = shmat(shmIds[i], nullptr, 0);
    if (shmAddr == (void *) -1) {
      std::cerr << "Failed to attach shared memory!" << std::endl;
      exit(1);
    }

    // Initialise a queue class at that shared memory
    queues[i] = (LamportQueue *) shmAddr;
  }



  // Connect to the other middlebox
  shapedSender = new ShapedTransciever::Sender{"localhost", 4567,
                                               true,
                                               ShapedTransciever::Sender::DEBUG,
                                               100000};
  stream = shapedSender->startStream();

  while (true) {
    sleep(1);
    sendShapedData(100);
  }
}