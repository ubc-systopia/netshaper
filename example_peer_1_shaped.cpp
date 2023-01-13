//
// Created by Rut Vora
//

// Peer 1 (client side middle box) example for unidirectional stream

#include <sstream>
#include <thread>
#include "ShapedTransciever/Sender.h"
#include "lamport_queue/queue/Cpp/LamportQueue.hpp"
#include "shaper/NoiseGenerator.h"

std::string appName = "minesVPN";

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

LamportQueue **queues;  // Receive buffer (queue) for each client
MsQuicStream **streams; // Array of outgoing numStream QUIC Streams

// Max data that can be sent out now
std::atomic<size_t> sendingCredit;

// Get the total data available to be sent out. We currently assume only one
// receiver
// TODO: Do this for each receiver
size_t getAggregatedQueueSize() {
  size_t aggregatedSize = 0;
  for (int i = 0; i < numStreams; i++) {
    size_t queueSize = queues[i]->size();
    aggregatedSize += queueSize;
  }
  return aggregatedSize;
}

// DP Decision function (runs in a separate thread at decisionInterval interval)
[[noreturn]] void DPCreditor(NoiseGenerator &noiseGenerator,
                             __useconds_t decisionInterval) {
  while (true) {
    auto aggregatedSize = getAggregatedQueueSize();
    auto DPDecision = noiseGenerator.getDPDecision(aggregatedSize);
    std::cout << "DP Decision: " << DPDecision << std::endl;
    size_t credit = sendingCredit.load(std::memory_order_acquire);
    credit += DPDecision;
    sendingCredit.store(credit, std::memory_order_release);
    std::this_thread::sleep_for(std::chrono::microseconds(decisionInterval));
  }
}

// Send dataSize bytes of data out to the other middlebox
void sendData(size_t dataSize) {
  // TODO: Add prioritisation
  for (int i = 0; i < numStreams; i++) {
    if (dataSize == 0) break;
    auto size = queues[i]->size();
    auto sizeToSend = std::min(dataSize, size);
    auto buffer = (uint8_t *) malloc(sizeToSend);
    queues[i]->pop(buffer, sizeToSend);
    shapedSender->send(streams[i], sizeToSend, buffer);
    dataSize -= sizeToSend;
  }
}

// decide on the amount of data + dummy to be sent and then call sendData.
// Runs in a separate thread at sendingInterval interval.
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
            (uint8_t *) calloc(credit - aggregatedSize, sizeof(uint8_t));
        shapedSender->send(streams[numStreams],
                           credit - aggregatedSize, dummy);
      }

      sendData(dataSize);
      credit -= (dataSize + dummySize);
    }
    sendingCredit.store(credit, std::memory_order_release);
  }
}

int main() {
  // Initialise a fixed number of buffers/queues at the beginning (as shared
  // memory)
  std::cout << "Enter the maximum number of clients that should be supported:"
               " " << std::endl;
  std::cin >> numStreams;
  queues =
      (LamportQueue **) malloc(numStreams * sizeof(LamportQueue *));

  // Additional stream is for dummy
  streams =
      (MsQuicStream **) malloc((numStreams + 1) * sizeof(MsQuicStream *));


  NoiseGenerator noiseGenerator{0.01, 100};

  // Connect to the other middlebox
  shapedSender = new ShapedTransciever::Sender{"localhost", 4567,
                                               true,
                                               ShapedTransciever::Sender::WARNING,
                                               100000};

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

    // Marks the shm for deletion
    // SHM is deleted once all attached processes exit
    shmctl(shmId, IPC_RMID, nullptr);

    // Initialise a queue class at that shared memory
    queues[i] = (LamportQueue *) shmAddr;

    // Data streams
    streams[i] = shapedSender->startStream();
  }

  // Dummy stream
  streams[numStreams] = shapedSender->startStream();

  std::thread dpCreditor(DPCreditor, std::ref(noiseGenerator), 1000000);
  dpCreditor.detach();
  std::thread sendingThread(sendShapedData, 500000);
  sendingThread.detach();

  // Dummy blocking function
  std::string s;
  std::cin >> s;
}