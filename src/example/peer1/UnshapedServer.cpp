//
// Created by Rut Vora
//

#include <iomanip>
#include <utility>
#include "UnshapedServer.h"

UnshapedServer::UnshapedServer(std::string &appName, int maxClients,
                               logLevels logLevel,
                               __useconds_t shapedClientLoopInterval,
                               config::UnshapedServer &config) :
    appName(appName), logLevel(logLevel), serverAddr(config.serverAddr),
    shapedClientLoopInterval(shapedClientLoopInterval), maxClients(maxClients),
    sigInfo(nullptr) {
  socketToQueues = new std::unordered_map<int, QueuePair>(maxClients);
  queuesToSocket = new std::unordered_map<QueuePair,
      int, QueuePairHash>(maxClients);
  pendingSignal =
      new std::unordered_map<uint64_t, connectionStatus>(maxClients);
  unassignedQueues = new std::queue<QueuePair>{};

  initialiseSHM();

  // Start listening for unshaped traffic
  auto tcpReceiveFunc = [this](auto &&PH1, auto &&PH2, auto &&PH3, auto &&PH4,
                               auto &&PH5) {
    return receivedUnshapedData(std::forward<decltype(PH1)>(PH1),
                                std::forward<decltype(PH2)>(PH2),
                                std::forward<decltype(PH3)>(PH3),
                                std::forward<decltype(PH4)>(PH4),
                                std::forward<decltype(PH5)>(PH5));
  };

  unshapedServer = new TCP::Server{config.bindAddr, config.bindPort,
                                   tcpReceiveFunc, logLevel};
  unshapedServer->startListening();

  std::thread responseLoop([=, this]() {
    receivedResponse(config.checkResponseLoopInterval);
  });
  responseLoop.detach();

//  std::signal(SIGUSR1, handleQueueSignal);
}

[[noreturn]] void UnshapedServer::receivedResponse(__useconds_t interval) {
#ifdef SHAPING
  auto nextCheck = std::chrono::steady_clock::now();
  while (true) {
    nextCheck += std::chrono::microseconds(interval);
//    std::this_thread::sleep_for(std::chrono::microseconds(interval));
    std::this_thread::sleep_until(nextCheck);
#else
  while (true) {
#endif
    mapLock.lock_shared();
    auto tempMap = *queuesToSocket;
    mapLock.unlock_shared();
    for (const auto &[queues, socket]: tempMap) {
//      if (socket == 0) continue;
      auto size = queues.fromShaped->size();
      if (size == 0) {
        if ((*pendingSignal)[queues.fromShaped->ID] == FIN) {
#ifdef DEBUGGING
          log(DEBUG,
                "Sending FIN to socket " + std::to_string(socket) +
                " mapped to queues {" +
                std::to_string(queues.fromShaped->ID) + "," +
                std::to_string(queues.toShaped->ID) + "}");
#endif
          TCP::Server::sendFIN(socket);
          (*pendingSignal).erase(queues.fromShaped->ID);
          queues.fromShaped->sentFIN = true;
        }
        if (queues.toShaped->markedForDeletion
            && queues.fromShaped->markedForDeletion
            && queues.fromShaped->sentFIN) {
          eraseMapping(socket);
        }
      }
      if (size > 0) {
        auto buffer = reinterpret_cast<uint8_t *>(malloc(size));
        queues.fromShaped->pop(buffer, size);
        auto sentBytes =
            unshapedServer->sendData(socket, buffer, size);
        if (sentBytes > 0 && (size_t) sentBytes == size) free(buffer);
      }
    }
  }
}

inline QueuePair
UnshapedServer::assignQueue(int clientSocket, std::string &clientAddress,
                            std::string serverAddress) {
  if (unassignedQueues->empty()) return {nullptr, nullptr};
  mapLock.lock();
  auto queues = unassignedQueues->front();
  unassignedQueues->pop();
#ifdef DEBUGGING
  log(DEBUG, "Assigning socket " +
             std::to_string(clientSocket) + " (client: " + clientAddress
             + ") to queues {" + std::to_string(queues.fromShaped->ID) +
             "," + std::to_string(queues.toShaped->ID) + "}");
#endif
  // No socket attached to this queue pair
  (*socketToQueues)[clientSocket] = queues;
  (*queuesToSocket)[queues] = clientSocket;
  mapLock.unlock();
  // Set client of queue to the new client
  auto address = clientAddress.substr(0, clientAddress.find(':'));
  auto port = clientAddress.substr(address.size() + 1);
  queues.toShaped->markedForDeletion = false;
  queues.fromShaped->markedForDeletion = false;
  queues.toShaped->sentFIN = queues.fromShaped->sentFIN = false;
  queues.toShaped->clear();
  queues.fromShaped->clear();

  std::strcpy(queues.fromShaped->addrPair.clientAddress, address.c_str());
  std::strcpy(queues.fromShaped->addrPair.clientPort, port.c_str());
  std::strcpy(queues.toShaped->addrPair.clientAddress, address.c_str());
  std::strcpy(queues.toShaped->addrPair.clientPort, port.c_str());

  // Set server of queue to given server
  address = serverAddress.substr(0, serverAddress.find(':'));
  port = serverAddress.substr((address.size() + 1));
  std::strcpy(queues.fromShaped->addrPair.serverAddress, address.c_str());
  std::strcpy(queues.fromShaped->addrPair.serverPort, port.c_str());
  std::strcpy(queues.toShaped->addrPair.serverAddress, address.c_str());
  std::strcpy(queues.toShaped->addrPair.serverPort, port.c_str());
  return queues;
}

inline void UnshapedServer::eraseMapping(int socket) {
  auto queues = (*socketToQueues)[socket];
  if (!queues.fromShaped->markedForDeletion
      || !queues.toShaped->markedForDeletion) {
    log(ERROR, "eraseMapping called before both queues were marked for "
               "deletion");
    return;
  }
#ifdef DEBUGGING
  log(DEBUG, "Erase Mapping of socket " + std::to_string(socket)
             + " mapped to queues {" + std::to_string(queues.fromShaped->ID) +
             "," + std::to_string(queues.toShaped->ID) + "}");
#endif
  close(socket);
  mapLock.lock();
  (*socketToQueues).erase(socket);
  (*queuesToSocket).erase(queues);
  unassignedQueues->push(queues);
  mapLock.unlock();
}

void UnshapedServer::signalShapedProcess(uint64_t queueID,
                                         connectionStatus connStatus) {
  std::scoped_lock lock(writeLock);
  struct SignalInfo::queueInfo queueInfo{queueID, connStatus};
  sigInfo->enqueue(SignalInfo::toShaped, queueInfo);

  // Signal the other process (does not actually kill the shaped process)
  kill(sigInfo->shaped, SIGUSR1);
}

void UnshapedServer::handleQueueSignal(int signum) {
  if (signum == SIGUSR1) {
    std::scoped_lock lock(readLock);
    struct SignalInfo::queueInfo queueInfo{};
    while (sigInfo->dequeue(SignalInfo::fromShaped, queueInfo)) {
      if (queueInfo.connStatus == FIN) {
        (*pendingSignal)[queueInfo.queueID] = FIN;
      }
    }
  }
}

bool UnshapedServer::receivedUnshapedData(int fromSocket,
                                          std::string &clientAddress,
                                          uint8_t *buffer, size_t length, enum
                                              connectionStatus connStatus) {

  switch (connStatus) {
    case SYN: {
      auto queues = assignQueue(fromSocket, clientAddress, serverAddr);
      if (queues.toShaped == nullptr) {
        log(ERROR, "More clients than configured for!");
        return false;
      }
//      mapLock.lock_shared();
//      queues = (*socketToQueues)[fromSocket];
//      mapLock.unlock_shared();
#ifdef DEBUGGING
      {
        log(DEBUG, "Received SYN from socket " + std::to_string(fromSocket)
                   + " (client: " + clientAddress + ") mapped to {" +
                   std::to_string(queues.fromShaped->ID) + "," +
                   std::to_string(queues.toShaped->ID) + "}");
      }
#endif
      signalShapedProcess(queues.toShaped->ID, SYN);
      return true;
    }
    case ONGOING: {
      mapLock.lock_shared();
      auto queues = (*socketToQueues)[fromSocket];
      mapLock.unlock_shared();
      auto toShaped = queues.toShaped;
      while (toShaped->push(buffer, length) == -1) {
        log(WARNING, "(toShaped) " + std::to_string(toShaped->ID) +
                     +" mapped to socket " + std::to_string(fromSocket) +
                     " is full, waiting for it to be empty!");
#ifdef SHAPING
        // Sleep for some time. For performance reasons, this is the same as
        // the interval with which DP Logic thread runs in Shaped component.
        std::this_thread::sleep_for(
            std::chrono::microseconds(shapedClientLoopInterval));
#endif
      }
      return true;
    }
    case FIN: {
      mapLock.lock_shared();
      auto queues = (*socketToQueues)[fromSocket];
      mapLock.unlock_shared();
#ifdef DEBUGGING
      log(DEBUG, "Received FIN from socket " + std::to_string(fromSocket)
                 + " (client: " + clientAddress + ") mapped to {" +
                 std::to_string(queues.fromShaped->ID) + "," +
                 std::to_string(queues.toShaped->ID) + "}");
#endif
      queues.toShaped->markedForDeletion = true;
      signalShapedProcess(queues.toShaped->ID, FIN);
      return true;
    }

    default:
      return false; // Just to satisfy the compiler
  }
}

inline void UnshapedServer::initialiseSHM() {
  auto shmAddr = helpers::initialiseSHM(maxClients, appName);

  // The beginning of the SHM contains the signalStruct struct
  sigInfo = new(shmAddr) SignalInfo{};
  sigInfo->unshaped = getpid();

  // The rest of the SHM contains the queues
  shmAddr += sizeof(class SignalInfo);
  for (int i = 0; i < maxClients * 2; i += 2) {
    // Initialise a queue class at that shared memory and put it in the maps
    auto queue1 =
        new(shmAddr + (i * sizeof(class LamportQueue))) LamportQueue(i);
    auto queue2 =
        new(shmAddr + ((i + 1) * sizeof(class LamportQueue)))
            LamportQueue(i + 1);
    unassignedQueues->push({queue1, queue2});
  }
}

void UnshapedServer::log(logLevels level, const std::string &log) {
  if (logLevel < level) return;
  std::string levelStr;
  switch (level) {
    case DEBUG:
      levelStr = "UnshapedServer:DEBUG: ";
      break;
    case ERROR:
      levelStr = "UnshapedServer:ERROR: ";
      break;
    case WARNING:
      levelStr = "UnshapedServer:WARNING: ";
      break;

  }
  auto time = std::time(nullptr);
  auto localTime = std::localtime(&time);
  std::scoped_lock lock(logWriter);
  std::cerr << std::put_time(localTime, "[%H:%M:%S] ")
            << levelStr << log << std::endl;
}