//
// Created by Rut Vora
//

#include "UnshapedReceiver.h"

UnshapedReceiver::UnshapedReceiver(std::string &appName, int maxClients,
                                   std::string bindAddr, uint16_t bindPort,
                                   __useconds_t checkResponseInterval) :
    appName(appName), logLevel(DEBUG), maxClients(maxClients), sigInfo
    (nullptr) {
  socketToQueues = new std::unordered_map<int, QueuePair>(maxClients);
  queuesToSocket = new std::unordered_map<QueuePair,
      int, QueuePairHash>(maxClients);

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
  while (true) {
    std::this_thread::sleep_for(std::chrono::microseconds(interval));
    for (auto &iterator: *queuesToSocket) {
      if (iterator.second == 0) continue;
      auto size = iterator.first.fromShaped->size();
      if (size == 0 && iterator.first.fromShaped->markedForDeletion) {
        unshapedReceiver->sendFIN(iterator.second);
        // TODO: This definitely causes an issue. Find a better time to close
        //  socket
        if (iterator.first.toShaped->markedForDeletion) {
          log(DEBUG, "Erase Mapping of (toShaped) " +
                     std::to_string(iterator.first.toShaped->ID));
//          (*socketToQueues).erase(iterator.second);
//          iterator.second = 0;
        }
      }
      if (size > 0) {
        log(DEBUG, "Received data in (fromShaped) " +
                   std::to_string(iterator.first.fromShaped->ID));

        auto buffer = reinterpret_cast<uint8_t *>(malloc(size));
        iterator.first.fromShaped->pop(buffer, size);
        auto sentBytes =
            unshapedReceiver->sendData(iterator.second, buffer, size);
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
    if (iterator.second == 0) {
      QueuePair queues = iterator.first;
      log(DEBUG, "Assigning socket " +
                 std::to_string(clientSocket) +
                 " to queues {" + std::to_string(queues.fromShaped->ID) + "," +
                 std::to_string(queues.toShaped->ID) + "}");
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

QueuePair UnshapedReceiver::findQueuesByID(uint64_t queueID) {
  for (auto &iterator: *queuesToSocket) {
    if (iterator.first.toShaped->ID == queueID) {
      return iterator.first;
    }
  }
  return {nullptr, nullptr};
}

void UnshapedReceiver::signalShapedProcess(uint64_t queueID,
                                           connectionStatus connStatus) {
  std::scoped_lock lock(writeLock);
  struct SignalInfo::queueInfo queueInfo{queueID, connStatus};
  sigInfo->enqueue(SignalInfo::toShaped, queueInfo);

  // Signal the other process (does not actually kill the shaped process)
  kill(sigInfo->shaped, SIGUSR1);
}

//void UnshapedReceiver::handleQueueSignal(int signum) {
//  if (signum == SIGUSR1) {
//    std::scoped_lock lock(readLock);
//    struct SignalInfo::queueInfo queueInfo{};
//    while (sigInfo->dequeue(SignalInfo::fromShaped, queueInfo)) {
//      if (queueInfo.connStatus != FIN)
//        std::cerr << "Peer1:Unshaped: Wrong signal from Shaped process" <<
//                  std::endl;
//      std::cout << "Peer1:Unshaped: Mapping removed on termination" <<
//                std::endl;
//
//    }
//  }
//}

bool UnshapedReceiver::receivedUnshapedData(int fromSocket,
                                            std::string &clientAddress,
                                            uint8_t *buffer, size_t length, enum
                                                connectionStatus connStatus) {

  switch (connStatus) {
    case SYN: {
      if (!assignQueue(fromSocket, clientAddress)) {
        log(ERROR, "More clients than configured for!");
        return false;
      }
      signalShapedProcess((*socketToQueues)[fromSocket].toShaped->ID, SYN);
      return true;
    }
    case ONGOING: {
      auto toShaped = (*socketToQueues)[fromSocket].toShaped;
      while (toShaped->push(buffer, length) == -1) {
        log(WARNING, "(toShaped) " + std::to_string(toShaped->ID) +
                     " is full, waiting for it to be empty!");

        // Sleep for some time. For performance reasons, this is the same as
        // the interval with which DP Logic thread runs in Shaped component.
        std::this_thread::sleep_for(std::chrono::microseconds(500000));
      }
      return true;
    }
    case FIN: {
      auto &queues = (*socketToQueues)[fromSocket];
      log(DEBUG, "Received FIN on socket " + std::to_string(fromSocket)
                 + " mapped to {" + std::to_string(queues.toShaped->ID) +
                 "," +
                 std::to_string(queues.fromShaped->ID) + "}");
      queues.toShaped->markedForDeletion = true;
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
  if (logLevel >= level) {

    std::cerr << levelStr << log << std::endl;
  }
}