//
// Created by Rut Vora
//

#include "UnshapedSender.h"

#include <utility>

UnshapedSender::UnshapedSender(std::string appName, int maxPeers,
                               int maxStreamsPerPeer,
                               __useconds_t checkQueuesInterval) :
    appName(std::move(appName)), logLevel(DEBUG), sigInfo(nullptr) {
  queuesToSender =
      new std::unordered_map<QueuePair, TCP::Sender *,
          QueuePairHash>(maxStreamsPerPeer);

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
    (*senderToQueues)[sender].toShaped->push(buffer, length);
  } else if (connStatus == FIN) {
    (*senderToQueues)[sender].toShaped->markedForDeletion = true;
  }
}

QueuePair UnshapedSender::findQueuesByID(uint64_t queueID) {
  for (auto &iterator: *queuesToSender) {
    if (iterator.first.fromShaped->ID == queueID) {
      return iterator.first;
    }
  }
  return {nullptr, nullptr};
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
            queues.fromShaped->serverAddress,
            std::stoi(queues.fromShaped->serverPort),
            onResponseFunc, WARNING};
        log(DEBUG, "Starting a new sender paired to queues {" +
                   std::to_string(queues.fromShaped->ID) + "," +
                   std::to_string(queues.toShaped->ID) + "}");
        (*queuesToSender)[queues] = unshapedSender;
        (*senderToQueues)[unshapedSender] = queues;
      } else if (queueInfo.connStatus == FIN) {
//        std::cout << "Peer2:Unshaped: Connection Terminated" << std::endl;
//        // Push all available data from queue to Sender
//        auto size = queues.fromShaped->size();
//        if (size > 0) {
//          auto buffer = reinterpret_cast<uint8_t *>(alloca(size));
//          queues.fromShaped->pop(buffer, size);
//          (*queuesToSender)[queues]->sendData(buffer, size);
//        }
//        // Clear the queues and mapping
//        queues.toShaped->clear();
//        queues.fromShaped->clear();
//        (*senderToQueues).erase((*queuesToSender)[queues]);
//        delete (*queuesToSender)[queues];
//        (*queuesToSender)[queues] = nullptr;
//        queues.toShaped->markedForDeletion = false;
//        queues.fromShaped->markedForDeletion = false;
      }
    }
  }
}

[[noreturn]] void UnshapedSender::checkQueuesForData(__useconds_t interval) {
  while (true) {
    std::this_thread::sleep_for(std::chrono::microseconds(interval));
//    usleep(100000);
    for (auto &iterator: *queuesToSender) {
      auto size = iterator.first.fromShaped->size();
      // If queue is marked for deletion, it will be flushed by the signal
      // handler
      if (size == 0 && iterator.first.fromShaped->markedForDeletion) {
        iterator.second->sendFIN();
        // TODO: This definitely causes an issue. Find a better time to clear
        //  mappings
        if (iterator.first.toShaped->markedForDeletion) {
          // Clear mappings
//          (*senderToQueues).erase(iterator.second);
//          delete iterator.second;
//          iterator.second = nullptr;
        }
      } else if (size > 0) {
        log(DEBUG, "Data in queue (fromShaped) " +
                   std::to_string(iterator.first.fromShaped->ID));
        auto buffer = reinterpret_cast<uint8_t *>(malloc(size));
        iterator.first.fromShaped->pop(buffer, size);
        while (iterator.second == nullptr);
        iterator.second->sendData(buffer, size);
        free(buffer);
      }
    }
  }
}

void UnshapedSender::log(logLevels level, const std::string &log) {
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
  if (logLevel >= level) {

    std::cerr << levelStr << log << std::endl;
  }
}