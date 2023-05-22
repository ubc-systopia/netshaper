//
// Created by Rut Vora
//

#include <thread>
#include <functional>
#include <sstream>
#include <fstream>
#include <shared_mutex>
#include "helpers.h"
#include "config.h"

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
extern std::vector<std::pair<uint64_t, uint64_t>> timeDPDecisions;
extern std::vector<std::pair<uint64_t, uint64_t>> timeDataPrep;
extern std::vector<std::pair<uint64_t, uint64_t>> timeDataEnqueue;
static std::atomic<int> totalIter = 0;
static std::atomic<int> failedDPMask = 0;
static std::atomic<int> failedPrepMask = 0;
static std::atomic<int> failedEnqueueMask = 0;
#endif

namespace helpers {
  void setCPUAffinity(std::vector<int> &cpus) {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    for (int i: cpus) {
      CPU_SET(i, &mask);
    }
    int result = sched_setaffinity(0, sizeof(mask), &mask);
    if (result == -1) {
      std::cout << "Could not set CPU affinity" << std::endl;
      exit(1);
    }
  }

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
      {
        std::cout << "Total iters: " << totalIter
                  << "\nFailed DP Masks: " << failedDPMask
                  << "\nFailed Prep Masks: " << failedPrepMask
                  << "\nFailed Enqueue Masks: " << failedEnqueueMask
                  << std::endl;
      }
      {
        std::ofstream maskDurations;
        maskDurations.open("maskDurations.csv");
        maskDurations <<
                      "Data size, DP Decision Time, Prep Time, Enqueue Time\n";
        auto length = std::min(timeDPDecisions.size(),
                               std::min(timeDataPrep.size(),
                                        timeDataEnqueue.size()));
        for (unsigned long i = 0; i < length; i++) {
          maskDurations << timeDataPrep[i].first << ", "
                        << timeDPDecisions[i].second << ", "
                        << timeDataPrep[i].second << ", "
                        << timeDataEnqueue[i].second
                        << "\n";
        }
        maskDurations << std::endl;
        maskDurations.close();
      }
      {
        std::ofstream quicSendLatencies;
        quicSendLatencies.open("quicSend.csv");
        std::vector<unsigned long> filledColumns{};
        for (unsigned long i = 0; i < quicSend.size(); i++) {
          if (!quicSend[i].empty())
            filledColumns.push_back(i);
        }
        filledColumns.pop_back(); // Remove last column as it is control stream
        unsigned long minRows = INT_MAX;
        for (auto i: filledColumns) {
          quicSendLatencies << "Stream " << i << ",";
          if (minRows > quicSend[i].size()) minRows = quicSend[i].size();
        }
        quicSendLatencies << "\n";
        for (unsigned long j = 0; j < minRows; j++) {
          for (auto i: filledColumns) {
            quicSendLatencies << quicSend[i][j] << ", ";
          }
          quicSendLatencies << "\n";
        }

        quicSendLatencies << std::endl;
        quicSendLatencies.close();
      }
      {
        std::ofstream shapedEval;
        shapedEval.open("shaped.csv");
        std::vector<unsigned long> filledColumns{};
        for (unsigned long i = 0; i < quicIn.size(); i++) {
          if (!quicIn[i].empty())
            filledColumns.push_back(i);
        }
        filledColumns.pop_back(); // Remove the control stream entries
        for (auto i: filledColumns) {
          shapedEval << "quicIn " << i << ",";
          for (auto elem: quicIn[i]) {
            shapedEval << elem.time_since_epoch().count() << ",";
          }
          shapedEval << "\n";
        }
        shapedEval << "\n";

        filledColumns.clear();
        for (unsigned long i = 0; i < quicOut.size(); i++) {
          if (!quicOut[i].empty())
            filledColumns.push_back(i);
        }
        filledColumns.pop_back(); // Remove the control stream entries
        for (auto i: filledColumns) {
          shapedEval << "quicOut " << i << ",";
          for (auto elem: quicOut[i]) {
            shapedEval << elem.time_since_epoch().count() << ",";
          }
          shapedEval << "\n";
        }
        shapedEval << std::endl;
        shapedEval.close();
      }
    } else {
      std::ofstream tcpSendLatencies;
      tcpSendLatencies.open("tcpSend.csv");
      unsigned long minRows = INT_MAX;
      for (unsigned long i = 0; i < tcpSend.size(); i++) {
        if (!tcpSend[i].empty()) {
          tcpSendLatencies << "Socket " << i << ",";
          if (minRows > tcpSend[i].size()) minRows = tcpSend[i].size();
        }
      }
      tcpSendLatencies << "\n";
      for (unsigned long j = 0; j < minRows; j++) {
        for (const auto &column: tcpSend) {
          if (!column.empty()) {
            tcpSendLatencies << column[j] << ", ";
          }
        }
        tcpSendLatencies << "\n";
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

    addSignal(&set, 7, SIGINT, SIGKILL, SIGTERM, SIGABRT, SIGSTOP,
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
                  const std::function<PreparedBuffer(size_t)>
                  &prepareDummy,
                  const std::function<std::vector<PreparedBuffer>(size_t)>
                  &prepareData,
                  const std::function<void(MsQuicStream *, uint8_t *, size_t)>
                  &placeInQuicQueues,
                  __useconds_t sendingInterval, __useconds_t decisionInterval,
                  sendingStrategy strategy, std::shared_mutex &mapLock,
                  std::vector<int> cores) {
    setCPUAffinity(cores);
#ifdef RECORD_STATS
    // 0 if we want to profile for these values
    auto maskDPDecisionUs = 500;
    auto maskPrepDurationUs = 9000;
    auto maskEnqueueDurationUs = 250;
#else
    auto maskDPDecisionUs = 1000; // TODO: Replace this value
    auto maskPrepDurationUs = 1000; // TODO: Replace this value
    auto maskEnqueueDurationUs = 1000; // TODO: Replace this value
#endif
    auto mask = std::chrono::steady_clock::now();
    auto decisionSleepUntil = std::chrono::steady_clock::now();
    auto sendingSleepUntil = std::chrono::steady_clock::now();
    auto start = std::chrono::steady_clock::now();
    auto end = start;
    while (true) {
#ifdef SHAPING
      decisionSleepUntil += std::chrono::microseconds(decisionInterval);
#else
      decisionSleepUntil = std::chrono::steady_clock::now() +
                           std::chrono::microseconds(decisionInterval);
#endif
      // Masked DP Decision Time
      mask = std::chrono::steady_clock::now() +
             std::chrono::microseconds(maskDPDecisionUs);
      start = std::chrono::steady_clock::now();
      mapLock.lock_shared();
      auto aggregatedSize = helpers::getAggregatedQueueSize(queuesToStream);
      mapLock.unlock_shared();
      auto DPDecision = noiseGenerator->getDPDecision(aggregatedSize);
      end = std::chrono::steady_clock::now();
#ifdef RECORD_STATS
      totalIter++;
      timeDPDecisions.emplace_back(DPDecision, (end - start).count());
#endif
      if (std::chrono::steady_clock::now() < mask)
        std::this_thread::sleep_until(mask);
#ifdef RECORD_STATS
      else failedDPMask++;
#endif

#ifndef SHAPING
      DPDecision = aggregatedSize;
#endif
      if (DPDecision != 0) {
        // Enqueue data for quic to send.
        unsigned int divisor;
        switch (strategy) {
          case BURST:
            divisor = 1;
            break;
          case UNIFORM:
            divisor = decisionInterval / sendingInterval;
            break;
        }
        auto maxBytesToSend = DPDecision / divisor;
        for (unsigned int i = 0; i < divisor; i++) {
          sendingSleepUntil += std::chrono::microseconds(sendingInterval);

          // Masked Prep time
          mask = std::chrono::steady_clock::now() +
                 std::chrono::microseconds(maskPrepDurationUs);
          start = std::chrono::steady_clock::now();
          size_t dataSize = std::min(aggregatedSize, maxBytesToSend);
          size_t dummySize = maxBytesToSend - dataSize;
          auto preparedBuffers = prepareData(dataSize);
          preparedBuffers.push_back(prepareDummy(dummySize));
          end = std::chrono::steady_clock::now();
#ifdef RECORD_STATS
          timeDataPrep.emplace_back(aggregatedSize, (end - start).count());
#endif
          if (std::chrono::steady_clock::now() < mask)
            std::this_thread::sleep_until(mask);
#ifdef RECORD_STATS
          else failedPrepMask++;
#endif
          int err = pthread_rwlock_wrlock(&quicSendLock);
          mask = std::chrono::steady_clock::now() +
                 std::chrono::microseconds(maskEnqueueDurationUs);
          start = std::chrono::steady_clock::now();
          if (err == 0) {
            for (auto preparedBuffer: preparedBuffers) {
              if (preparedBuffer.stream == nullptr
                  || preparedBuffer.buffer == nullptr)
                continue;
              placeInQuicQueues(preparedBuffer.stream, preparedBuffer.buffer,
                                preparedBuffer.length);
            }
            end = std::chrono::steady_clock::now();
#ifdef RECORD_STATS
            timeDataEnqueue.emplace_back(aggregatedSize, (end - start).count());
#endif
            if (std::chrono::steady_clock::now() < mask)
              std::this_thread::sleep_until(mask);
#ifdef RECORD_STATS
            else failedEnqueueMask++;
#endif
            pthread_rwlock_unlock(&quicSendLock);
          }
          if (std::chrono::steady_clock::now() < sendingSleepUntil)
            std::this_thread::sleep_until(sendingSleepUntil);
        }
      } else {
        prepareData(0); // For state management of client who disconnected
      }
      if (std::chrono::steady_clock::now() < decisionSleepUntil) {
        std::this_thread::sleep_until(decisionSleepUntil);
      }
    }
  }
}