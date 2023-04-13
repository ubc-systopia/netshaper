//
// Created by Rut Vora
//

#include "UnshapedSender.h"

#include <utility>
#include <iomanip>

UnshapedSender::UnshapedSender(std::string appName, int maxPeers,
                               int maxStreamsPerPeer,
                               __useconds_t checkQueuesInterval,
                               __useconds_t shapedReceiverLoopInterval,
                               logLevels logLevel) :
    appName(std::move(appName)), logLevel(logLevel),
    shapedReceiverLoopInterval(shapedReceiverLoopInterval), sigInfo(nullptr) {
  queuesToSender =
      new std::unordered_map<QueuePair, TCP::Sender *,
          QueuePairHash>(maxStreamsPerPeer);
  pendingSignal =
      new std::unordered_map<uint64_t, connectionStatus>(maxStreamsPerPeer);
  senderToQueues =
      new std::unordered_map<TCP::Sender *, QueuePair>();

  initialiseSHM(maxPeers * maxStreamsPerPeer);

  auto checkQueuesFunc = [=, this]() {
    checkQueuesForData(checkQueuesInterval);
  };
  std::thread checkQueuesLoop(checkQueuesFunc);
  checkQueuesLoop.detach();
}

inline void UnshapedSender::initialiseSHM(int numStreams) {
  auto shmAddr = helpers::initialiseSHM(numStreams, appName);

  // The beginning of the SHM contains the signalStruct struct
  sigInfo = new(shmAddr) SignalInfo{};
  sigInfo->unshaped = getpid();

  // The rest of the SHM contains the queues
  shmAddr += sizeof(class SignalInfo);
  for (int i = 0; i < numStreams * 2; i += 2) {
    auto queue1 =
        new(shmAddr + (i * sizeof(class LamportQueue))) LamportQueue(i);
    auto queue2 =
        new(shmAddr + ((i + 1) * sizeof(class LamportQueue)))
            LamportQueue(i + 1);
    (*queuesToSender)[{queue1, queue2}] = nullptr;
  }
}

void UnshapedSender::onResponse(TCP::Sender *sender,
                                uint8_t *buffer, size_t length,
                                connectionStatus connStatus) {
  if (connStatus == ONGOING) {
    auto toShaped = (*senderToQueues)[sender].toShaped;
//    log(DEBUG, "Received response on sender connected to (toShaped) " +
//               std::to_string(toShaped->ID));
    while (toShaped->push(buffer, length) == -1) {
      log(WARNING, "(toShaped) " + std::to_string(toShaped->ID) +
                   " is full, waiting for it to be empty!");

      // Sleep for some time. For performance reasons, this is the same as
      // the interval with which DP Logic thread runs in Shaped component.
      std::this_thread::sleep_for(
          std::chrono::microseconds(shapedReceiverLoopInterval));
    }
  } else if (connStatus == FIN) {
    auto &queues = (*senderToQueues)[sender];
    if (queues.fromShaped == nullptr || queues.toShaped == nullptr) {
      log(WARNING, "No queues mapped to the sender!");
      return;
    }
    log(DEBUG, "Received FIN from sender connected to queues {" +
               std::to_string(queues.fromShaped->ID) + "," +
               std::to_string(queues.toShaped->ID) + "}");
    queues.toShaped->markedForDeletion = true;
//    if (queues.fromShaped->markedForDeletion
//        && queues.fromShaped->size() == 0) {
//      eraseMapping(sender);
//    }
    signalShapedProcess(queues.toShaped->ID, FIN);
  }
}

inline void UnshapedSender::eraseMapping(TCP::Sender *sender) {
  auto queues = (*senderToQueues)[sender];
  log(DEBUG, "Clearing the mapping for the queues {" +
             std::to_string(queues.fromShaped->ID) + "," +
             std::to_string(queues.toShaped->ID) + "}");
  // Clear mappings
  (*senderToQueues).erase(sender);
  delete sender;
  (*queuesToSender)[queues] = nullptr;
}

QueuePair UnshapedSender::findQueuesByID(uint64_t queueID) {
  for (const auto &[queues, sender]: *queuesToSender) {
    if (queues.fromShaped->ID == queueID) {
      return queues;
    }
  }
  return {nullptr, nullptr};
}

void UnshapedSender::signalShapedProcess(uint64_t queueID,
                                         connectionStatus connStatus) {
  std::scoped_lock lock(writeLock);
  struct SignalInfo::queueInfo queueInfo{queueID, connStatus};
  sigInfo->enqueue(SignalInfo::toShaped, queueInfo);

  // Signal the other process (does not actually kill the shaped process)
  kill(sigInfo->shaped, SIGUSR1);
}

void UnshapedSender::handleQueueSignal(int signum) {
  if (signum == SIGUSR1) {
    std::scoped_lock lock(readLock);
    struct SignalInfo::queueInfo queueInfo{};
    while (sigInfo->dequeue(SignalInfo::fromShaped, queueInfo)) {
      auto queues = findQueuesByID(queueInfo.queueID);
      if (queueInfo.connStatus == SYN) {
        log(DEBUG, "Received SYN on queue (fromShaped) " +
                   std::to_string(queues.fromShaped->ID));

        auto onResponseFunc = [this](auto &&PH1, auto &&PH2,
                                     auto &&PH3, auto &&PH4) {
          onResponse(std::forward<decltype(PH1)>(PH1),
                     std::forward<decltype(PH2)>(PH2),
                     std::forward<decltype(PH3)>(PH3),
                     std::forward<decltype(PH4)>(PH4));
        };
        auto unshapedSender = new TCP::Sender{
            queues.fromShaped->addrPair.serverAddress,
            std::stoi(queues.fromShaped->addrPair.serverPort),
            onResponseFunc, logLevel};
        log(DEBUG, "Starting a new sender paired to queues {" +
                   std::to_string(queues.fromShaped->ID) + "," +
                   std::to_string(queues.toShaped->ID) + "}");
        (*queuesToSender)[queues] = unshapedSender;
        (*senderToQueues)[unshapedSender] = queues;
      } else if (queueInfo.connStatus == FIN) {
        (*pendingSignal)[queueInfo.queueID] = FIN;
      }
    }
  }
}

[[noreturn]] void UnshapedSender::checkQueuesForData(__useconds_t interval) {
  auto nextCheck = std::chrono::steady_clock::now();

  while (true) {
    nextCheck += std::chrono::microseconds(interval);
//    std::this_thread::sleep_for(std::chrono::microseconds(interval));
    std::this_thread::sleep_until(nextCheck);

    for (const auto &[queues, sender]: *queuesToSender) {
      if (sender == nullptr) continue;
      auto size = queues.fromShaped->size();
      if (size == 0) {
        if ((*pendingSignal)[queues.fromShaped->ID] == FIN) {
          log(DEBUG, "Sending FIN to sender connected to (fromShaped)" +
                     std::to_string(queues.fromShaped->ID));
          sender->sendFIN();
          (*pendingSignal).erase(queues.fromShaped->ID);
        }
        // TODO: This definitely causes an issue. Find a better time to clear
        //  mappings
        if (queues.fromShaped->markedForDeletion
            && queues.toShaped->markedForDeletion) {
          eraseMapping(sender);
        }
      } else {
        auto buffer = reinterpret_cast<uint8_t *>(malloc(size));
        queues.fromShaped->pop(buffer, size);
//        while (sender == nullptr);
        auto sentBytes = sender->sendData(buffer, size);
        if ((unsigned long) sentBytes == size) {
          free(buffer);
        }

      }
    }
  }
}

void UnshapedSender::log(logLevels level, const std::string &log) {
  if (logLevel < level) return;
  std::string levelStr;
  switch (level) {
    case DEBUG:
      levelStr = "US:DEBUG: ";
      break;
    case ERROR:
      levelStr = "US:ERROR: ";
      break;
    case WARNING:
      levelStr = "US:WARNING: ";
      break;

  }
  auto time = std::time(nullptr);
  auto localTime = std::localtime(&time);
  std::scoped_lock lock(logWriter);
  std::cerr << std::put_time(localTime, "[%H:%M:%S] ")
            << levelStr << log << std::endl;
}