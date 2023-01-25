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
#include "../util/helpers.h"
#include "../../Modules/shaper/NoiseGenerator.h"

std::string appName = "minesVPNPeer2";


// Load the API table. Necessary before any calls to MsQuic
// It is defined as an extern const in "msquic.hpp"
// This needs to be here (on the heap)
const MsQuicApi *MsQuic = new MsQuicApi();


ShapedTransciever::Receiver *shapedReceiver;
int numStreams; // Max number of streams each client can initiate

std::unordered_map<MsQuicStream *, queuePair> *streamToQueues;
std::unordered_map<queuePair, MsQuicStream *, queuePairHash> *queuesToStream;
// Map that stores streamIDs for which client information is received (but
// the stream has not yet begun). Value is address, port.
std::unordered_map<QUIC_UINT62, std::pair<std::string, std::string>>
    streamIDtoClient;

MsQuicStream *controlStream;
uint64_t controlStreamID;
MsQuicStream *dummyStream;
uint64_t dummyStreamID;


// The credit for sender such that it is the credit is maximum data it can send at any given time.
std::atomic<size_t> sendingCredit;


// Create numStreams number of shared memory and initialise Lamport Queues
// for each stream
inline void initialiseSHM() {
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

MsQuicStream *findStreamByID(QUIC_UINT62 ID) {
  QUIC_UINT62 streamID;
  for (auto &iterator: *streamToQueues) {
    iterator.first->GetID(&streamID);
    if (streamID == ID) return iterator.first;
  }
  return nullptr;
}

// Hacky response loop
// Reads all queues and sends the data on the related stream if it exists,
// else on any stream (For testing only)
// TODO: Replace this with a proper loop for multiple clients and servers
[[maybe_unused]] [[noreturn]] void sendResponseLoop(__useconds_t interval) {
  while (true) {
    std::this_thread::sleep_for(std::chrono::microseconds(interval));
    for (auto &iterator: *queuesToStream) {
      auto size = iterator.first.toShaped->size();
      if (size > 0) {
        std::cout << "Peer2:shaped: Got data in queue: "
                  << iterator.first.toShaped <<
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
      QUIC_UINT62 streamID;
      stream->GetID(&streamID);
      if (streamIDtoClient.find(streamID) != streamIDtoClient.end()) {
        std::strcpy(iterator.first.toShaped->clientAddress,
                    streamIDtoClient[streamID].first.c_str());
        std::strcpy(iterator.first.toShaped->clientPort,
                    streamIDtoClient[streamID].second.c_str());
        std::strcpy(iterator.first.fromShaped->clientAddress,
                    streamIDtoClient[streamID].first.c_str());
        std::strcpy(iterator.first.fromShaped->clientPort,
                    streamIDtoClient[streamID].second.c_str());
      }
      std::cout << "Peer2:Shaped: Assigned queues to one stream: " << std::endl;
      // Let's check we have at least one stream to send data on
      for (auto &itr: *queuesToStream) {
        if (itr.second != nullptr) {
          std::cout
              << "Peer2:Shaped: Found a stream to send data on in assignQueues function"
              <<
              std::endl;
        }
      }
      return true;
    }
    return false;
  });
}

void receivedShapedData(MsQuicStream *stream, uint8_t *buffer, size_t
length) {
  uint64_t streamID;
  stream->GetID(&streamID);
  // Check if this is first byte from the other middlebox
  if (controlStream == nullptr) {
    std::cout << "Peer2:Shaped: Setting control stream" << std::endl;
    size_t numMessages = length / sizeof(struct controlMessage);

    // struct controlMessage *messages[length/sizeof(struct controlMessage)];
    for (int i = 0; i < numMessages; i++) {
      auto ctrlMsg = reinterpret_cast<struct controlMessage *>(buffer +
                                                               (sizeof(struct controlMessage) *
                                                                i));
      // auto ctrlMsg = static_cast<struct controlMessage *>(buffer + (sizeof(struct controlMessage) * i));
      if (ctrlMsg->streamType == Control) {
        controlStream = stream;
        controlStreamID = streamID;
      } else if (ctrlMsg->streamType == Dummy) {
        dummyStreamID = ctrlMsg->streamID;
      }
    }
    return;
  } else if (controlStream == stream) {
    // A message from the controlStream
    std::cout << "Peer2:Shaped: Received information on dummy in control stream"
              << std::endl;
    auto ctrlMsg = reinterpret_cast<struct controlMessage *>(buffer);
    switch (ctrlMsg->streamType) {
      case Dummy:
        dummyStreamID = ctrlMsg->streamID;
        break;
      case Data: {
        auto dataStream = findStreamByID(ctrlMsg->streamID);
        if (dataStream != nullptr) {
          std::strcpy((*streamToQueues)[dataStream].toShaped->clientAddress,
                      ctrlMsg->srcIP);
          std::strcpy((*streamToQueues)[dataStream].toShaped->clientPort,
                      ctrlMsg->srcPort);
          std::strcpy((*streamToQueues)[dataStream].fromShaped->clientAddress,
                      ctrlMsg->srcIP);
          std::strcpy((*streamToQueues)[dataStream].fromShaped->clientPort,
                      ctrlMsg->srcPort);
        } else {
          // Map from stream (which has not yet started) to client
          streamIDtoClient[ctrlMsg->streamID] = std::make_pair(ctrlMsg->srcIP,
                                                               ctrlMsg->srcPort);
        }
      }
        break;
      case Control:
        // This should never happen (re-identification of control stream)
        break;
    }
    return;
  }

  // Not a control stream... Check for other types
  if (streamID == dummyStreamID) {
    // Dummy Data
    // TODO: Maybe make a queue and drop it in the unshaped side?
    std::cout << "Peer2:Shaped: Received dummy on: " << dummyStreamID
              << std::endl;
    if (dummyStream == nullptr) dummyStream = stream;
    return;
  }

  // This is a data stream
  uint64_t tmpStreamID;
  std::cout << "Peer2:Shaped: Received actual data on: "
            << stream->GetID(&tmpStreamID) << std::endl;
  if ((*streamToQueues)[stream].fromShaped == nullptr) {
    std::cout << "Peer2:Shaped: Assigning queues to stream: " << tmpStreamID
              << std::endl;
    if (!assignQueues(stream))
      std::cerr << "More streams than expected!" << std::endl;
  }
  (*streamToQueues)[stream].fromShaped->push(buffer, length);
}

// Get the total size of all queues. It is all the data we have to send in next rounds.
size_t getAggregatedQueueSize() {
  size_t aggregatedSize = 0;
  for (auto &iterator: *queuesToStream) {
    aggregatedSize += iterator.first.toShaped->size();
  }
  return aggregatedSize;
}


void sendDummy(size_t dummySize) {
  // We do not have dummy stream yet
  if (dummyStream == nullptr) return;
  auto buffer = (uint8_t *) malloc(dummySize);
  memset(buffer, 0, dummySize);
  if (!sendResponse(dummyStream, buffer, dummySize)) {
    std::cerr << "Failed to send dummy data" << std::endl;
  }
}


size_t sendData(size_t dataSize) {
  uint64_t tmpStreamID;
  // check to see we have at least one stream to send data on
  for (auto &iterator: *queuesToStream) {
    if (iterator.second != nullptr) {
      std::cout
          << "Peer2:Shaped: Found a stream to send data on in sendData function"
          << std::endl;
      std::cout << "Peer2:Shaped: The size of queue mapped to this steam is: "
                << iterator.first.toShaped->size() << std::endl;
      std::cout << "Peer2:Shaped: the aggregated queue size is: "
                << getAggregatedQueueSize() << std::endl;
      iterator.second->GetID(&tmpStreamID);
      std::cout << "Peer2:Shaped: The stream ID is: " << tmpStreamID
                << std::endl;
      break;
    }
  }
  for (auto &iterator: *queuesToStream) {
    // if(iterator.second == nullptr) {
    //   // No stream attached to this queue pair
    //   continue;
    // }
    auto queueSize = iterator.first.toShaped->size();
    // No data in this queue, check others
    if (queueSize == 0) continue;

    std::cout << "Peer2:Shaped: not-null Queue size is: " << queueSize
              << std::endl;
    // We have sent enough
    if (dataSize == 0) break;

    auto SizeToSendFromQueue = std::min(queueSize, dataSize);
    auto buffer = (uint8_t *) malloc(SizeToSendFromQueue);
    iterator.first.toShaped->pop(buffer, SizeToSendFromQueue);
    // We find the queue that has data, lets find the stream which is not null to send data on it.
    MsQuicStream *stream = nullptr;
    for (auto &streamIterator: *queuesToStream) {
      if (streamIterator.second == nullptr) continue;
      stream = streamIterator.second;
    }
    stream->GetID(&tmpStreamID);
    std::cout << "Peer2:Shaped: Sending data to the stream: " << tmpStreamID
              << std::endl;
    std::cout << "Peer2:Shaped: data is: " << buffer << std::endl;
    if (!sendResponse(stream, buffer, SizeToSendFromQueue)) {
      std::cerr << "Failed to send data" << std::endl;
    }
    dataSize -= SizeToSendFromQueue;
  }
  // We expect the data size to be zero if we have sent all the data
  return dataSize;
}

[[noreturn]] void DPCreditor(NoiseGenerator &noiseGenerator,
                             __useconds_t interval) {
  while (true) {
    auto aggregatedSize = getAggregatedQueueSize();
    auto DPDecision = noiseGenerator.getDPDecision(aggregatedSize);
    size_t creditSnapshot = sendingCredit.load(std::memory_order_acquire);
    creditSnapshot += DPDecision;
    sendingCredit.store(creditSnapshot, std::memory_order_release);
    std::this_thread::sleep_for(std::chrono::microseconds(interval));
  }
}

[[noreturn]] void sendShapedData(__useconds_t interval) {
  while (true) {
    auto creditSnapshot = sendingCredit.load(std::memory_order_acquire);
    auto aggregatedSize = getAggregatedQueueSize();
    if (creditSnapshot == 0) {
      // Do not send data
      continue;
    } else {
      size_t dataSize = std::min(creditSnapshot, aggregatedSize);
      // size_t dataSize = aggregatedSize;
      size_t dummySize = std::max(0, (int) (creditSnapshot - aggregatedSize));
      if (dataSize > 0) {
        std::cout << "Peer2:Shaped: Sending data of size: " << dataSize
                  << std::endl;
        if (sendData(dataSize) != 0) {
          std::cerr << "Peer2:Shaped: Failed to send all data" << std::endl;
        }
        creditSnapshot -= dataSize;
      }
      if (dummySize > 0) {
        sendDummy(dummySize);
        creditSnapshot -= dummySize;
      }
    }
    sendingCredit.store(creditSnapshot, std::memory_order_release);
    std::this_thread::sleep_for(std::chrono::microseconds(interval));
  }
}

int main() {

  std::cout << "Enter the maximum number of streams each client can initiate "
               "(does not include dummy streams): "
            << std::endl;
  std::cin >> numStreams;

  queuesToStream =
      new std::unordered_map<queuePair,
          MsQuicStream *, queuePairHash>(numStreams);

  streamToQueues =
      new std::unordered_map<MsQuicStream *, queuePair>(numStreams);

  initialiseSHM();

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

  // Loop to send response back for the stream
  // TODO: Make this DP
  NoiseGenerator noiseGenerator{0.01, 100};
  // std::thread sendResp(sendResponseLoop, 100000);
  // sendResp.detach();

  std::thread dpCreditor(DPCreditor, std::ref(noiseGenerator), 1000000);
  dpCreditor.detach();

  std::thread sendShaped(sendShapedData, 500000);
  sendShaped.detach();
  // Wait for signal to exit
  waitForSignal();
}