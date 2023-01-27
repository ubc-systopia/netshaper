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

std::unordered_map<queuePair, MsQuicStream *, queuePairHash> *queuesToStream;
std::unordered_map<MsQuicStream *, queuePair> *streamToQueues;
MsQuicStream *dummyStream;
MsQuicStream *controlStream;

// Max data that can be sent out now
std::atomic<size_t> sendingCredit;


// Control and Dummy stream IDs
uint64_t controlStreamID;
uint64_t dummyStreamID;


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
//    std::cout << "DP Decision: " << DPDecision << std::endl;
    size_t credit = sendingCredit.load(std::memory_order_acquire);
    credit += DPDecision;
    sendingCredit.store(credit, std::memory_order_release);
    std::this_thread::sleep_for(std::chrono::microseconds(decisionInterval));
  }
}

/**
 * @brief Send data to the receiving middleBox
 * @param dataSize The number of bytes to send out
 */
void sendData(size_t dataSize) {
  // TODO: Add prioritisation
  for (auto &iterator: *queuesToStream) {
    if (dataSize == 0) break;
    auto size = iterator.first.toShaped->size();
    if (size == 0) continue; //TODO: Send at least 1 byte

    // Send a control message identifying stream with the actual client
    uint64_t tmpStreamID;
    iterator.second->GetID(&tmpStreamID);
    struct controlMessage ctrlMsg{tmpStreamID, Data};
    std::strcpy(ctrlMsg.srcIP, iterator.first.toShaped->clientAddress);
    std::strcpy(ctrlMsg.srcPort, iterator.first.toShaped->clientPort);

    auto sizeToSend = std::min(dataSize, size);
    auto buffer = (uint8_t *) malloc(sizeToSend);
    iterator.first.toShaped->pop(buffer, sizeToSend);

    std::cout << "Sending data of " << ctrlMsg.srcIP << ":" << ctrlMsg.srcPort
              << " on: " << tmpStreamID << std::endl;
    shapedSender->send(iterator.second, buffer, sizeToSend);
    dataSize -= sizeToSend;
  }
}

// decide on the amount of data + dummy to be sent and then call sendData.
// Runs in a separate thread at sendingInterval interval.
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
          std::cout << "Peer1:Shaped: Sending dummy on " << dummyStreamID
              << std::endl;
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

// Dummy onResponse function
// TODO: Ensure the response is sent to the correct queuePair. This requires
//  the response be received on the correct stream
/**
 * @brief Function that is called when a response is received
 * @param stream The stream on which the response was received
 * @param buffer The buffer where the response is stored
 * @param length The length of the received response
 */
void onResponse(MsQuicStream *stream, uint8_t *buffer, size_t length) {
  // (void) (stream);
  uint64_t tmp_streamID;
  stream->GetID(&tmp_streamID);
  std::cout << "Peer1:Shaped: Received response on stream " << tmp_streamID
      << std::endl;
  if (tmp_streamID != dummyStreamID && tmp_streamID != controlStreamID) {
    for (auto &iterator: *streamToQueues) {
      if (iterator.second.fromShaped != nullptr)
        iterator.second.fromShaped->push(buffer, length);
    }
    std::cout << "Peer1:Shaped: Received response from Server" << std::endl;
  } else if (tmp_streamID == dummyStreamID) {
    std::cout << "Peer1:Shaped: Received dummy from QUIC Server" << std::endl;
  } else if (tmp_streamID == controlStreamID) {
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
      (struct controlMessage *) malloc(sizeof(struct controlMessage));
  controlStream->GetID(&message->streamID);
  // save the control stream ID for later
  controlStreamID = message->streamID;
  message->streamType = Control;
  shapedSender->send(controlStream,
                     reinterpret_cast<uint8_t *>(message),
                     sizeof(*message));
  std::cout << "Starting control stream at " << message->streamID << std::endl;
}

/**
 * @brief Starts the dummy stream
 */
inline void startDummyStream() {
  dummyStream = shapedSender->startStream();
  auto *message =
      (struct controlMessage *) malloc(sizeof(struct controlMessage));
  dummyStream->GetID(&message->streamID);
  // save the dummy stream ID for later
  dummyStreamID = message->streamID;
  message->streamType = Dummy;
  shapedSender->send(controlStream,
                     reinterpret_cast<uint8_t *>(message),
                     sizeof(*message));
  std::cout << "Starting dummy stream at " << message->streamID << std::endl;
}

int main() {
  // Initialise a fixed number of buffers/queues at the beginning (as shared
  // memory)
  std::cout << "Enter the maximum number of clients that should be supported:"
               " " << std::endl;
  std::cin >> numStreams;

  queuesToStream =
      new std::unordered_map<queuePair,
          MsQuicStream *, queuePairHash>(numStreams);
  streamToQueues =
      new std::unordered_map<MsQuicStream *, queuePair>(numStreams);

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

  //Wait for signal to exit
  waitForSignal();
}