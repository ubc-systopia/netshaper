//
// Created by Rut Vora
//

// Peer 2 (server side middlebox) example for unidirectional stream

#include <sstream>
#include <csignal>
#include <cstdarg>
#include <algorithm>
#include <chrono>
#include <thread>
#include "../../Modules/ShapedTransciever/Receiver.h"
#include "../../Modules/lamport_queue/queue/Cpp/LamportQueue.hpp"
#include "../../Modules/util/helpers.h"
#include "../../Modules/shaper/NoiseGenerator.h"

std::string appName = "minesVPNPeer2";


// Load the API table. Necessary before any calls to MsQuic
// It is defined as an extern const in "msquic.hpp"
// This needs to be here (on the heap)
const MsQuicApi *MsQuic = new MsQuicApi();


ShapedTransciever::Receiver *shapedReceiver;

std::unordered_map<MsQuicStream *, queuePair> *streamToQueues;
std::unordered_map<queuePair, MsQuicStream *, queuePairHash> *queuesToStream;

bool sendResponse(MsQuicStream *stream, uint8_t *data, size_t length) {
  auto SendBuffer = (QUIC_BUFFER *) malloc(sizeof(QUIC_BUFFER));
  if (SendBuffer == nullptr) {
    return false;
  }

  SendBuffer->Buffer = data;
  SendBuffer->Length = length;

  if (QUIC_FAILED(stream->Send(SendBuffer, 1, QUIC_SEND_FLAG_NONE))) {
    free(SendBuffer);
    return false;
  }
  return true;
}

// Hacky response loop
// Reads all queues and sends the data on the related stream if it exists,
// else on any stream (For testing only)
// TODO: Replace this with a proper loop for multiple clients and servers
[[noreturn]] void sendResponseLoop(__useconds_t interval) {
  while (true) {
    std::this_thread::sleep_for(std::chrono::microseconds(interval));
    for (auto &iterator: *queuesToStream) {
      auto size = iterator.first.toShaped->size();
      if (size > 0) {
        std::cout << "Got data in queue: " << iterator.first.toShaped <<
                  std::endl;
        auto buffer = (uint8_t *) malloc(size);
        iterator.first.toShaped->pop(buffer, size);
        MsQuicStream *stream = iterator.second;
        if (stream == nullptr) {
          for (auto &itr: *streamToQueues) {
            if (itr.first != nullptr) {
              stream = itr.first;
              break;
            }
          }
        }
        sendResponse(stream, buffer, size);
      }
    }
  }
}

inline bool assignQueues(MsQuicStream *stream) {
  // Find an unused queuePair and map it
  return std::ranges::any_of(*queuesToStream, [&](auto &iterator) {
    // iterator.first is queuePair, iterator.second is sender
    if (iterator.second == nullptr) {
      // No sender attached to this queue pair
      (*streamToQueues)[stream] = iterator.first;
      (*queuesToStream)[iterator.first] = stream;
      return true;
    }
    return false;
  });
}

void receivedShapedData(MsQuicStream *stream, uint8_t *buffer, size_t
length) {
  if ((*streamToQueues)[stream].fromShaped == nullptr) {
    if (!assignQueues(stream))
      std::cerr << "More streams than expected!" << std::endl;
  }
  std::cout << "Received Data..." << std::endl;
  (*streamToQueues)[stream].fromShaped->push(buffer, length);
//  return unshapedSender->sendData(buffer, length);
}

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

    // Marks the shm for deletion
    // SHM is deleted once all attached processes exit
    shmctl(shmId1, IPC_RMID, nullptr);
    shmctl(shmId2, IPC_RMID, nullptr);

    // put all queues into the map
    (*queuesToStream)[{(LamportQueue *) shmAddr1,
                       (LamportQueue *) shmAddr2}] = nullptr;
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
  int numStreams; // Max number of streams each client can initiate

  std::cout << "Enter the maximum number of streams each client can initiate "
               "(does not include dummy streams): "
            << std::endl;
  std::cin >> numStreams;

  queuesToStream =
      new std::unordered_map<queuePair,
          MsQuicStream *, queuePairHash>(numStreams);

  streamToQueues =
      new std::unordered_map<MsQuicStream *, queuePair>(numStreams);

  initialiseSHM(numStreams);

  // Start listening for connections from the other middlebox
  // Add additional stream for dummy data
  shapedReceiver =
      new ShapedTransciever::Receiver{"server.cert", "server.key",
                                      4567,
                                      receivedShapedData,
                                      ShapedTransciever::Receiver::DEBUG,
                                      numStreams + 1,
                                      100000};
  shapedReceiver->startListening();

  // Loop to send response back for the stream
  // TODO: Make this DP
  std::thread sendResp(sendResponseLoop, 100000);
  sendResp.detach();

  // Wait for signal to exit
  waitForSignal();
}