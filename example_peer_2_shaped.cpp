//
// Created by Rut Vora
//

// Peer 2 (server side middlebox) example for unidirectional stream

#include <sstream>
#include "ShapedTransciever/Receiver.h"
#include "lamport_queue/queue/Cpp/LamportQueue.hpp"

std::string appName = "minesVPNPeer2";


// Load the API table. Necessary before any calls to MsQuic
// It is defined as an extern const in "msquic.hpp"
// This needs to be here (on the heap)
const MsQuicApi *MsQuic = new MsQuicApi();

ShapedTransciever::Receiver *shapedReceiver;

std::unordered_map<MsQuicStream *, LamportQueue *> *streamToQueue;
std::unordered_map<LamportQueue *, MsQuicStream *> *queueToStream;

inline bool assignQueue(MsQuicStream *stream) {
  // Find an unused queue and map it
  for (auto &iterator: *queueToStream) {
    if (iterator.second == nullptr) {
      // No stream attached to this queue
      (*streamToQueue)[stream] = iterator.first;
      (*queueToStream)[iterator.first] = stream;
      return true;
    }
  }
  return false;
}

void receivedShapedData(MsQuicStream *stream, uint8_t *buffer, size_t
length) {
  if ((*streamToQueue)[stream] == nullptr) {
    if (!assignQueue(stream))
      std::cerr << "More streams than expected!" << std::endl;
  }
  std::cout << "Received Data..." << std::endl;
  (*streamToQueue)[stream]->push(buffer, length);
//  return unshapedSender->sendData(buffer, length);
}

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

    // Marks the shm for deletion
    // SHM is deleted once all attached processes exit
    shmctl(shmId, IPC_RMID, nullptr);

    // put all queues into the map
    (*queueToStream)[(LamportQueue *) shmAddr] = nullptr;
  }
}

int main() {
  int numStreams; // Max number of streams each client can initiate

  std::cout << "Enter the maximum number of streams each client can initiate "
               "(does not include dummy streams): "
            << std::endl;
  std::cin >> numStreams;

  queueToStream = new std::unordered_map<LamportQueue *, MsQuicStream *>
      (numStreams);
  streamToQueue = new std::unordered_map<MsQuicStream *, LamportQueue *>
      (numStreams);

  initialiseSHM(numStreams);

  // Start listening for connections from the other middlebox
  // Add additional stream for dummy data
  shapedReceiver =
      new ShapedTransciever::Receiver{"server.cert", "server.key",
                                      4567,
                                      receivedShapedData,
                                      ShapedTransciever::Receiver::WARNING,
                                      numStreams + 1,
                                      100000};
  shapedReceiver->startListening();

  // Dummy blocking function
  std::string s;
  std::cin >> s;

}