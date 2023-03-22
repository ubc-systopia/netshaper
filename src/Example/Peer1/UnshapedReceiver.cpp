//
// Created by Rut Vora
//

#include <iomanip>
#include "UnshapedReceiver.h"

UnshapedReceiver::UnshapedReceiver(std::string &appName, int maxClients,
                                   std::string bindAddr, uint16_t bindPort,
                                   __useconds_t checkResponseInterval) :
    appName(appName), logLevel(DEBUG), maxClients(maxClients), sigInfo
    (nullptr) {
  socketToQueues = new std::unordered_map<int, QueuePair>(maxClients);
  queuesToSocket = new std::unordered_map<QueuePair,
      int, QueuePairHash>(maxClients);
  pendingSignal =
      new std::unordered_map<uint64_t, connectionStatus>(maxClients);

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

  unshapedReceiver = new TCP::Receiver{std::move(bindAddr), bindPort,
                                       tcpReceiveFunc, WARNING};
  unshapedReceiver->startListening();

  std::thread responseLoop([=, this]() {
    receivedResponse(checkResponseInterval);
  });
  responseLoop.detach();

//  std::signal(SIGUSR1, handleQueueSignal);
}

[[noreturn]] void UnshapedReceiver::receivedResponse(__useconds_t interval) {
  auto nextCheck = std::chrono::steady_clock::now();
  while (true) {
    nextCheck += std::chrono::microseconds(interval);
//    std::this_thread::sleep_for(std::chrono::microseconds(interval));
    std::this_thread::sleep_until(nextCheck);
    for (const auto &[queues, socket]: *queuesToSocket) {
      if (socket == 0) continue;
      auto size = queues.fromShaped->size();
      if (size == 0) {
        if ((*pendingSignal)[queues.fromShaped->ID] == FIN) {
          log(DEBUG,
              "Sending FIN to socket " + std::to_string(socket) +
              " mapped to queues {" +
              std::to_string(queues.fromShaped->ID) + "," +
              std::to_string(queues.toShaped->ID) + "}");
          TCP::Receiver::sendFIN(socket);
          (*pendingSignal).erase(queues.fromShaped->ID);
        }
        // TODO: This definitely causes an issue. Find a better time to close
        //  socket
        if (queues.toShaped->markedForDeletion
            && queues.fromShaped->markedForDeletion) {
          eraseMapping(socket);
        }
      }
      if (size > 0) {
        auto buffer = reinterpret_cast<uint8_t *>(malloc(size));
        queues.fromShaped->pop(buffer, size);
        auto sentBytes =
            unshapedReceiver->sendData(socket, buffer, size);
        if (sentBytes > 0 && (size_t) sentBytes == size) free(buffer);
      }
    }
  }
}

inline bool
UnshapedReceiver::assignQueue(int clientSocket, std::string &clientAddress,
                              std::string serverAddress) {
  // Find an unused queue and map it
  return std::ranges::any_of(*queuesToSocket, [&](auto &iterator) {
    // iterator.first is QueuePair, iterator.second is socket
    QueuePair queues = iterator.first;
    if (iterator.second == 0 && queues.toShaped->size() == 0) {
      log(DEBUG, "Assigning socket " +
                 std::to_string(clientSocket) + " (client: " + clientAddress
                 + ") to queues {" + std::to_string(queues.fromShaped->ID) +
                 "," + std::to_string(queues.toShaped->ID) + "}");
      // No socket attached to this queue pair
      (*socketToQueues)[clientSocket] = iterator.first;
      (*queuesToSocket)[iterator.first] = clientSocket;
      // Set client of queue to the new client
      auto address = clientAddress.substr(0, clientAddress.find(':'));
      auto port = clientAddress.substr(address.size() + 1);
      queues.toShaped->markedForDeletion = false;
      queues.fromShaped->markedForDeletion = false;
      queues.toShaped->clear();
      queues.fromShaped->clear();

      std::strcpy(queues.fromShaped->clientAddress, address.c_str());
      std::strcpy(queues.fromShaped->clientPort, port.c_str());
      std::strcpy(queues.toShaped->clientAddress, address.c_str());
      std::strcpy(queues.toShaped->clientPort, port.c_str());

      // Set server of queue to given server
      address = serverAddress.substr(0, serverAddress.find(':'));
      port = serverAddress.substr((address.size() + 1));
      std::strcpy(queues.fromShaped->serverAddress, address.c_str());
      std::strcpy(queues.fromShaped->serverPort, port.c_str());
      std::strcpy(queues.toShaped->serverAddress, address.c_str());
      std::strcpy(queues.toShaped->serverPort, port.c_str());
      return true;
    }
    return false;
  });
}

inline void UnshapedReceiver::eraseMapping(int socket) {
  auto queues = (*socketToQueues)[socket];
  if (!queues.fromShaped->markedForDeletion
      || !queues.toShaped->markedForDeletion) {
    log(ERROR, "eraseMapping called before both queues were marked for "
               "deletion");
  }
  log(DEBUG, "Erase Mapping of socket " + std::to_string(socket)
             + " mapped to queues {" + std::to_string(queues.fromShaped->ID) +
             "," + std::to_string(queues.toShaped->ID) + "}");
  close(socket);
  (*socketToQueues).erase(socket);
  (*queuesToSocket)[queues] = 0;
}

void UnshapedReceiver::signalShapedProcess(uint64_t queueID,
                                           connectionStatus connStatus) {
  std::scoped_lock lock(writeLock);
  struct SignalInfo::queueInfo queueInfo{queueID, connStatus};
  sigInfo->enqueue(SignalInfo::toShaped, queueInfo);

  // Signal the other process (does not actually kill the shaped process)
  kill(sigInfo->shaped, SIGUSR1);
}

void UnshapedReceiver::handleQueueSignal(int signum) {
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

bool UnshapedReceiver::receivedUnshapedData(int fromSocket,
                                            std::string &clientAddress,
                                            uint8_t *buffer, size_t length, enum
                                                connectionStatus connStatus) {

  switch (connStatus) {
    case SYN: {
      if (!assignQueue(fromSocket, clientAddress)) {
        log(ERROR, "More clients than configured for!");
//        for (auto &pair: *socketToQueues) {
//          std::cout << pair.first << " --> {" << pair.second.fromShaped->ID
//                    << "," << pair.second.toShaped->ID << "}" << std::endl;
//        }
//        exit(1);
        return false;
      }
      {
        auto &queues = (*socketToQueues)[fromSocket];
        log(DEBUG, "Received SYN from socket " + std::to_string(fromSocket)
                   + " (client: " + clientAddress + ") mapped to {" +
                   std::to_string(queues.fromShaped->ID) + "," +
                   std::to_string(queues.toShaped->ID) + "}");
      }
      signalShapedProcess((*socketToQueues)[fromSocket].toShaped->ID, SYN);
      return true;
    }
    case ONGOING: {
//      log(DEBUG, "Received Data on socket: " + std::to_string(fromSocket));
      auto toShaped = (*socketToQueues)[fromSocket].toShaped;
      while (toShaped->push(buffer, length) == -1) {
        log(WARNING, "(toShaped) " + std::to_string(toShaped->ID) +
                     +" mapped to socket " + std::to_string(fromSocket) +
                     " is full, waiting for it to be empty!");

        // Sleep for some time. For performance reasons, this is the same as
        // the interval with which DP Logic thread runs in Shaped component.
        std::this_thread::sleep_for(std::chrono::microseconds(500000));
      }
      return true;
    }
    case FIN: {
      auto &queues = (*socketToQueues)[fromSocket];
      log(DEBUG, "Received FIN from socket " + std::to_string(fromSocket)
                 + " (client: " + clientAddress + ") mapped to {" +
                 std::to_string(queues.fromShaped->ID) + "," +
                 std::to_string(queues.toShaped->ID) + "}");
      queues.toShaped->markedForDeletion = true;
      signalShapedProcess((*socketToQueues)[fromSocket].toShaped->ID, FIN);
      return true;
    }

    default:
      return false; // Just to satisfy the compiler
  }
}

inline void UnshapedReceiver::initialiseSHM() {
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
    (*queuesToSocket)[{queue1, queue2}] = 0;
  }
}

void UnshapedReceiver::log(logLevels level, const std::string &log) {
  if (logLevel < level) return;
  std::string levelStr;
  switch (level) {
    case DEBUG:
      levelStr = "UR:DEBUG: ";
      break;
    case ERROR:
      levelStr = "UR:ERROR: ";
      break;
    case WARNING:
      levelStr = "UR:WARNING: ";
      break;

  }
  auto time = std::time(nullptr);
  auto localTime = std::localtime(&time);
  std::scoped_lock lock(logWriter);
  std::cerr << std::put_time(localTime, "[%H:%M:%S] ")
            << levelStr << log << std::endl;
}