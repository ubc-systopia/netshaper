//
// Created by Rut Vora
//

#include "UnshapedSender.h"

#include <utility>

UnshapedSender::UnshapedSender(std::string appName, int maxPeers,
                               int maxStreamsPerPeer,
                               __useconds_t checkQueuesInterval) :
    appName(std::move(appName)), sigInfo(nullptr) {
  queuesToSender =
      new std::unordered_map<QueuePair, TCP::Sender *,
          QueuePairHash>(maxStreamsPerPeer);

  senderToQueues =
      new std::unordered_map<TCP::Sender *, QueuePair>();

  initialiseSHM(maxPeers * maxStreamsPerPeer);

  auto checkQueuesFunc = [this](auto &&PH1) {
    checkQueuesForData(std::forward<decltype(PH1)>(PH1));
  };
  std::thread checkQueuesLoop(checkQueuesFunc, checkQueuesInterval);
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
                                uint8_t *buffer, size_t length) {
  (*senderToQueues)[sender].toShaped->push(buffer, length);
}

QueuePair UnshapedSender::findQueuesByID(uint64_t queueID) {
  for (auto &iterator: *queuesToSender) {
    if (iterator.first.fromShaped->queueID == queueID) {
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
      if (queueInfo.connStatus == NEW) {
        std::cout << "Peer2:Unshaped: New Connection" << std::endl;
        auto onResponseFunc = [this](auto &&PH1, auto &&PH2, auto &&PH3) {
          onResponse(std::forward<decltype(PH1)>(PH1),
                     std::forward<decltype(PH2)>(PH2),
                     std::forward<decltype(PH3)>(PH3));
        };
        auto unshapedSender = new TCP::Sender{
            queues.fromShaped->serverAddress,
            std::stoi(queues.fromShaped->serverPort),
            onResponseFunc};

        (*queuesToSender)[queues] = unshapedSender;
        (*senderToQueues)[unshapedSender] = queues;
      } else if (queueInfo.connStatus == TERMINATED) {
        std::cout << "Peer2:Unshaped: Connection Terminated" << std::endl;
        // Push all available data from queue to Sender
        auto size = queues.fromShaped->size();
        if (size > 0) {
          auto buffer = reinterpret_cast<uint8_t *>(alloca(size));
          queues.fromShaped->pop(buffer, size);
          (*queuesToSender)[queues]->sendData(buffer, size);
        }
        // Clear the queues and mapping
        queues.toShaped->clear();
        queues.fromShaped->clear();
        (*senderToQueues).erase((*queuesToSender)[queues]);
        delete (*queuesToSender)[queues];
        (*queuesToSender)[queues] = nullptr;
        queues.toShaped->markedForDeletion = false;
        queues.fromShaped->markedForDeletion = false;
      }
    }
  }
}

[[noreturn]] void UnshapedSender::checkQueuesForData(__useconds_t interval) {
  // TODO: Replace this with signalling from shaped side (would it be more
  //  efficient?)
  while (true) {
    std::this_thread::sleep_for(std::chrono::microseconds(interval));
//    usleep(100000);
    for (auto &iterator: *queuesToSender) {
      auto size = iterator.first.fromShaped->size();
      // If queue is marked for deletion, it will be flushed by the signal
      // handler
      if (size > 0 && !iterator.first.fromShaped->markedForDeletion) {
        std::cout << "Got data in queue: " << iterator.first.fromShaped <<
                  std::endl;
        auto buffer = reinterpret_cast<uint8_t *>(malloc(size));
        iterator.first.fromShaped->pop(buffer, size);
        while (iterator.second == nullptr);
        iterator.second->sendData(buffer, size);
        free(buffer);
      }
    }
  }
}