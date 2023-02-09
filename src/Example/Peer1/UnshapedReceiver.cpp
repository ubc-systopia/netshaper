//
// Created by Rut Vora
//

#include "UnshapedReceiver.h"

UnshapedReceiver::UnshapedReceiver(std::string &appName, int maxClients,
                                   std::string bindAddr, uint16_t bindPort,
                                   __useconds_t checkResponseInterval) :
    appName(appName), maxClients(maxClients), sigInfo(nullptr) {
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
                                       tcpReceiveFunc};
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
      auto size = iterator.first.fromShaped->size();
      if (size > 0) {
        std::cout << "Peer1:Unshaped: Got data in queue: " <<
                  iterator.first.fromShaped->queueID << std::endl;
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
UnshapedReceiver::assignQueue(int fromSocket, std::string &clientAddress,
                              std::string serverAddress) {
  // Find an unused queue and map it
  return std::ranges::any_of(*queuesToSocket, [&](auto &iterator) {
    // iterator.first is QueuePair, iterator.second is socket
    if (iterator.second == 0) {
      // No socket attached to this queue pair
      (*socketToQueues)[fromSocket] = iterator.first;
      (*queuesToSocket)[iterator.first] = fromSocket;
      // Set client of queue to the new client
      auto address = clientAddress.substr(0, clientAddress.find(':'));
      auto port = clientAddress.substr(address.size() + 1);
      QueuePair queues = iterator.first;
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
    if (iterator.first.toShaped->queueID == queueID) {
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
  //TODO: Handle case when queue is full

  // Signal the other process (does not actually kill the shaped process)
  kill(sigInfo->shaped, SIGUSR1);
}

void UnshapedReceiver::handleQueueSignal(int signum) {
  if (signum == SIGUSR1) {
    std::scoped_lock lock(readLock);
    struct SignalInfo::queueInfo queueInfo{};
    while (sigInfo->dequeue(SignalInfo::fromShaped, queueInfo)) {
      if (queueInfo.connStatus != TERMINATED)
        std::cerr << "Peer1:Unshaped: Wrong signal from Shaped process" <<
                  std::endl;
      auto queues = findQueuesByID(queueInfo.queueID);
      queues.toShaped->clear();
      queues.fromShaped->clear();
      (*socketToQueues).erase((*queuesToSocket)[queues]);
      (*queuesToSocket)[queues] = 0;
      std::cout << "Peer1:Unshaped: Mapping removed on termination" <<
                std::endl;
      queues.toShaped->markedForDeletion = false;
      queues.fromShaped->markedForDeletion = false;

    }
  }
}

bool UnshapedReceiver::receivedUnshapedData(int fromSocket,
                                            std::string &clientAddress,
                                            uint8_t *buffer, size_t length, enum
                                                connectionStatus connStatus) {

  switch (connStatus) {
    case NEW: {
      if (!assignQueue(fromSocket, clientAddress)) {
        std::cerr << "More clients than expected!" << std::endl;
        return false;
      }
      signalShapedProcess((*socketToQueues)[fromSocket].toShaped->queueID, NEW);
      return true;
    }
    case ONGOING: {
      // TODO: Send an ACK back only after pushing!
      auto toShaped = (*socketToQueues)[fromSocket].toShaped;
      while (toShaped->push(buffer, length) == -1) {
        std::cerr << "Queue for client " << toShaped->clientAddress
                  << ":" << toShaped->clientPort
                  << " is full. Waiting for it to be empty"
                  << std::endl;
        // Sleep for some time. For performance reasons, this is the same as
        // the interval with which DP Logic thread runs in Shaped component.
        std::this_thread::sleep_for(std::chrono::microseconds(500000));
      }
      return true;
    }
    case TERMINATED: {
      auto toShaped = (*socketToQueues)[fromSocket].toShaped;
      auto fromShaped = (*socketToQueues)[fromSocket].fromShaped;
      toShaped->markedForDeletion = true;
      fromShaped->markedForDeletion = true;
      signalShapedProcess(toShaped->queueID, TERMINATED);
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