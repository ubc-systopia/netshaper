//
// Created by Rut Vora
//

#include "UnshapedClient.h"

#include <utility>
#include <iomanip>

UnshapedClient::UnshapedClient(config::Peer2Config &peer2Config) {
  this->appName = std::move(appName);
  this->logLevel = logLevel;
  shapedProcessLoopInterval =
      peer2Config.shapedServer.strategy == UNIFORM
      ? peer2Config.shapedServer.sendingLoopInterval
      : peer2Config.shapedServer.DPCreditorLoopInterval;

  queuesToClient =
      new std::unordered_map<QueuePair, TCP::Client *,
          QueuePairHash>(peer2Config.maxStreamsPerPeer);
  pendingSignal =
      new std::unordered_map<uint64_t, connectionStatus>(
          peer2Config.maxStreamsPerPeer);
  clientToQueues =
      new std::unordered_map<TCP::Client *, QueuePair>();

  initialiseSHM(peer2Config.maxPeers * peer2Config.maxStreamsPerPeer,
                peer2Config.queueSize);

  auto checkQueuesFunc = [=, this]() {
    checkQueuesForData(peer2Config.unshapedClient.checkQueuesInterval,
                       peer2Config.queueSize);
  };
  std::thread checkQueuesLoop(checkQueuesFunc);
  checkQueuesLoop.detach();

  std::thread updateQueueStatus([this]() { getUpdatedConnectionStatus(); });
  updateQueueStatus.detach();
}

inline void UnshapedClient::initialiseSHM(int numStreams, size_t queueSize) {
  auto shmAddr = helpers::initialiseSHM(numStreams, appName, queueSize);

  // The beginning of the SHM contains the signalStruct struct
  sigInfo = new(shmAddr) SignalInfo{numStreams};

  // The rest of the SHM contains the queues
  shmAddr += (sizeof(SignalInfo) + (2 * sizeof(LamportQueue)) +
              (4 * numStreams * sizeof(SignalInfo::queueInfo)));
  for (unsigned long i = 0; i <= numStreams * 2; i += 2) {
    auto queue1 =
        new(shmAddr +
            (i * (sizeof(class LamportQueue) + queueSize)))
            LamportQueue{i, queueSize};
    auto queue2 =
        new(shmAddr + ((i + 1) * (sizeof(class LamportQueue) + queueSize)))
            LamportQueue{i + 1, queueSize};
    if (i > 0) (*queuesToClient)[{queue1, queue2}] = nullptr;
    else dummyQueues = {queue1, queue2};
  }
}

void UnshapedClient::onResponse(TCP::Client *client,
                                uint8_t *buffer, size_t length,
                                connectionStatus connStatus) {
  if (connStatus == ONGOING) {
    auto toShaped = (*clientToQueues)[client].toShaped;
    while (toShaped->push(buffer, length) == -1) {
      log(WARNING, "(toShaped) " + std::to_string(toShaped->ID) +
                   " is full, waiting for it to be empty!");
#ifdef SHAPING
      // Sleep for some time. For performance reasons, this is the same as
      // the interval with which DP Logic thread runs in Shaped component.
      std::this_thread::sleep_for(
          std::chrono::microseconds(shapedProcessLoopInterval));
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
    updateConnectionStatus(queues.toShaped->ID, FIN);
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

void UnshapedClient::updateConnectionStatus(uint64_t queueID,
                                            connectionStatus connStatus) {
  std::scoped_lock lock(writeLock);
  struct SignalInfo::queueInfo queueInfo{queueID, connStatus};
  sigInfo->enqueue(SignalInfo::toShaped, queueInfo);

  // Signal the other process (does not actually kill the shaped process)
//  kill(sigInfo->shaped, SIGUSR1);
}

[[noreturn]] void UnshapedClient::getUpdatedConnectionStatus() {
  struct SignalInfo::queueInfo queueInfo{};
  auto sleepUntil = std::chrono::steady_clock::now();
  while (true) {
    sleepUntil = std::chrono::steady_clock::now() +
                 std::chrono::milliseconds(1);
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
        auto unshapedClient = new TCP::Client{
            queues.fromShaped->addrPair.serverAddress,
            std::stoi(queues.fromShaped->addrPair.serverPort),
            onResponseFunc, logLevel};
#ifdef DEBUGGING
        log(DEBUG, "Starting a new client paired to queues {" +
                   std::to_string(queues.fromShaped->ID) + "," +
                   std::to_string(queues.toShaped->ID) + "}");
#endif
        (*queuesToClient)[queues] = unshapedClient;
        (*clientToQueues)[unshapedClient] = queues;
      } else if (queueInfo.connStatus == FIN) {
        (*pendingSignal)[queueInfo.queueID] = FIN;
      }
    }
    std::this_thread::sleep_until(sleepUntil);
  }
}

[[noreturn]] void UnshapedClient::checkQueuesForData(__useconds_t interval,
                                                     size_t queueSize) {
  auto buffer = reinterpret_cast<uint8_t *>(malloc(queueSize));
#ifdef SHAPING
  auto nextCheck = std::chrono::steady_clock::now();

  while (true) {
    nextCheck += std::chrono::microseconds(interval);
//    std::this_thread::sleep_for(std::chrono::microseconds(interval));
    std::this_thread::sleep_until(nextCheck);
#else
    while (true) {
#endif
    auto size = dummyQueues.fromShaped->size();
    if (size > 0)
      std::cout << "Popping dummies of size: "
                << dummyQueues.fromShaped->size() << std::endl;
    dummyQueues.fromShaped->pop(buffer, dummyQueues.fromShaped->size());
    for (const auto &[queues, client]: *queuesToClient) {
      if (client == nullptr) continue;
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
        queues.fromShaped->pop(buffer, size);
        client->sendData(buffer, size);
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