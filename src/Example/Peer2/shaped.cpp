//
// Created by Rut Vora
//

// Peer 2 (server side middlebox) example for unidirectional stream

#include <sstream>
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
struct queueSignal *queueSig;
// Map that stores streamIDs for which client information is received (but
// the stream has not yet begun). Value is address, port.
std::unordered_map<QUIC_UINT62, struct controlMessage> streamIDtoCtrlMsg;

MsQuicStream *controlStream;
uint64_t controlStreamID;
MsQuicStream *dummyStream;
uint64_t dummyStreamID;


// The credit for sender such that it is the credit is maximum data it can send at any given time.
std::atomic<size_t> sendingCredit;


/**
 * @brief Create numStreams number of shared memory streams and initialise
 * Lamport Queues for each stream
 */
inline void initialiseSHM() {
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
  if (queueSig->unshaped == 0) {
    std::cerr << "Start the unshaped process first" << std::endl;
    exit(1);
  }
  queueSig->shaped = getpid();
  queueSig->ack = true;

  // The rest of the SHM contains the queues
  shmAddr += sizeof(struct queueSignal);
  for (int i = 0; i < numStreams * 2; i += 2) {
    // Marks the shm for deletion
    // SHM is deleted once all attached processes exit
    shmctl(shmId, IPC_RMID, nullptr);

    auto queue1 =
        (LamportQueue *) (shmAddr + (i * sizeof(class LamportQueue)));
    auto queue2 =
        (LamportQueue *) (shmAddr + ((i + 1) * sizeof(class LamportQueue)));

    // Data streams
    (*queuesToStream)[{queue1, queue2}] = nullptr;
  }
}

/**
 * @brief Send response to the other middleBox
 * @param stream The stream to send the response on
 * @param data The buffer which holds the data to be sent
 * @param length The length of the data to be sent
 * @return
 */
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

/**
 * @brief Finds a stream by it's ID
 * @param ID The ID of the stream to match
 * @return The stream pointer of the stream the ID matches with, or nullptr
 */
MsQuicStream *findStreamByID(QUIC_UINT62 ID) {
  QUIC_UINT62 streamID;
  for (auto &iterator: *streamToQueues) {
    if (iterator.first == nullptr) continue;
    iterator.first->GetID(&streamID);
    if (streamID == ID) return iterator.first;
  }
  return nullptr;
}

// Reads all queues and sends the data on the related stream
/**
 * @brief Check for response in a separate thread
 * @param interval The interval with which the queues are checked for responses
 */
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
        QUIC_UINT62 streamId;
        stream->GetID(&streamId);
        std::cout << "Peer2:Shaped: Sending response on " << streamId <<
                  std::endl;
        sendResponse(stream, buffer, size);
      }
    }
  }
}

/**
 * @brief Signal the shaped process on change of queue status
 * @param queueID The ID of the queue whose status has changed
 * @param connStatus The changed status of the given queue
 */
void signalUnshapedProcess(uint64_t queueID, connectionStatus connStatus) {
  // Wait in spin lock while the other process acknowledges previous signal
  while (!queueSig->ack)
    continue;
  queueSig->queueID = queueID;
  queueSig->connStatus = connStatus;
  // Signal the other process (does not actually kill the shaped process)
  kill(queueSig->unshaped, SIGUSR1);
}

/**
 * @brief Copies the client and server info from the control message to the
 * queues
 * @param queues The queues to copy the data to
 * @param ctrlMsg The control message to copy the data from
 */
void copyClientInfo(queuePair queues, struct controlMessage *ctrlMsg) {
  // Source/Client
  std::strcpy(queues.toShaped->clientAddress,
              ctrlMsg->srcIP);
  std::strcpy(queues.toShaped->clientPort,
              ctrlMsg->srcPort);
  std::strcpy(queues.fromShaped->clientAddress,
              ctrlMsg->srcIP);
  std::strcpy(queues.fromShaped->clientPort,
              ctrlMsg->srcPort);

  //Dest/Server
  std::strcpy(queues.toShaped->serverAddress,
              ctrlMsg->destIP);
  std::strcpy(queues.toShaped->serverPort,
              ctrlMsg->destPort);
  std::strcpy(queues.fromShaped->serverAddress,
              ctrlMsg->destIP);
  std::strcpy(queues.fromShaped->serverPort,
              ctrlMsg->destPort);
}

/**
 * @brief assign a new queue for a new client
 * @param stream The new stream (representing a new client)
 * @return true if queue was assigned successfully
 */
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
      queuePair queues = iterator.first;
      if (streamIDtoCtrlMsg.find(streamID) != streamIDtoCtrlMsg.end()) {
        copyClientInfo(queues, &streamIDtoCtrlMsg[streamID]);
        std::thread signalOtherProcess(signalUnshapedProcess,
                                       (*streamToQueues)[stream]
                                           .fromShaped->queueID, NEW);
        signalOtherProcess.detach();
        streamIDtoCtrlMsg.erase(streamID);

      }
      std::cout << "Peer2:Shaped: Assigned queues to a new stream: " <<
                std::endl;
      return true;
    }
    return false;
  });
}

/**
 * @brief Handle receiving control messages from the other middleBox
 * @param ctrlMsg The control message that was received
 */
void receivedControlMessage(struct controlMessage *ctrlMsg) {
  switch (ctrlMsg->streamType) {
    case Dummy:
      std::cout << "Dummy stream ID " << ctrlMsg->streamID << std::endl;
      dummyStreamID = ctrlMsg->streamID;
      break;
    case Data: {
      auto dataStream = findStreamByID(ctrlMsg->streamID);
      if (ctrlMsg->connStatus == NEW) {
        std::cout << "Data stream new ID " << ctrlMsg->streamID << std::endl;
        auto queues = (*streamToQueues)[dataStream];
        if (dataStream != nullptr) {
          copyClientInfo(queues, ctrlMsg);
          std::thread signalOtherProcess(signalUnshapedProcess,
                                         queues.fromShaped->queueID, NEW);
          signalOtherProcess.detach();
        } else {
          // Map from stream (which has not yet started) to client
          streamIDtoCtrlMsg[ctrlMsg->streamID] = *ctrlMsg;
        }
      } else if (ctrlMsg->connStatus == TERMINATED) {
        std::cout << "Data stream terminated ID " << ctrlMsg->streamID
                  << std::endl;
        std::thread signalOtherProcess(signalUnshapedProcess,
                                       (*streamToQueues)[dataStream]
                                           .fromShaped->queueID, TERMINATED);
        signalOtherProcess.detach();
        (*queuesToStream)[(*streamToQueues)[dataStream]] = nullptr;
        (*streamToQueues).erase(dataStream);

      }
    }
      break;
    case Control:
      // This (re-identification of control stream) should never happen
      break;
  }
}

/**
 * @brief The function that is called when shaped data is received from the
 * other middleBox.
 * @param stream The stream on which the data was received
 * @param buffer The buffer where the received data is stored
 * @param length The length of the received data
 */
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
    auto ctrlMsg = reinterpret_cast<struct controlMessage *>(buffer);
    std::cout << "Peer2:Shaped: Received control message for stream type " <<
              ctrlMsg->streamType << std::endl;
    receivedControlMessage(ctrlMsg);
    return;
  }

  // Not a control stream... Check for other types
  if (streamID == dummyStreamID) {
    // Dummy Data
    // TODO: Maybe make a queue and drop it in the unshaped side?
//    std::cout << "Peer2:Shaped: Received dummy on: " << dummyStreamID
//              << std::endl;
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

/**
 * @brief Get the total size of all queues. It is all the data we have to send in next rounds.
 * @return The total data available to be sent out
 */
size_t getAggregatedQueueSize() {
  size_t aggregatedSize = 0;
  for (auto &iterator: *queuesToStream) {
    aggregatedSize += iterator.first.toShaped->size();
  }
  return aggregatedSize;
}

/**
 * @brief Send dummy bytes on the dummy stream
 * @param dummySize The number of bytes to be sent out
 */
void sendDummy(size_t dummySize) {
  // We do not have dummy stream yet
  if (dummyStream == nullptr) return;
  auto buffer = (uint8_t *) malloc(dummySize);
  memset(buffer, 0, dummySize);
  if (!sendResponse(dummyStream, buffer, dummySize)) {
    std::cerr << "Failed to send dummy data" << std::endl;
  }
}

/**
 * @brief Send data to the other middlebox
 * @param dataSize The number of bytes to send to the other middlebox
 * @return The number of bytes sent out
 */
size_t sendData(size_t dataSize) {
  auto origSize = dataSize;

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
  return origSize - dataSize;
}

/**
 * @brief DP Decision function (runs in a separate thread at decisionInterval interval)
 * @param noiseGenerator The instance of Gaussian Noise Generator
 * @param decisionInterval The interval to run this thread at
 */
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

/**
 * @brief Loop that calls sendData(...) after obtaining the token generated
 * by the DPCreditor(...) thread.
 * @param sendingInterval The interval at which this loop should be run
 */
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

  NoiseGenerator noiseGenerator{0.01, 100};

  std::thread dpCreditor(DPCreditor, std::ref(noiseGenerator), 1000000);
  dpCreditor.detach();

  std::thread sendShaped(sendShapedData, 500000);
  sendShaped.detach();
  // Wait for signal to exit
  waitForSignal();
}