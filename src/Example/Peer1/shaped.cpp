//
// Created by Rut Vora
//

// Peer 1 (client side middle box) example for unidirectional stream

#include <thread>
#include <unordered_map>
#include <csignal>
#include "../../Modules/ShapedTransciever/Sender.h"
#include "../../Modules/lamport_queue/queue/Cpp/LamportQueue.hpp"
#include "../../Modules/shaper/NoiseGenerator.h"
#include "../util/helpers.h"

std::string appName = "minesVPNPeer1";

// Load the API table. Necessary before any calls to MsQuic
// It is defined as an extern const in "msquic.hpp"
// This needs to be here (on the heap)
const MsQuicApi *MsQuic = new MsQuicApi();

ShapedTransciever::Sender *shapedSender;

// We fix the number of streams beforehand to avoid side-channels caused by
// the additional size of the stream header.
// Note: For completely correct information, each QUIC Frame should contain a
// fixed number of streams. MsQuic does NOT do this out of the box, and the
// current code does not implement it either.
int numStreams;

std::unordered_map<QueuePair, MsQuicStream *, QueuePairHash> *queuesToStream;
std::unordered_map<MsQuicStream *, QueuePair> *streamToQueues;

class SignalInfo *sigInfo;

std::mutex readLock;
std::mutex writeLock;

MsQuicStream *dummyStream;
MsQuicStream *controlStream;

// Max data that can be sent out now
std::atomic<size_t> sendingCredit;

// Control and Dummy stream IDs
uint64_t controlStreamID;
uint64_t dummyStreamID;

/**
 * @brief Find a queue pair by the ID of it's "toShaped" queue
 * @param queueID The ID of the "toShaped" queue to find
 * @return The QueuePair this ID belongs to
 */
QueuePair findQueuesByID(uint64_t queueID) {
  for (auto &iterator: *queuesToStream) {
    if (iterator.first.toShaped->queueID == queueID) {
      return iterator.first;
    }
  }
  return {nullptr, nullptr};
}

/**
 * @brief Handle the queue status change signal sent by the unshaped process
 * @param signal The signal that was received
 */
void handleQueueSignal(int signum) {
  if (signum == SIGUSR1) {
    std::scoped_lock lock(readLock);
    struct SignalInfo::queueInfo queueInfo{};
    while (sigInfo->dequeue(SignalInfo::toShaped, queueInfo)) {
      auto queues = findQueuesByID(queueInfo.queueID);
      auto *message =
          (struct ControlMessage *) malloc(sizeof(struct ControlMessage));
      (*queuesToStream)[queues]->GetID(&message->streamID);
      message->streamType = Data;
      message->connStatus = ONGOING;  // For fallback purposes only

      switch (queueInfo.connStatus) {
        case NEW:
          message->connStatus = NEW;
          std::strcpy(message->srcIP, queues.toShaped->clientAddress);
          std::strcpy(message->srcPort, queues.toShaped->clientPort);
          std::strcpy(message->destIP, queues.toShaped->serverAddress);
          std::strcpy(message->destPort, queues.toShaped->serverPort);
          break;
        case TERMINATED:
          message->connStatus = TERMINATED;
          // Client terminated, no point in sending data to it
//          queues.fromShaped->clear();
          break;
        default:
          break;
      }
      shapedSender->send(controlStream,
                         reinterpret_cast<uint8_t *>(message),
                         sizeof(*message));
    }
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
  sigInfo = reinterpret_cast<class SignalInfo *>(shmAddr);
  if (sigInfo->unshaped == 0) {
    std::cerr << "Start the unshaped process first" << std::endl;
    exit(1);
  }
  sigInfo->shaped = getpid();

  // The rest of the SHM contains the queues
  shmAddr += sizeof(class SignalInfo);
  for (int i = 0; i < numStreams * 2; i += 2) {
    // Marks the shm for deletion
    // SHM is deleted once all attached processes exit
    shmctl(shmId, IPC_RMID, nullptr);

    auto queue1 =
        (LamportQueue *) (shmAddr + (i * sizeof(class LamportQueue)));
    auto queue2 =
        (LamportQueue *) (shmAddr + ((i + 1) * sizeof(class LamportQueue)));
    auto stream = shapedSender->startStream();

    // Data streams
    (*queuesToStream)[{queue1, queue2}] = stream;
    (*streamToQueues)[stream] = {queue1, queue2};
  }
}

/**
 * @brief Get the total data available to be sent out. We currently assume
 * only one receiver
 * @return Aggregated size of all queues
 */
// TODO: Do this for each receiver
size_t getAggregatedQueueSize() {
  size_t aggregatedSize = 0;
  for (auto &iterator: *queuesToStream) {
    aggregatedSize += iterator.first.toShaped->size();
  }
  return aggregatedSize;
}

/**
 * @brief DP Decision function (runs in a separate thread at decisionInterval interval)
 * @param noiseGenerator The instance of Gaussian Noise Generator
 * @param decisionInterval The interval to run this thread at
 */
[[noreturn]] void DPCreditor(NoiseGenerator &noiseGenerator,
                             __useconds_t decisionInterval) {
  while (true) {
    auto aggregatedSize = getAggregatedQueueSize();
    auto DPDecision = noiseGenerator.getDPDecision(aggregatedSize);
    size_t credit = sendingCredit.load(std::memory_order_acquire);
    credit += DPDecision;
    sendingCredit.store(credit, std::memory_order_release);
    std::this_thread::sleep_for(std::chrono::microseconds(decisionInterval));
  }
}

/**
 * @brief Signal the shaped process on change of queue status
 * @param queueID The ID of the queue whose status has changed
 * @param connStatus The changed status of the given queue
 */
void signalUnshapedProcess(uint64_t queueID, connectionStatus connStatus) {
  std::scoped_lock lock(writeLock);
  struct SignalInfo::queueInfo queueInfo{queueID, connStatus};
  sigInfo->enqueue(SignalInfo::fromShaped, queueInfo);
  //TODO: Handle case when queue is full

  // Signal the other process (does not actually kill the shaped process)
  kill(sigInfo->unshaped, SIGUSR1);
}

/**
 * @brief Send data to the receiving middleBox
 * @param dataSize The number of bytes to send out
 */
void sendData(size_t dataSize) {
  // TODO: Add prioritisation
  for (auto &iterator: *queuesToStream) {
    auto toShaped = iterator.first.toShaped;
    auto size = toShaped->size();
    if (size == 0 || dataSize == 0) {
      if (toShaped->markedForDeletion) {
        std::cout << "Peer1:Shaped: Queue is marked for deletion" << std::endl;
        signalUnshapedProcess(toShaped->queueID, TERMINATED);
        toShaped->markedForDeletion = false;
      }
      continue; //TODO: Send at least 1 byte
    }

    auto sizeToSend = std::min(dataSize, size);
    auto buffer = (uint8_t *) malloc(sizeToSend);
    iterator.first.toShaped->pop(buffer, sizeToSend);
    shapedSender->send(iterator.second, buffer, sizeToSend);
    QUIC_UINT62 streamID;
    iterator.second->GetID(&streamID);
    std::cout << "Peer1:Shaped: Sending actual data on " << streamID <<
              std::endl;
    dataSize -= sizeToSend;
  }
}


/**
 * @brief Loop that calls sendData(...) after obtaining the token generated
 * by the DPCreditor(...) thread.
 * @param sendingInterval The interval at which this loop should be run
 */
[[noreturn]] void sendShapedData(__useconds_t sendingInterval) {
  while (true) {
    std::this_thread::sleep_for(std::chrono::microseconds(sendingInterval));
    auto credit = sendingCredit.load(std::memory_order_acquire);
    auto aggregatedSize = getAggregatedQueueSize();
    if (credit == 0) {
      // Don't send anything
      continue;
    } else {
      // Get dummy and data size
      size_t dataSize = std::min(aggregatedSize, credit);
      size_t dummySize = std::max((size_t) 0, credit - aggregatedSize);
      if (dummySize > 0) {
        // Send dummy

        auto dummy =
            (uint8_t *) malloc(dummySize);
        if (dummy != nullptr) {
          memset(dummy, 0, dummySize);
          dummyStream->GetID(&dummyStreamID);
//          std::cout << "Peer1:Shaped: Sending dummy on " << dummyStreamID
//                    << std::endl;
          shapedSender->send(dummyStream,
                             dummy, dummySize);
        } else {
          std::cerr << "Could not allocate memory for dummy data!" << std::endl;
        }
      }

      sendData(dataSize);
      credit -= (dataSize + dummySize);
    }
    sendingCredit.store(credit, std::memory_order_release);
  }
}


/**
 * @brief Function that is called when a response is received
 * @param stream The stream on which the response was received
 * @param buffer The buffer where the response is stored
 * @param length The length of the received response
 */
void onResponse(MsQuicStream *stream, uint8_t *buffer, size_t length) {
  // (void) (stream);
  uint64_t streamId;
  stream->GetID(&streamId);
  if (streamId != dummyStreamID && streamId != controlStreamID) {
    for (auto &iterator: *streamToQueues) {
      if (iterator.second.fromShaped != nullptr)
        iterator.second.fromShaped->push(buffer, length);
    }
    std::cout << "Peer1:Shaped: Received response on " << streamId << std::endl;
  } else if (streamId == dummyStreamID) {
//    std::cout << "Peer1:Shaped: Received dummy from QUIC Server" << std::endl;
  } else if (streamId == controlStreamID) {
    std::cout << "Peer1:Shaped: Received control message from QUIC Server"
              << std::endl;
  }
}

/**
 * @brief Starts the control stream
 */
inline void startControlStream() {
  controlStream = shapedSender->startStream();
  auto *message =
      (struct ControlMessage *) malloc(sizeof(struct ControlMessage));
  controlStream->GetID(&message->streamID);
  // save the control stream ID for later
  controlStreamID = message->streamID;
  message->streamType = Control;
  shapedSender->send(controlStream,
                     reinterpret_cast<uint8_t *>(message),
                     sizeof(*message));
  std::cout << "Peer1:Shaped: Starting control stream at " << message->streamID
            << std::endl;
}

/**
 * @brief Starts the dummy stream
 */
inline void startDummyStream() {
  dummyStream = shapedSender->startStream();
  auto *message =
      (struct ControlMessage *) malloc(sizeof(struct ControlMessage));
  dummyStream->GetID(&message->streamID);
  // save the dummy stream ID for later
  dummyStreamID = message->streamID;
  message->streamType = Dummy;
  shapedSender->send(controlStream,
                     reinterpret_cast<uint8_t *>(message),
                     sizeof(*message));
  std::cout << "Peer1:Shaped: Starting dummy stream at " << message->streamID
            << std::endl;
}

int main() {
  // Initialise a fixed number of buffers/queues at the beginning (as shared
  // memory)
  std::cout << "Enter the maximum number of clients that should be supported:"
               " " << std::endl;
  std::cin >> numStreams;

  queuesToStream =
      new std::unordered_map<QueuePair,
          MsQuicStream *, QueuePairHash>(numStreams);
  streamToQueues =
      new std::unordered_map<MsQuicStream *, QueuePair>(numStreams);

  NoiseGenerator noiseGenerator{0.01, 100};
  // Connect to the other middlebox

  // Two middle-boxes are connected in a client-server setup, where peer1 middlebox is
  // the client and peer2 middlebox is the server. In middlebox 1 we have
  // shapedSender and in middlebox 2 we have shapedReceiver
  shapedSender = new ShapedTransciever::Sender{"localhost", 4567, onResponse,
                                               true,
                                               ShapedTransciever::Sender::WARNING,
                                               100000};

  // We map a pair of queues over the shared memory region to every stream
  // CAUTION: we assume the shared queues are already initialized in unshaped process
  initialiseSHM();

  // Start the control stream
  startControlStream();

  // Start the dummy stream
  startDummyStream();
  sleep(2);
  std::thread dpCreditor(DPCreditor, std::ref(noiseGenerator), 1000000);
  dpCreditor.detach();
  std::thread sendingThread(sendShapedData, 500000);
  sendingThread.detach();

  std::signal(SIGUSR1, handleQueueSignal);

  //Wait for signal to exit
  waitForSignal();
}