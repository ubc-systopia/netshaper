//
// Created by Rut Vora
//

#include <iomanip>
#include "ShapedSender.h"

ShapedSender::ShapedSender(std::string &appName, int maxClients,
                           double noiseMultiplier, double sensitivity,
                           const std::string &peer2IP, uint16_t peer2Port,
                           __useconds_t DPCreditorLoopInterval,
                           __useconds_t senderLoopInterval,
                           logLevels logLevel,
                           uint64_t idleTimeout) :
    appName(appName), logLevel(logLevel), maxClients(maxClients),
    sigInfo(nullptr), dummyStream(nullptr), controlStream(nullptr),
    dummyStreamID(QUIC_UINT62_MAX), controlStreamID(QUIC_UINT62_MAX) {
  queuesToStream =
      new std::unordered_map<QueuePair,
          MsQuicStream *, QueuePairHash>(maxClients);
  streamToQueues =
      new std::unordered_map<MsQuicStream *, QueuePair>(maxClients);
  pendingSignal =
      new std::unordered_map<uint64_t, connectionStatus>(maxClients);

  noiseGenerator = new NoiseGenerator{noiseMultiplier, sensitivity};
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
                            senderLoopInterval, DPCreditorLoopInterval);

  sendingThread.detach();
}

QueuePair ShapedSender::findQueuesByID(uint64_t queueID) {
  for (const auto &[queues, stream]: *queuesToStream) {
    if (queues.toShaped->ID == queueID) {
      return queues;
    }
  }
  return {nullptr, nullptr};
}

MsQuicStream *ShapedSender::findStreamByID(QUIC_UINT62 ID) {
  QUIC_UINT62 streamID;
  for (const auto &[stream, queues]: *streamToQueues) {
    if (stream == nullptr) continue;
    stream->GetID(&streamID);
    if (streamID == ID) return stream;
  }
  return nullptr;
}

void ShapedSender::signalUnshapedProcess(uint64_t queueID,
                                         connectionStatus connStatus) {
  std::scoped_lock lock(writeLock);
  struct SignalInfo::queueInfo queueInfo{queueID, connStatus};
  sigInfo->enqueue(SignalInfo::fromShaped, queueInfo);

  // Signal the other process (does not actually kill the unshaped process)
  kill(sigInfo->unshaped, SIGUSR1);
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
        log(DEBUG,
            "Sending SYN on stream " + std::to_string(message->streamID) +
            " mapped to queues {" + std::to_string(queues.fromShaped->ID) +
            "," + std::to_string(queues.toShaped->ID) + "}");
        message->streamType = Data;
        message->connStatus = SYN;
        std::strcpy(message->addrPair.clientAddress,
                    queues.toShaped->addrPair.clientAddress);
        std::strcpy(message->addrPair.clientPort,
                    queues.toShaped->addrPair.clientPort);
        std::strcpy(message->addrPair.serverAddress,
                    queues.toShaped->addrPair.serverAddress);
        std::strcpy(message->addrPair.serverPort,
                    queues.toShaped->addrPair.serverPort);
        shapedSender->send(controlStream,
                           reinterpret_cast<uint8_t *>(message),
                           sizeof(*message));
      } else if (queueInfo.connStatus == FIN) {
        (*pendingSignal)[queueInfo.queueID] = FIN;
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
    log(DEBUG, "Mapping stream " + std::to_string(streamID) +
               " to queues {" + std::to_string(queue1->ID) + "," +
               std::to_string(queue2->ID) + "}");
    // Data streams
    (*queuesToStream)[{queue1, queue2}] = stream;
    (*streamToQueues)[stream] = {queue1, queue2};
  }
}

void ShapedSender::sendData(size_t dataSize) {
  // TODO: Add prioritisation
  for (const auto &[queues, stream]: *queuesToStream) {
    if (dataSize == 0) break;
    auto toShaped = queues.toShaped;

    auto queueSize = toShaped->size();
    if (queueSize == 0) {
      if ((*pendingSignal)[toShaped->ID] == FIN) {
        // Send a termination control message
        auto *message =
            reinterpret_cast<struct ControlMessage *>(malloc(sizeof(struct
                ControlMessage)));
        stream->GetID(&message->streamID);
        log(DEBUG,
            "Sending FIN on stream " + std::to_string(message->streamID) +
            " mapped to queues " +
            std::to_string(queues.fromShaped->ID) + "," +
            std::to_string(queues.toShaped->ID) + "}"
        );
        message->streamType = Data;
        message->connStatus = FIN;
        shapedSender->send(controlStream,
                           reinterpret_cast<uint8_t *>(message),
                           sizeof(*message));
        (*pendingSignal).erase(toShaped->ID);
      }
      continue;
    }
    auto sizeToSend = std::min(dataSize, queueSize);
    auto buffer = reinterpret_cast<uint8_t *>(malloc(sizeToSend));
    queues.toShaped->pop(buffer, sizeToSend);
    shapedSender->send(stream, buffer, sizeToSend);
    QUIC_UINT62 streamID;
    stream->GetID(&streamID);
    dataSize -= sizeToSend;
  }
}

void ShapedSender::sendDummy(size_t dummySize) {
  uint8_t dummy[dummySize];
  memset(dummy, 0, dummySize);
  shapedSender->send(dummyStream,
                     dummy, dummySize);
}

void ShapedSender::handleControlMessages(uint8_t *buffer, size_t length) {
  if (length % sizeof(ControlMessage) != 0) {
    log(ERROR, "Received half a control message!");
    return;
  } else {
    uint8_t messages = length / sizeof(ControlMessage);
    for (uint8_t i = 0; i < messages; i++) {
      auto ctrlMsg =
          reinterpret_cast<struct ControlMessage *>
          (buffer + (i * sizeof(ControlMessage)));
      if (ctrlMsg->connStatus == FIN) {
        auto dataStream = findStreamByID(ctrlMsg->streamID);
        auto &queues = (*streamToQueues)[dataStream];
        queues.fromShaped->markedForDeletion = true;
        signalUnshapedProcess(queues.fromShaped->ID, FIN);
        log(DEBUG, "Received FIN from stream " +
                   std::to_string(ctrlMsg->streamID) + ", marking (fromShaped)"
                   + std::to_string(queues.fromShaped->ID) + " for deletion");
      }
    }
  }
}

void
ShapedSender::onResponse(MsQuicStream *stream, uint8_t *buffer, size_t length) {
  uint64_t streamID;
  stream->GetID(&streamID);
  if (streamID == controlStreamID) {
    handleControlMessages(buffer, length);
    return;
  }
  if (streamID == dummyStreamID) {
    // Dummy received. Do nothing
    return;
  }

  // All other streams that are not dummy or control
  auto fromShaped = (*streamToQueues)[stream].fromShaped;
  if (fromShaped == nullptr) {
    log(ERROR, "Received data on unmapped stream " +
               std::to_string(streamID));
    return;
  }
  fromShaped->push(buffer, length);

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
  if (logLevel < level) return;
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
  auto time = std::time(nullptr);
  auto localTime = std::localtime(&time);
  std::scoped_lock lock(logWriter);
  std::cerr << std::put_time(localTime, "[%H:%M:%S] ")
            << levelStr << log << std::endl;

}