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
    appName(appName), logLevel(DEBUG), maxClients(maxClients),
    sigInfo(nullptr), dummyStream(nullptr), controlStream(nullptr),
    dummyStreamID(QUIC_UINT62_MAX), controlStreamID(QUIC_UINT62_MAX) {
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
                                  WARNING,
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
    if (iterator.first.toShaped->ID == queueID) {
      return iterator.first;
    }
  }
  return {nullptr, nullptr};
}

MsQuicStream *ShapedSender::findStreamByID(QUIC_UINT62 ID) {
  QUIC_UINT62 streamID;
  for (auto &iterator: *streamToQueues) {
    if (iterator.first == nullptr) continue;
    iterator.first->GetID(&streamID);
    if (streamID == ID) return iterator.first;
  }
  return nullptr;
}

void ShapedSender::handleQueueSignal(int signum) {
  if (signum == SIGUSR1) {
    std::scoped_lock lock(readLock);
    struct SignalInfo::queueInfo queueInfo{};
    while (sigInfo->dequeue(SignalInfo::toShaped, queueInfo)) {
      auto queues = findQueuesByID(queueInfo.queueID);

      if (queueInfo.connStatus == SYN) {
        auto *message =
            reinterpret_cast<struct ControlMessage *>(malloc(
                sizeof(struct ControlMessage)));
        (*queuesToStream)[queues]->GetID(&message->streamID);
        message->streamType = Data;
        message->connStatus = SYN;
        std::strcpy(message->srcIP, queues.toShaped->clientAddress);
        std::strcpy(message->srcPort, queues.toShaped->clientPort);
        std::strcpy(message->destIP, queues.toShaped->serverAddress);
        std::strcpy(message->destPort, queues.toShaped->serverPort);
        shapedSender->send(controlStream,
                           reinterpret_cast<uint8_t *>(message),
                           sizeof(*message));
      }
    }
  }
}

inline void ShapedSender::initialiseSHM() {
  auto shmAddr = helpers::initialiseSHM(maxClients, appName, true);

  // The beginning of the SHM contains the signalStruct struct
  sigInfo = reinterpret_cast<class SignalInfo *>(shmAddr);
  if (sigInfo->unshaped == 0) {
    log(ERROR, "Unshaped Process has not yet started!");
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

    QUIC_UINT62 streamID;
    stream->GetID(&streamID);
    log(DEBUG, "Mapping stream " +
               std::to_string(streamID) +
               " to queues {" + std::to_string(queue1->ID) + "," +
               std::to_string(queue2->ID) + "}");
    // Data streams
    (*queuesToStream)[{queue1, queue2}] = stream;
    (*streamToQueues)[stream] = {queue1, queue2};
  }
}

void ShapedSender::sendData(size_t dataSize) {
  // TODO: Add prioritisation
  for (auto &iterator: *queuesToStream) {
    auto toShaped = iterator.first.toShaped;
    auto size = toShaped->size();
    if (size == 0 || dataSize == 0) {
      if (toShaped->markedForDeletion) {
        log(DEBUG, "(toShaped) " +
                   std::to_string(iterator.first.toShaped->ID) +
                   " is marked for deletion");

        // Send a termination control message
        auto *message =
            reinterpret_cast<struct ControlMessage *>(malloc(sizeof(struct
                ControlMessage)));
        iterator.second->GetID(&message->streamID);
        message->streamType = Data;
        message->connStatus = FIN;
        shapedSender->send(controlStream,
                           reinterpret_cast<uint8_t *>(message),
                           sizeof(*message));

        // Signal the unshaped process to clear the queue and related mappings
//        signalUnshapedProcess(toShaped->ID, FIN);
      }
      continue; //TODO: Send at least 1 byte
    }

    auto sizeToSend = std::min(dataSize, size);
    auto buffer = reinterpret_cast<uint8_t *>(malloc(sizeToSend));
    iterator.first.toShaped->pop(buffer, sizeToSend);
    shapedSender->send(iterator.second, buffer, sizeToSend);
    QUIC_UINT62 streamID;
    iterator.second->GetID(&streamID);
    log(DEBUG, "Sending actual data on stream " + std::to_string(streamID));
    dataSize -= sizeToSend;
  }
}

void ShapedSender::sendDummy(size_t dummySize) {
  uint8_t dummy[dummySize];
  memset(dummy, 0, dummySize);
  dummyStream->GetID(&dummyStreamID);
  shapedSender->send(dummyStream,
                     dummy, dummySize);
}

void
ShapedSender::onResponse(MsQuicStream *stream, uint8_t *buffer, size_t length) {
  uint64_t streamId;
  stream->GetID(&streamId);
  if (streamId != dummyStreamID && streamId != controlStreamID) {
    auto fromShaped = (*streamToQueues)[stream].fromShaped;
    if (fromShaped == nullptr) {
      log(ERROR, "Received data on unmapped stream " +
                 std::to_string(streamId));
      return;
    }
    fromShaped->push(buffer, length);
    log(DEBUG, "Received response on stream " + std::to_string(streamId));
  } else if (streamId == dummyStreamID) {
  } else if (streamId == controlStreamID) {
    auto ctrlMsg = reinterpret_cast<struct ControlMessage *>(buffer);
    if (ctrlMsg->connStatus == FIN) {
      log(DEBUG, "Received FIN for stream " +
                 std::to_string(ctrlMsg->streamID));
      auto dataStream = findStreamByID(ctrlMsg->streamID);
      auto &queues = (*streamToQueues)[dataStream];
      queues.fromShaped->markedForDeletion = true;
    }
  }
}

inline void ShapedSender::startControlStream() {
  controlStream = shapedSender->startStream();
  auto *message =
      reinterpret_cast<struct ControlMessage *>(calloc(1, sizeof(struct
          ControlMessage)));
  controlStream->GetID(&message->streamID);
  // save the control stream ID for later
  controlStreamID = message->streamID;
  message->streamType = Control;
  shapedSender->send(controlStream,
                     reinterpret_cast<uint8_t *>(message),
                     sizeof(*message));
  log(DEBUG, "Control stream is at " + std::to_string(message->streamID));
}

inline void ShapedSender::startDummyStream() {
  dummyStream = shapedSender->startStream();
  auto *message =
      reinterpret_cast<struct ControlMessage *>(calloc(1, sizeof(struct
          ControlMessage)));
  dummyStream->GetID(&message->streamID);
  // save the dummy stream ID for later
  dummyStreamID = message->streamID;
  message->streamType = Dummy;
  shapedSender->send(controlStream,
                     reinterpret_cast<uint8_t *>(message),
                     sizeof(*message));
  log(DEBUG, "Dummy stream is at " +
             std::to_string(message->streamID));

}

void ShapedSender::log(logLevels level, const std::string &log) {
  std::string levelStr;
  switch (level) {
    case DEBUG:
      levelStr = "SS:DEBUG: ";
      break;
    case ERROR:
      levelStr = "SS:ERROR: ";
      break;
    case WARNING:
      levelStr = "SS:WARNING: ";
      break;

  }
  if (logLevel >= level) {

    std::cerr << levelStr << log << std::endl;
  }
}