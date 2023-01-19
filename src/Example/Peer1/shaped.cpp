//
// Created by Rut Vora
//

// Peer 1 (client side middle box) example for unidirectional stream

#include <sstream>
#include <thread>
#include <unordered_map>
#include <csignal>
#include <cstdarg>
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

    auto queue1 = (LamportQueue *) shmAddr1;
    auto queue2 = (LamportQueue *) shmAddr2;
    auto stream = shapedSender->startStream();

    // Data streams
    (*queuesToStream)[{queue1, queue2}] = stream;
    (*streamToQueues)[stream] = {queue1, queue2};
  }
}

// Get the total data available to be sent out. We currently assume only one
// receiver
// TODO: Do this for each receiver
size_t getAggregatedQueueSize() {
  size_t aggregatedSize = 0;
  for (auto &iterator: *queuesToStream) {
    aggregatedSize += iterator.first.toShaped->size();
  }
  return aggregatedSize;
}

// DP Decision function (runs in a separate thread at decisionInterval interval)
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

// Send dataSize bytes of data out to the other middlebox
void sendData(size_t dataSize) {
  // TODO: Add prioritisation
  for (auto &iterator: *queuesToStream) {
    if (dataSize == 0) break;
    auto size = iterator.first.toShaped->size();
    if (size == 0) continue; //TODO: Send at least 1 byte
    auto sizeToSend = std::min(dataSize, size);
    auto buffer = (uint8_t *) malloc(sizeToSend);
    iterator.first.toShaped->pop(buffer, sizeToSend);
    shapedSender->send(iterator.second, buffer, sizeToSend);
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
            (uint8_t *) malloc(dummySize);
        if (dummy != nullptr) {
          memset(dummy, 0, dummySize);
          uint64_t dummyStreamID;
          dummyStream->GetID(&dummyStreamID);
          std::cout << "Sending dummy on " << dummyStreamID << std::endl;
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
void onResponse(MsQuicStream *stream, uint8_t *buffer, size_t length) {
  (void) (stream);
  for (auto &iterator: *streamToQueues) {
    if (iterator.second.fromShaped != nullptr)
      iterator.second.fromShaped->push(buffer, length);
  }
  std::cout << "Received response from Server" << std::endl;
}

inline void startControlStream() {
  controlStream = shapedSender->startStream();
  auto *message =
      (struct controlMessage *) malloc(sizeof(struct controlMessage));
  controlStream->GetID(&message->streamID);
  message->streamType = Control;
  shapedSender->send(controlStream,
                     reinterpret_cast<uint8_t *>(message),
                     sizeof(*message));
  std::cout << "Starting control stream at " << message->streamID << std::endl;
}

inline void startDummyStream() {
  dummyStream = shapedSender->startStream();
  auto *message =
      (struct controlMessage *) malloc(sizeof(struct controlMessage));
  dummyStream->GetID(&message->streamID);
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
  shapedSender = new ShapedTransciever::Sender{"localhost", 4567, onResponse,
                                               true,
                                               ShapedTransciever::Sender::WARNING,
                                               100000};

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