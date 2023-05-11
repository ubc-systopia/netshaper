//
// Created by Rut Vora
//

#include <iomanip>
#include "ShapedClient.h"

#ifdef RECORD_STATS
extern std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    quicIn;
extern std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    quicOut;
#endif

ShapedClient::ShapedClient(std::string &appName, int maxClients,
                           double noiseMultiplier, double sensitivity,
                           uint64_t maxDecisionSize, uint64_t minDecisionSize,
                           const std::string &peer2IP, uint16_t peer2Port,
                           __useconds_t DPCreditorLoopInterval,
                           __useconds_t sendingLoopInterval,
                           __useconds_t unshapedResponseLoopInterval,
                           logLevels logLevel, sendingStrategy strategy,
                           uint64_t idleTimeout) :
    appName(appName), logLevel(logLevel),
    unshapedResponseLoopInterval(unshapedResponseLoopInterval),
    maxClients(maxClients), sigInfo(nullptr), dummyStream(nullptr),
    controlStream(nullptr) {
  queuesToStream =
      new std::unordered_map<QueuePair,
          MsQuicStream *, QueuePairHash>(maxClients);
  streamToQueues =
      new std::unordered_map<MsQuicStream *, QueuePair>(maxClients);
  pendingSignal =
      new std::unordered_map<uint64_t, connectionStatus>(maxClients);

  noiseGenerator = new NoiseGenerator{noiseMultiplier, sensitivity,
                                      maxDecisionSize, minDecisionSize};
  // Connect to the other middlebox

  auto onResponseFunc = [this](auto &&PH1, auto &&PH2, auto &&PH3) {
    onResponse(std::forward<decltype(PH1)>(PH1),
               std::forward<decltype(PH2)>(PH2),
               std::forward<decltype(PH3)>(PH3));
  };

  // Two middle-boxes are connected in a client-server setup, where peer1 middlebox is
  // the client and peer2 middlebox is the server. In middlebox 1 we have
  // shapedClient and in middlebox 2 we have shapedServer
  shapedClient = new QUIC::Client{peer2IP, peer2Port, onResponseFunc,
                                  true,
                                  logLevel,
                                  idleTimeout};

  // We map a pair of queues over the shared memory region to every stream
  // CAUTION: we assume the shared queues are already initialized in unshaped process
  initialiseSHM();

  // Start the control stream
  startControlStream();

  // Start the dummy stream
  startDummyStream();
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  std::thread senderLoopThread(helpers::shaperLoop, queuesToStream,
                               noiseGenerator,
                               [this](auto &&PH1) {
                                 sendDummy(std::forward<decltype(PH1)>(PH1));
                               },
                               [this](auto &&PH1) {
                                 sendData(std::forward<decltype(PH1)>(PH1));
                               },
                               sendingLoopInterval, DPCreditorLoopInterval,
                               strategy, std::ref(mapLock));
  senderLoopThread.detach();
}

QueuePair ShapedClient::findQueuesByID(uint64_t queueID) {
  for (const auto &[queues, stream]: *queuesToStream) {
    if (queues.toShaped->ID == queueID) {
      return queues;
    }
  }
  return {nullptr, nullptr};
}

MsQuicStream *ShapedClient::findStreamByID(QUIC_UINT62 ID) {
  for (const auto &[stream, queues]: *streamToQueues) {
    if (stream == nullptr) continue;
    if (stream->ID() == ID) return stream;
  }
  return nullptr;
}

void ShapedClient::signalUnshapedProcess(uint64_t queueID,
                                         connectionStatus connStatus) {
  std::scoped_lock lock(writeLock);
  struct SignalInfo::queueInfo queueInfo{queueID, connStatus};
  sigInfo->enqueue(SignalInfo::fromShaped, queueInfo);

  // Signal the other process (does not actually kill the unshaped process)
  kill(sigInfo->unshaped, SIGUSR1);
}

void ShapedClient::handleQueueSignal(int signum) {
  if (signum == SIGUSR1) {
    std::scoped_lock lock(readLock);
    struct SignalInfo::queueInfo queueInfo{};
    while (sigInfo->dequeue(SignalInfo::toShaped, queueInfo)) {
      auto queues = findQueuesByID(queueInfo.queueID);
      if (queueInfo.connStatus == SYN) {
        auto *message =
            reinterpret_cast<struct ControlMessage *>(malloc(
                sizeof(struct ControlMessage)));
        message->streamID = (*queuesToStream)[queues]->ID();
#ifdef DEBUGGING
        log(DEBUG,
            "Sending SYN on stream " + std::to_string(message->streamID) +
            " mapped to queues {" + std::to_string(queues.fromShaped->ID) +
            "," + std::to_string(queues.toShaped->ID) + "}");
#endif
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
        shapedClient->send(controlStream,
                           reinterpret_cast<uint8_t *>(message),
                           sizeof(*message));
      } else if (queueInfo.connStatus == FIN) {
#ifdef DEBUGGING
        log(DEBUG, "Got a FIN signal " + std::to_string(queueInfo.queueID));
#endif
        (*pendingSignal)[queueInfo.queueID] = FIN;
      }
    }
  }
}

inline void ShapedClient::initialiseSHM() {
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
    auto stream = shapedClient->startStream();

#ifdef DEBUGGING
    log(DEBUG, "Mapping stream " + std::to_string(stream->ID()) +
               " to queues {" + std::to_string(queue1->ID) + "," +
               std::to_string(queue2->ID) + "}");
#endif
    // Data streams
    (*queuesToStream)[{queue1, queue2}] = stream;
    (*streamToQueues)[stream] = {queue1, queue2};
  }
}

void ShapedClient::sendData(size_t dataSize) {
  // TODO: Add prioritisation
  for (const auto &[queues, stream]: *queuesToStream) {
    auto toShaped = queues.toShaped;
    if (!toShaped->inUse) continue;
    auto queueSize = toShaped->size();
    if (queueSize == 0) {
      if ((*pendingSignal)[toShaped->ID] == FIN) {
        // Send a termination control message
        auto *message =
            reinterpret_cast<struct ControlMessage *>(malloc(sizeof(struct
                ControlMessage)));
        message->streamID = stream->ID();
#ifdef DEBUGGING
        log(DEBUG,
            "Sending FIN on stream " + std::to_string(message->streamID) +
            " mapped to queues " +
            std::to_string(queues.fromShaped->ID) + "," +
            std::to_string(queues.toShaped->ID) + "}"
        );
#endif
        message->streamType = Data;
        message->connStatus = FIN;
        shapedClient->send(controlStream,
                           reinterpret_cast<uint8_t *>(message),
                           sizeof(*message));
        (*pendingSignal).erase(toShaped->ID);
        queues.toShaped->inUse = false;
      }
      continue;
    }
    if (dataSize == 0) break;
    auto sizeToSend = std::min(dataSize, queueSize);
    auto buffer = reinterpret_cast<uint8_t *>(malloc(sizeToSend));
    queues.toShaped->pop(buffer, sizeToSend);
#ifdef RECORD_STATS
    quicOut[queues.fromShaped->ID / 2]
        .push_back(std::chrono::steady_clock::now());
#endif
    shapedClient->send(stream, buffer, sizeToSend);
    dataSize -= sizeToSend;
  }
}

void ShapedClient::sendDummy(size_t dummySize) {
  auto buffer = reinterpret_cast<uint8_t *>(malloc(dummySize));
  memset(buffer, 0, dummySize);
  shapedClient->send(dummyStream,
                     buffer, dummySize);
}

void ShapedClient::handleControlMessages(uint8_t *buffer, size_t length) {
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
#ifdef DEBUGGING
        log(DEBUG, "Received FIN from stream " +
                   std::to_string(ctrlMsg->streamID) + ", marking (fromShaped)"
                   + std::to_string(queues.fromShaped->ID) + " for deletion");
#endif
      }
    }
  }
}

void
ShapedClient::onResponse(MsQuicStream *stream, uint8_t *buffer, size_t length) {
  if (stream == controlStream) {
    handleControlMessages(buffer, length);
    return;
  }
  if (stream == dummyStream) {
    // Dummy received. Do nothing
    return;
  }

  // All other streams that are not dummy or control
  auto fromShaped = (*streamToQueues)[stream].fromShaped;
#ifdef RECORD_STATS
  quicIn[fromShaped->ID / 2].push_back(std::chrono::steady_clock::now());
#endif
  if (fromShaped == nullptr) {
    log(ERROR, "Received data on unmapped stream " +
               std::to_string(stream->ID()));
    return;
  }
  while (fromShaped->push(buffer, length) == -1) {
    log(WARNING, "(fromShaped) " + std::to_string(fromShaped->ID) +
                 +" mapped to stream " + std::to_string(stream->ID()) +
                 " is full, waiting for it to be empty!");
#ifdef SHAPING
    std::this_thread::sleep_for(
        std::chrono::microseconds(unshapedResponseLoopInterval));
#endif
  }

}

inline void ShapedClient::startControlStream() {
  controlStream = shapedClient->startStream();
  auto *message =
      reinterpret_cast<struct ControlMessage *>(calloc(1, sizeof(struct
          ControlMessage)));
  message->streamID = controlStream->ID();
  message->streamType = Control;
  shapedClient->send(controlStream,
                     reinterpret_cast<uint8_t *>(message),
                     sizeof(*message));
#ifdef DEBUGGING
  log(DEBUG, "Control stream is at " + std::to_string(message->streamID));
#endif
}

inline void ShapedClient::startDummyStream() {
  dummyStream = shapedClient->startStream();
  auto *message =
      reinterpret_cast<struct ControlMessage *>(calloc(1, sizeof(struct
          ControlMessage)));
  message->streamID = dummyStream->ID();
  message->streamType = Dummy;
  shapedClient->send(controlStream,
                     reinterpret_cast<uint8_t *>(message),
                     sizeof(*message));
#ifdef DEBUGGING
  log(DEBUG, "Dummy stream is at " +
             std::to_string(message->streamID));
#endif

}

void ShapedClient::log(logLevels level, const std::string &log) {
  if (logLevel < level) return;
  std::string levelStr;
  switch (level) {
    case DEBUG:
      levelStr = "ShapedClient:DEBUG: ";
      break;
    case ERROR:
      levelStr = "ShapedClient:ERROR: ";
      break;
    case WARNING:
      levelStr = "ShapedClient:WARNING: ";
      break;

  }
  auto time = std::time(nullptr);
  auto localTime = std::localtime(&time);
  std::scoped_lock lock(logWriter);
  std::cerr << std::put_time(localTime, "[%H:%M:%S] ")
            << levelStr << log << std::endl;

}