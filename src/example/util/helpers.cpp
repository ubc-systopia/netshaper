//
// Created by Rut Vora
//

#include <thread>
#include <functional>
#include <sstream>
#include <fstream>
#include <shared_mutex>
#include "helpers.h"

#ifdef RECORD_STATS
extern std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    tcpIn;
extern std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    tcpOut;
extern std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    quicIn;
extern std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    quicOut;
extern std::vector<std::vector<uint64_t>>  tcpSend;
#endif

namespace helpers {
  bool SignalInfo::dequeue(Direction direction, SignalInfo::queueInfo &info) {
    switch (direction) {
      case toShaped:
        return signalQueueToShaped.pop(reinterpret_cast<uint8_t *>(&info),
                                       sizeof(info)) >= 0;
      case fromShaped:
        return signalQueueFromShaped.pop(reinterpret_cast<uint8_t *>(&info),
                                         sizeof(info)) >= 0;
      default:
        return false;
    }
  }

  ssize_t
  SignalInfo::enqueue(Direction direction, SignalInfo::queueInfo &info) {
    switch (direction) {
      case toShaped:
        return signalQueueToShaped.push(reinterpret_cast<uint8_t *>(&info),
                                        sizeof(info));
      case fromShaped:
        return signalQueueFromShaped.push(reinterpret_cast<uint8_t *>(&info),
                                          sizeof(info));
      default:
        return false;
    }
  }

  void addSignal(sigset_t *set, int numSignals, ...) {
    va_list args;
    va_start(args, numSignals);
    for (int i = 0; i < numSignals; i++) {
      sigaddset(set, va_arg(args, int));
    }

  }

  void waitForSignal(bool shaped) {
    signal(SIGPIPE, SIG_IGN);
    sigset_t set;
    int sig;
    int ret_val;
    sigemptyset(&set);

    addSignal(&set, 6, SIGINT, SIGKILL, SIGTERM, SIGABRT, SIGSTOP,
              SIGTSTP, SIGHUP);
    sigprocmask(SIG_BLOCK, &set, nullptr);

    ret_val = sigwait(&set, &sig);
    if (ret_val == -1)
      perror("The signal wait failed\n");
    else {
      if (sigismember(&set, sig)) {
        std::cout << "\nExiting with signal " << sig << std::endl;
#ifdef RECORD_STATS
        if (shaped) {
          std::ofstream shapedEval;
          shapedEval.open("shaped.csv");
          for (unsigned long i = 0; i < quicIn.size(); i++) {
            shapedEval << "quicIn " << i << ",";
            for (auto elem: quicIn[i]) {
              shapedEval << elem.time_since_epoch().count() << ",";
            }
            shapedEval << "\n";
          }
          shapedEval << "\n";
          for (unsigned long i = 0; i < quicOut.size(); i++) {
            shapedEval << "quicOut " << i << ",";
            for (auto elem: quicOut[i]) {
              shapedEval << elem.time_since_epoch().count() << ",";
            }
            shapedEval << "\n";
          }
          shapedEval << std::endl;
          shapedEval.close();
        } else {
          std::ofstream tcpSendLatencies;
          tcpSendLatencies.open("tcpSend.csv");
          for (unsigned long i = 0; i < tcpSend.size(); i++) {
            if (!tcpSend[i].empty()) {
              tcpSendLatencies << "Client " << i << ",";
              for (auto elem : tcpSend[i]) {
                tcpSendLatencies << elem << ",";
              }
              tcpSendLatencies << "\n";
            }
          }
          tcpSendLatencies << std::endl;
          tcpSendLatencies.close();

          std::ofstream unshapedEval;
          unshapedEval.open("unshaped.csv");
//          std::cout << "Sizes: " << tcpIn.size() << " " << tcpOut.size() <<
//                    std::endl;
          for (unsigned long i = 0; i < tcpIn.size(); i++) {
            unshapedEval << "tcpIn " << i << ",";
            for (auto elem: tcpIn[i]) {
              unshapedEval << elem.time_since_epoch().count() << ",";
            }
            unshapedEval << "\n";
          }
          unshapedEval << "\n";
          for (unsigned long i = 0; i < tcpOut.size(); i++) {
            unshapedEval << "tcpOut " << i << ",";
            for (auto elem: tcpOut[i]) {
              unshapedEval << elem.time_since_epoch().count() << ",";
            }
            unshapedEval << "\n";
          }
          unshapedEval << std::endl;
          unshapedEval.close();
        }
#endif
        exit(0);
      }
    }
  }

  uint8_t *initialiseSHM(int numStreams, std::string &appName,
                         bool markForDeletion) {
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

    // Marks the shm for deletion
    // SHM is deleted once all attached processes exit
    if (markForDeletion) shmctl(shmId, IPC_RMID, nullptr);
    return shmAddr;
  }

#ifdef SHAPING
  [[noreturn]]
#endif

  void DPCreditor(std::atomic<size_t> *sendingCredit,
                  std::unordered_map<QueuePair, MsQuicStream *,
                      QueuePairHash> *queuesToStream,
                  NoiseGenerator *noiseGenerator,
                  __useconds_t decisionInterval, std::shared_mutex &mapLock) {
#ifdef SHAPING
    auto nextCheck = std::chrono::steady_clock::now();
    while (true) {
      nextCheck = std::chrono::steady_clock::now() +
                  std::chrono::microseconds(decisionInterval);
      mapLock.lock();
      auto aggregatedSize = helpers::getAggregatedQueueSize(queuesToStream);
      mapLock.unlock();
      auto DPDecision = noiseGenerator->getDPDecision(aggregatedSize);
      size_t credit = sendingCredit->load(std::memory_order_acquire);
      credit += DPDecision;
      sendingCredit->store(credit, std::memory_order_release);
      std::this_thread::sleep_until(nextCheck);
    }
#endif
  }

  [[noreturn]]
  void sendShapedData(std::atomic<size_t> *sendingCredit,
                      std::unordered_map<QueuePair, MsQuicStream *,
                          QueuePairHash> *queuesToStream,
                      const std::function<void(size_t)> &sendDummy,
                      const std::function<void(size_t)> &sendData,
                      __useconds_t sendingInterval,
                      __useconds_t decisionInterval,
                      sendingStrategy strategy, std::shared_mutex &mapLock) {

#ifdef PACING
    auto nextCheck = std::chrono::steady_clock::now();
#endif
    while (true) {
#ifdef PACING
      nextCheck = std::chrono::steady_clock::now() +
                  std::chrono::microseconds(sendingInterval);
#endif
#ifdef SHAPING
      unsigned int divisor;
      if (strategy == BURST) divisor = 1; // Strategy is BURST
      if (strategy == UNIFORM) divisor = decisionInterval / sendingInterval;
      auto credit = sendingCredit->load(std::memory_order_acquire);
//      std::cout << "Loaded credit: " << credit << std::endl;
#endif
      mapLock.lock_shared();
      auto aggregatedSize = helpers::getAggregatedQueueSize(queuesToStream);
      mapLock.unlock_shared();
#ifdef SHAPING
      if (credit == 0) {
        // Don't send anything
        continue;
      } else {
        auto maxBytesToSend = credit / divisor;
        for (unsigned int i = 0; i < divisor; i++) {
          nextCheck = std::chrono::steady_clock::now() +
                      std::chrono::microseconds(sendingInterval);
          // Get dummy and data size
          size_t dataSize = std::min(aggregatedSize, maxBytesToSend);
          size_t dummySize = maxBytesToSend - dataSize;
          if (dummySize > 0) sendDummy(dummySize);
          sendData(dataSize);
          credit -= (dataSize + dummySize);
          sendingCredit->store(credit, std::memory_order_release);
          std::this_thread::sleep_until(nextCheck);
        }
      }
#else
      sendData(aggregatedSize);
#endif
#ifdef PACING
#ifndef SHAPING
      std::this_thread::sleep_until(nextCheck);
#endif
#endif
    }
  }
}