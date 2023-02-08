//
// Created by Rut Vora
//

#include "ShapedSender.h"

ShapedSender::ShapedSender(std::string &appName, int maxClients,
                           double epsilon, double sensitivity,
                           const std::string &peer2IP, uint16_t peer2Port,
                           uint64_t idleTimeout,
                           __useconds_t DPCreditorLoopInterval,
                           __useconds_t senderLoopInterval) :
    appName(appName), maxClients(maxClients), sigInfo(nullptr), dummyStream
    (nullptr), controlStream(nullptr), dummyStreamID(QUIC_UINT62_MAX),
    controlStreamID
        (QUIC_UINT62_MAX) {
  queuesToStream =
      new std::unordered_map<QueuePair,
          MsQuicStream *, QueuePairHash>(maxClients);
  streamToQueues =
      new std::unordered_map<MsQuicStream *, QueuePair>(maxClients);

  noiseGenerator = new NoiseGenerator{epsilon, sensitivity};
  // Connect to the other middlebox

  auto onResponseFunc = [this](auto &&PH1, auto &&PH2, auto &&PH3) {
    onResponse(std::forward<decltype(PH1)>(PH1),
               std::forward<decltype(PH2)>(PH2),
               std::forward<decltype(PH3)>(PH3));
  };

  // Two middle-boxes are connected in a client-server setup, where peer1 middlebox is
  // the client and peer2 middlebox is the server. In middlebox 1 we have
  // shapedSender and in middlebox 2 we have shapedReceiver
  shapedSender = new QUIC::Sender{peer2IP, peer2Port, onResponseFunc,
                                  true,
                                  QUIC::Sender::WARNING,
                                  idleTimeout};

  // We map a pair of queues over the shared memory region to every stream
  // CAUTION: we assume the shared queues are already initialized in unshaped process
  initialiseSHM();

  // Start the control stream
  startControlStream();

  // Start the dummy stream
  startDummyStream();
  sleep(2);

  std::thread dpCreditorThread(helpers::DPCreditor, &sendingCredit,
                               queuesToStream, noiseGenerator,
                               DPCreditorLoopInterval);
  dpCreditorThread.detach();

//  auto sendShapedData = std::bind(&ShapedSender::sendShapedData, this,
//                                  std::placeholders::_1);
  std::thread sendingThread(helpers::sendShapedData, &sendingCredit,
                            queuesToStream,
                            [this](auto &&PH1) {
                              sendDummy(std::forward<decltype(PH1)>(PH1));
                            },
                            [this](auto &&PH1) {
                              sendData(std::forward<decltype(PH1)>(PH1));
                            },
                            senderLoopInterval);

  sendingThread.detach();
}

QueuePair ShapedSender::findQueuesByID(uint64_t queueID) {
  for (auto &iterator: *queuesToStream) {
    if (iterator.first.toShaped->queueID == queueID) {
      return iterator.first;
    }
  }
  return {nullptr, nullptr};
}

void ShapedSender::handleQueueSignal(int signum) {
  if (signum == SIGUSR1) {
    std::scoped_lock lock(readLock);
    struct SignalInfo::queueInfo queueInfo{};
    while (sigInfo->dequeue(SignalInfo::toShaped, queueInfo)) {
      auto queues = findQueuesByID(queueInfo.queueID);

      auto *message =
          reinterpret_cast<struct ControlMessage *>(malloc(
              sizeof(struct ControlMessage)));
      (*queuesToStream)[queues]->GetID(&message->streamID);
      message->streamType = Data;
      message->connStatus = ONGOING;  // For fallback purposes only

      switch (queueInfo.connStatus) {
        case NEW:
          message->connStatus = NEW;
          std::strcpy(message->srcIP, queues.toShaped->clientAddress);
          std::strcpy(message->srcPort, queues.toShaped->clientPort);
          std::strcpy(message->destIP, queues.toShaped->serverAddress);
          std::strcpy(message->destPort, queues.toShaped->serverPort);
          break;
        case TERMINATED:
          // Do Nothing as the queue is marked for deletion already.
          // We will send a control stream message in the "sendData" function
          // when the queue is emptied out
        case ONGOING:
          // This should never happen (unshaped process never sends this)
          break;
      }
      shapedSender->send(controlStream,
                         reinterpret_cast<uint8_t *>(message),
                         sizeof(*message));
    }
  }
}

inline void ShapedSender::initialiseSHM() {
  auto shmAddr = helpers::initialiseSHM(maxClients, appName, true);

  // The beginning of the SHM contains the signalStruct struct
  sigInfo = reinterpret_cast<class SignalInfo *>(shmAddr);
  if (sigInfo->unshaped == 0) {
    std::cerr << "Start the unshaped process first" << std::endl;
    exit(1);
  }
  sigInfo->shaped = getpid();

  // The rest of the SHM contains the queues
  shmAddr += sizeof(class SignalInfo);
  for (int i = 0; i < maxClients * 2; i += 2) {
    auto queue1 =
        (LamportQueue *) (shmAddr + (i * sizeof(class LamportQueue)));
    auto queue2 =
        (LamportQueue *) (shmAddr + ((i + 1) * sizeof(class LamportQueue)));
    auto stream = shapedSender->startStream();

    // Data streams
    (*queuesToStream)[{queue1, queue2}] = stream;
    (*streamToQueues)[stream] = {queue1, queue2};
  }
}

void ShapedSender::signalUnshapedProcess(uint64_t queueID,
                                         connectionStatus connStatus) {
  std::scoped_lock lock(writeLock);
  struct SignalInfo::queueInfo queueInfo{queueID, connStatus};
  sigInfo->enqueue(SignalInfo::fromShaped, queueInfo);
  //TODO: Handle case when queue is full

  // Signal the other process (does not actually kill the shaped process)
  kill(sigInfo->unshaped, SIGUSR1);
}

void ShapedSender::sendData(size_t dataSize) {
  // TODO: Add prioritisation
  for (auto &iterator: *queuesToStream) {
    auto toShaped = iterator.first.toShaped;
    auto size = toShaped->size();
    if (size == 0 || dataSize == 0) {
      if (toShaped->markedForDeletion) {
        std::cout << "Peer1:Shaped: Queue is marked for deletion" << std::endl;
        // Send a termination control message
        auto *message =
            reinterpret_cast<struct ControlMessage *>(malloc(sizeof(struct
                ControlMessage)));
        iterator.second->GetID(&message->streamID);
        message->streamType = Data;
        message->connStatus = TERMINATED;
        shapedSender->send(controlStream,
                           reinterpret_cast<uint8_t *>(message),
                           sizeof(*message));

        // Signal the unshaped process to clear the queue and related mappings
        signalUnshapedProcess(toShaped->queueID, TERMINATED);
      }
      continue; //TODO: Send at least 1 byte
    }

    auto sizeToSend = std::min(dataSize, size);
    auto buffer = reinterpret_cast<uint8_t *>(malloc(sizeToSend));
    iterator.first.toShaped->pop(buffer, sizeToSend);
    shapedSender->send(iterator.second, buffer, sizeToSend);
    QUIC_UINT62 streamID;
    iterator.second->GetID(&streamID);
    std::cout << "Peer1:Shaped: Sending actual data on " << streamID <<
              std::endl;
    dataSize -= sizeToSend;
  }
}

void ShapedSender::sendDummy(size_t dummySize) {
  uint8_t dummy[dummySize];
  memset(dummy, 0, dummySize);
  dummyStream->GetID(&dummyStreamID);
//          std::cout << "Peer1:Shaped: Sending dummy on " << dummyStreamID
//                    << std::endl;
  shapedSender->send(dummyStream,
                     dummy, dummySize);
}

void
ShapedSender::onResponse(MsQuicStream *stream, uint8_t *buffer, size_t length) {
  // (void) (stream);
  uint64_t streamId;
  stream->GetID(&streamId);
  if (streamId != dummyStreamID && streamId != controlStreamID) {
    for (auto &iterator: *streamToQueues) {
      // If queue marked for deletion, no point in sending more data to
      // client (client already terminated). Unshaped side will clear it
      if (iterator.second.fromShaped != nullptr && !iterator.second
          .fromShaped->markedForDeletion) {
        // TODO: Check if this buffer needs to be freed
        iterator.second.fromShaped->push(buffer, length);
      }
    }
    std::cout << "Peer1:Shaped: Received response on " << streamId << std::endl;
  } else if (streamId == dummyStreamID) {
//    std::cout << "Peer1:Shaped: Received dummy from QUIC Server" << std::endl;
  } else if (streamId == controlStreamID) {
    std::cout << "Peer1:Shaped: Received control message from QUIC Server"
              << std::endl;
  }
}

inline void ShapedSender::startControlStream() {
  controlStream = shapedSender->startStream();
  auto *message =
      reinterpret_cast<struct ControlMessage *>(malloc(sizeof(struct
          ControlMessage)));
  controlStream->GetID(&message->streamID);
  // save the control stream ID for later
  controlStreamID = message->streamID;
  message->streamType = Control;
  shapedSender->send(controlStream,
                     reinterpret_cast<uint8_t *>(message),
                     sizeof(*message));
  std::cout << "Peer1:Shaped: Starting control stream at " << message->streamID
            << std::endl;
}

inline void ShapedSender::startDummyStream() {
  dummyStream = shapedSender->startStream();
  auto *message =
      reinterpret_cast<struct ControlMessage *>(malloc(sizeof(struct
          ControlMessage)));
  dummyStream->GetID(&message->streamID);
  // save the dummy stream ID for later
  dummyStreamID = message->streamID;
  message->streamType = Dummy;
  shapedSender->send(controlStream,
                     reinterpret_cast<uint8_t *>(message),
                     sizeof(*message));
  std::cout << "Peer1:Shaped: Starting dummy stream at " << message->streamID
            << std::endl;
}