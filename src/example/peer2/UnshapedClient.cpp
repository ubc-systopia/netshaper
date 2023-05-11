//
// Created by Rut Vora
//

#include "UnshapedClient.h"

#include <utility>
#include <iomanip>

UnshapedClient::UnshapedClient(std::string appName, int maxPeers,
                               int maxStreamsPerPeer,
                               __useconds_t checkQueuesInterval,
                               __useconds_t shapedServerLoopInterval,
                               logLevels logLevel) :
    appName(std::move(appName)), logLevel(logLevel),
    checkQueuesInterval(checkQueuesInterval),
    shapedServerLoopInterval(shapedServerLoopInterval), sigInfo(nullptr) {
  queuesToClient =
      new std::unordered_map<QueuePair, TCP::Client *,
          QueuePairHash>(maxStreamsPerPeer);
  pendingSignal =
      new std::unordered_map<uint64_t, connectionStatus>(maxStreamsPerPeer);
  clientToQueues =
      new std::unordered_map<TCP::Client *, QueuePair>();

  initialiseSHM(maxPeers * maxStreamsPerPeer);
}

inline void UnshapedClient::initialiseSHM(int numStreams) {
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
    (*queuesToClient)[{queue1, queue2}] = nullptr;
  }
}

void UnshapedClient::onResponse(TCP::Client *client,
                                uint8_t *buffer, size_t length,
                                connectionStatus connStatus) {
  if (connStatus == ONGOING) {
    auto toShaped = (*clientToQueues)[client].toShaped;
//    log(DEBUG, "Received response on client connected to (toShaped) " +
//               std::to_string(toShaped->ID));
    while (toShaped->push(buffer, length) == -1) {
      log(WARNING, "(toShaped) " + std::to_string(toShaped->ID) +
                   " is full, waiting for it to be empty!");
#ifdef SHAPING
      // Sleep for some time. For performance reasons, this is the same as
      // the interval with which DP Logic thread runs in Shaped component.
      std::this_thread::sleep_for(
          std::chrono::microseconds(shapedServerLoopInterval));
#endif
    }
  } else if (connStatus == FIN) {
    auto &queues = (*clientToQueues)[client];
    if (queues.fromShaped == nullptr || queues.toShaped == nullptr) {
      log(WARNING, "No queues mapped to the client!");
      return;
    }
#ifdef DEBUGGING
    log(DEBUG, "Received FIN from client connected to queues {" +
               std::to_string(queues.fromShaped->ID) + "," +
               std::to_string(queues.toShaped->ID) + "}");
#endif
    queues.toShaped->markedForDeletion = true;
//    if (queues.fromShaped->markedForDeletion
//        && queues.fromShaped->size() == 0) {
//      eraseMapping(client);
//    }
    signalShapedProcess(queues.toShaped->ID, FIN);
  }
}

inline void UnshapedClient::eraseMapping(TCP::Client *client) {
  auto queues = (*clientToQueues)[client];
#ifdef DEBUGGING
  log(DEBUG, "Clearing the mapping for the queues {" +
             std::to_string(queues.fromShaped->ID) + "," +
             std::to_string(queues.toShaped->ID) + "}");
#endif
  // Clear mappings
  (*clientToQueues).erase(client);
  queues.fromShaped->inUse = false;
  delete client;
  (*queuesToClient)[queues] = nullptr;
}

QueuePair UnshapedClient::findQueuesByID(uint64_t queueID) {
  for (const auto &[queues, client]: *queuesToClient) {
    if (queues.fromShaped->ID == queueID) {
      return queues;
    }
  }
  return {nullptr, nullptr};
}

void UnshapedClient::signalShapedProcess(uint64_t queueID,
                                         connectionStatus connStatus) {
  std::scoped_lock lock(writeLock);
  struct SignalInfo::queueInfo queueInfo{queueID, connStatus};
  sigInfo->enqueue(SignalInfo::toShaped, queueInfo);

  // Signal the other process (does not actually kill the shaped process)
  kill(sigInfo->shaped, SIGUSR1);
}

void UnshapedClient::handleQueueSignal(int signum) {
  if (signum == SIGUSR1) {
    std::scoped_lock lock(readLock);
    struct SignalInfo::queueInfo queueInfo{};
    while (sigInfo->dequeue(SignalInfo::fromShaped, queueInfo)) {
      auto queues = findQueuesByID(queueInfo.queueID);
      if (queueInfo.connStatus == SYN) {
#ifdef DEBUGGING
        log(DEBUG, "Received SYN on queue (fromShaped) " +
                   std::to_string(queues.fromShaped->ID));
#endif
        auto onResponseFunc = [this](auto &&PH1, auto &&PH2,
                                     auto &&PH3, auto &&PH4) {
          onResponse(std::forward<decltype(PH1)>(PH1),
                     std::forward<decltype(PH2)>(PH2),
                     std::forward<decltype(PH3)>(PH3),
                     std::forward<decltype(PH4)>(PH4));
        };
        auto client = new TCP::Client{
            queues.fromShaped->addrPair.serverAddress,
            std::stoi(queues.fromShaped->addrPair.serverPort),
            onResponseFunc, logLevel};
#ifdef DEBUGGING
        log(DEBUG, "Starting a new client paired to queues {" +
                   std::to_string(queues.fromShaped->ID) + "," +
                   std::to_string(queues.toShaped->ID) + "}");
#endif
        (*queuesToClient)[queues] = client;
        (*clientToQueues)[client] = queues;
        std::thread forwardDataThread(
            [=, this]() { forwardData(queues, client, checkQueuesInterval); }
        );
        forwardDataThread.detach();
      } else if (queueInfo.connStatus == FIN) {
        (*pendingSignal)[queueInfo.queueID] = FIN;
      }
    }
  }
}

void UnshapedClient::forwardData(QueuePair queues, TCP::Client *client,
                                 __useconds_t interval) {
#ifdef SHAPING
  auto nextCheck = std::chrono::steady_clock::now();
  while (queues.fromShaped->inUse) {
    nextCheck = std::chrono::steady_clock::now() +
        std::chrono::microseconds(interval);
    std::this_thread::sleep_until(nextCheck);
#else
  while (queues.fromShaped->inUse) {
#endif
    auto size = queues.fromShaped->size();
    if (size == 0) {
      if ((*pendingSignal)[queues.fromShaped->ID] == FIN) {
#ifdef DEBUGGING
        log(DEBUG, "Sending FIN to client connected to (fromShaped)" +
                   std::to_string(queues.fromShaped->ID));
#endif
        client->sendFIN();
        (*pendingSignal).erase(queues.fromShaped->ID);
          queues.fromShaped->sentFIN = true;
        }
        if (queues.fromShaped->markedForDeletion
            && queues.toShaped->markedForDeletion
            && queues.fromShaped->sentFIN) {
          eraseMapping(client);
        }
      } else {
        auto buffer = reinterpret_cast<uint8_t *>(malloc(size));
        queues.fromShaped->pop(buffer, size);
        auto sentBytes = client->sendData(buffer, size);
        if ((unsigned long) sentBytes == size) {
          free(buffer);
        }

      }
    }
}

void UnshapedClient::log(logLevels level, const std::string &log) {
  if (logLevel < level) return;
  std::string levelStr;
  switch (level) {
    case DEBUG:
      levelStr = "UnshapedClient:DEBUG: ";
      break;
    case ERROR:
      levelStr = "UnshapedClient:ERROR: ";
      break;
    case WARNING:
      levelStr = "UnshapedClient:WARNING: ";
      break;

  }
  auto time = std::time(nullptr);
  auto localTime = std::localtime(&time);
  std::scoped_lock lock(logWriter);
  std::cerr << std::put_time(localTime, "[%H:%M:%S] ")
            << levelStr << log << std::endl;
}