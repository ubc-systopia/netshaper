//
// Created by Rut Vora
//

#include <thread>
#include <functional>
#include <sstream>
#include <fstream>
#include <shared_mutex>
#include "helpers.h"

extern pthread_rwlock_t quicSendLock;
#ifdef RECORD_STATS
extern std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    tcpIn;
extern std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    tcpOut;
extern std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    quicIn;
extern std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    quicOut;
extern std::vector<std::vector<uint64_t>> tcpSend;
extern std::vector<std::vector<uint64_t>> quicSend;
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

#ifdef RECORD_STATS

  void printStats(bool isShapedProcess) {
    if (isShapedProcess) {
      std::ofstream quicSendLatencies;
      quicSendLatencies.open("quicSend.csv");
      for (unsigned long i = 0; i < quicSend.size(); i++) {
        if (!quicSend[i].empty()) {
          quicSendLatencies << "Stream " << i << ",";
          for (auto elem: quicSend[i]) {
            quicSendLatencies << elem << ",";
          }
          quicSendLatencies << "\n";
        }
      }
      quicSendLatencies << std::endl;
      quicSendLatencies.close();


      std::ofstream shapedEval;
      shapedEval.open("shaped.csv");
      for (unsigned long i = 0; i < quicIn.size(); i++) {
        if (!quicIn[i].empty()) {
          shapedEval << "quicIn " << i << ",";
          for (auto elem: quicIn[i]) {
            shapedEval << elem.time_since_epoch().count() << ",";
          }
          shapedEval << "\n";
        }
      }
      shapedEval << "\n";
      for (unsigned long i = 0; i < quicOut.size(); i++) {
        if (!quicOut[i].empty()) {
          shapedEval << "quicOut " << i << ",";
          for (auto elem: quicOut[i]) {
            shapedEval << elem.time_since_epoch().count() << ",";
          }
          shapedEval << "\n";
        }
      }
      shapedEval << std::endl;
      shapedEval.close();
    } else {
      std::ofstream tcpSendLatencies;
      tcpSendLatencies.open("tcpSend.csv");
      for (unsigned long i = 0; i < tcpSend.size(); i++) {
        if (!tcpSend[i].empty()) {
          tcpSendLatencies << "Socket " << i << ",";
          for (auto elem: tcpSend[i]) {
            tcpSendLatencies << elem << ",";
          }
          tcpSendLatencies << "\n";
        }
      }
      tcpSendLatencies << std::endl;
      tcpSendLatencies.close();

      std::ofstream unshapedEval;
      unshapedEval.open("unshaped.csv");
      for (unsigned long i = 0; i < tcpIn.size(); i++) {
        if (!tcpIn[i].empty()) {
          unshapedEval << "tcpIn " << i << ",";
          for (auto elem: tcpIn[i]) {
            unshapedEval << elem.time_since_epoch().count() << ",";
          }
          unshapedEval << "\n";
        }
      }
      unshapedEval << "\n";
      for (unsigned long i = 0; i < tcpOut.size(); i++) {
        if (!tcpOut[i].empty()) {
          unshapedEval << "tcpOut " << i << ",";
          for (auto elem: tcpOut[i]) {
            unshapedEval << elem.time_since_epoch().count() << ",";
          }
          unshapedEval << "\n";
        }
      }
      unshapedEval << std::endl;
      unshapedEval.close();
    }
  }

#endif

  void waitForSignal(bool isShapedProcess) {
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
      pthread_rwlock_destroy(&quicSendLock);
      if (sigismember(&set, sig)) {
        std::cout << "\nExiting with signal " << sig << std::endl;
#ifdef RECORD_STATS
        printStats(isShapedProcess);
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

  [[noreturn]]
  void shaperLoop(std::unordered_map<QueuePair, MsQuicStream *,
      QueuePairHash> *queuesToStream,
                  NoiseGenerator *noiseGenerator,
                  const std::function<void(size_t)> &sendDummy,
                  const std::function<void(size_t)> &sendData,
                  __useconds_t sendingInterval, __useconds_t decisionInterval,
                  sendingStrategy strategy, std::shared_mutex &mapLock) {
#ifdef SHAPING
    auto decisionSleepUntil = std::chrono::steady_clock::now();
    auto sendingSleepUntil = std::chrono::steady_clock::now();
    while (true) {
      // Get DP Decision
      decisionSleepUntil = std::chrono::steady_clock::now() +
                           std::chrono::microseconds(decisionInterval);
      mapLock.lock();
      auto aggregatedSize = helpers::getAggregatedQueueSize(queuesToStream);
      mapLock.unlock();
      auto DPDecision = noiseGenerator->getDPDecision(aggregatedSize);
      if (DPDecision != 0) {
        // Enqueue data for quic to send.
        unsigned int divisor;
        if (strategy == BURST) divisor = 1; // Strategy is BURST
        if (strategy == UNIFORM) divisor = decisionInterval / sendingInterval;
        auto maxBytesToSend = DPDecision / divisor;
        for (unsigned int i = 0; i < divisor; i++) {
          sendingSleepUntil = std::chrono::steady_clock::now() +
                              std::chrono::microseconds(sendingInterval);
          // Get dummy and data size
          size_t dataSize = std::min(aggregatedSize, maxBytesToSend);
          size_t dummySize = maxBytesToSend - dataSize;
          int err = pthread_rwlock_wrlock(&quicSendLock);
          if (err == 0) {
            if (dummySize > 0) sendDummy(dummySize);
            sendData(dataSize);
            pthread_rwlock_unlock(&quicSendLock);
          }
          if (std::chrono::steady_clock::now() < sendingSleepUntil)
            std::this_thread::sleep_until(sendingSleepUntil);
        }
      }
      if (std::chrono::steady_clock::now() < decisionSleepUntil)
        std::this_thread::sleep_until(decisionSleepUntil);
    }
#else
    while (true) {
      mapLock.lock_shared();
      auto aggregatedSize = helpers::getAggregatedQueueSize(queuesToStream);
      mapLock.unlock_shared();
      int err = pthread_rwlock_wrlock(&quicSendLock);
      if (err == 0) {
        sendData(aggregatedSize);
        pthread_rwlock_unlock(&quicSendLock);
      }
    }
#endif
  }
}