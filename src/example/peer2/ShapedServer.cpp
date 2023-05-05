//
// Created by Rut Vora
//

#include "ShapedServer.h"
#include <utility>
#include <iomanip>

#ifdef RECORD_STATS
extern std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    quicIn;
extern std::vector<std::vector<std::chrono::time_point<std::chrono::steady_clock>>>
    quicOut;
#endif

ShapedServer::ShapedServer(std::string appName,
                           const std::string &serverCert,
                           const std::string &serverKey,
                           int maxPeers, int maxStreamsPerPeer,
                           uint16_t bindPort,
                           double noiseMultiplier, double sensitivity,
                           uint64_t maxDecisionSize,
                           uint64_t minDecisionSize,
                           __useconds_t DPCreditorLoopInterval,
                           __useconds_t sendingLoopInterval,
                           __useconds_t unshapedClientLoopInterval,
                           logLevels logLevel, sendingStrategy strategy,
                           uint64_t idleTimeout) :
    appName(std::move(appName)), logLevel(logLevel),
    unshapedClientLoopInterval(unshapedClientLoopInterval), sigInfo(nullptr),
    controlStream(nullptr), dummyStream(nullptr),
    dummyStreamID(QUIC_UINT62_MAX) {
  queuesToStream =
      new std::unordered_map<QueuePair,
          MsQuicStream *, QueuePairHash>(maxStreamsPerPeer);
  streamToQueues =
      new std::unordered_map<MsQuicStream *, QueuePair>(maxStreamsPerPeer);
  pendingSignal =
      new std::unordered_map<uint64_t, connectionStatus>(maxStreamsPerPeer);
  unassignedQueues = new std::queue<QueuePair>{};

  initialiseSHM(maxPeers * maxStreamsPerPeer);

  auto receivedShapedDataFunc = [this](auto &&PH1, auto &&PH2, auto &&PH3) {
    receivedShapedData(std::forward<decltype(PH1)>(PH1),
                       std::forward<decltype(PH2)>(PH2),
                       std::forward<decltype(PH3)>(PH3));
  };

  // Start listening for connections from the other middlebox
  // Add additional stream for dummy data
  shapedServer =
      new QUIC::Server{serverCert, serverKey, bindPort,
                       receivedShapedDataFunc, logLevel,
                       maxStreamsPerPeer + 2, idleTimeout};
  shapedServer->startListening();

  noiseGenerator = new NoiseGenerator{noiseMultiplier, sensitivity,
                                      maxDecisionSize, minDecisionSize};

  std::thread dpCreditorThread(helpers::DPCreditor, &sendingCredit,
                               queuesToStream, noiseGenerator,
                               DPCreditorLoopInterval, std::ref(mapLock));
  dpCreditorThread.detach();

  std::thread sendShaped(helpers::sendShapedData, &sendingCredit,
                         queuesToStream,
                         [this](auto &&PH1) {
                           sendDummy
                               (std::forward<decltype(PH1)>(PH1));
                         },
                         [this](auto &&PH1) {
                           sendData
                               (std::forward<decltype(PH1)>(PH1));
                         },
                         sendingLoopInterval, DPCreditorLoopInterval, strategy,
                         std::ref(mapLock));
  sendShaped.detach();
}

inline void ShapedServer::initialiseSHM(int numStreams) {
  auto shmAddr = helpers::initialiseSHM(numStreams, appName, true);

  // The beginning of the SHM contains the signalStruct struct
  sigInfo = reinterpret_cast<class SignalInfo *>(shmAddr);
  if (sigInfo->unshaped == 0) {
    log(ERROR, "Unshaped process has not started yet!");
    exit(1);
  }
  sigInfo->shaped = getpid();

  // The rest of the SHM contains the queues
  shmAddr += sizeof(class SignalInfo);
  for (int i = 0; i < numStreams * 2; i += 2) {
    auto queue1 =
        (LamportQueue *) (shmAddr + (i * sizeof(class LamportQueue)));
    auto queue2 =
        (LamportQueue *) (shmAddr + ((i + 1) * sizeof(class LamportQueue)));

    // Data streams
//    (*queuesToStream)[{queue1, queue2}] = nullptr;
    unassignedQueues->push({queue1, queue2});
  }
}

MsQuicStream *ShapedServer::findStreamByID(QUIC_UINT62 ID) {
  QUIC_UINT62 streamID;
  for (const auto &[stream, queues]: *streamToQueues) {
    if (stream != nullptr) {
      stream->GetID(&streamID);
      if (streamID == ID) return stream;
    }
  }
  return nullptr;
}

void ShapedServer::handleQueueSignal(int signum) {
  if (signum == SIGUSR1) {
    std::scoped_lock lock(readLock);
    struct SignalInfo::queueInfo queueInfo{};
    while (sigInfo->dequeue(SignalInfo::toShaped, queueInfo)) {
      if (queueInfo.connStatus == FIN) {
        (*pendingSignal)[queueInfo.queueID] = FIN;
      }
    }
  }
}

void ShapedServer::signalUnshapedProcess(uint64_t queueID,
                                         connectionStatus connStatus) {
  std::scoped_lock lock(writeLock);
  struct SignalInfo::queueInfo queueInfo{queueID, connStatus};
  sigInfo->enqueue(SignalInfo::fromShaped, queueInfo);

  // Signal the other process (does not actually kill the unshaped process)
  kill(sigInfo->unshaped, SIGUSR1);
}

void ShapedServer::copyClientInfo(QueuePair queues,
                                  struct ControlMessage *ctrlMsg) {
  // Source/Client
  std::strcpy(queues.toShaped->addrPair.clientAddress,
              ctrlMsg->addrPair.clientAddress);
  std::strcpy(queues.toShaped->addrPair.clientPort,
              ctrlMsg->addrPair.clientPort);
  std::strcpy(queues.fromShaped->addrPair.clientAddress,
              ctrlMsg->addrPair.clientAddress);
  std::strcpy(queues.fromShaped->addrPair.clientPort,
              ctrlMsg->addrPair.clientPort);

  //Destination/Server
  std::strcpy(queues.toShaped->addrPair.serverAddress,
              ctrlMsg->addrPair.serverAddress);
  std::strcpy(queues.toShaped->addrPair.serverPort,
              ctrlMsg->addrPair.serverPort);
  std::strcpy(queues.fromShaped->addrPair.serverAddress,
              ctrlMsg->addrPair.serverAddress);
  std::strcpy(queues.fromShaped->addrPair.serverPort,
              ctrlMsg->addrPair.serverPort);
}

inline bool ShapedServer::assignQueues(MsQuicStream *stream) {
  if (unassignedQueues->empty()) return false;
//  mapLock.lock();
  auto queues = unassignedQueues->front();
  unassignedQueues->pop();
  (*streamToQueues)[stream] = queues;
  (*queuesToStream)[queues] = stream;

  QUIC_UINT62 streamID;
  stream->GetID(&streamID);
#ifdef DEBUGGING
  log(DEBUG,
      "Assigning stream " + std::to_string(streamID) + " to queues {" +
      std::to_string(queues.fromShaped->ID) + "," +
      std::to_string(queues.toShaped->ID) + "}");
#endif

  queues.fromShaped->markedForDeletion = false;
  queues.toShaped->markedForDeletion = false;
  queues.toShaped->sentFIN = queues.fromShaped->sentFIN = false;
  queues.toShaped->clear();
  queues.fromShaped->clear();

  if (streamIDtoCtrlMsg.find(streamID) != streamIDtoCtrlMsg.end()) {
    copyClientInfo(queues, &streamIDtoCtrlMsg[streamID]);
    signalUnshapedProcess((*streamToQueues)[stream].fromShaped->ID,
                          SYN);

    streamIDtoCtrlMsg.erase(streamID);

  }
//  mapLock.unlock();
  return true;
}

inline void ShapedServer::eraseMapping(MsQuicStream *stream) {
  auto queues = (*streamToQueues)[stream];
  if (queues.toShaped->size() != 0) {
    log(ERROR, "Requested map clearing before all data was sent!");
    return;
  }
#ifdef DEBUGGING
  uint64_t streamID;
  stream->GetID(&streamID);
  log(DEBUG, "Clearing the mapping for the stream " +
             std::to_string(streamID) + " mapped to queues {" +
             std::to_string(queues.fromShaped->ID) + "," +
             std::to_string(queues.toShaped->ID) + "}");
#endif
  mapLock.lock();
  (*streamToQueues).erase(stream);
  (*queuesToStream).erase(queues);
  unassignedQueues->push(queues);
  mapLock.unlock();
}

void ShapedServer::handleControlMessages(MsQuicStream *ctrlStream,
                                         uint8_t *buffer, size_t length) {
  if (length % sizeof(ControlMessage) != 0) {
    log(ERROR, "Received half a control message!");
    return;
  }
  auto ctrlMsgSize = sizeof(ControlMessage);
  uint8_t msgCount = length / ctrlMsgSize;

  for (uint8_t i = 0; i < msgCount; i++) {
    auto ctrlMsg =
        reinterpret_cast<ControlMessage *>(buffer + (i * ctrlMsgSize));
    switch (ctrlMsg->streamType) {
      case Dummy:
#ifdef DEBUGGING
        log(DEBUG, "Dummy stream is at " + std::to_string(ctrlMsg->streamID));
#endif
        dummyStreamID = ctrlMsg->streamID;
        break;
      case Data: {
        mapLock.lock();
        auto dataStream = findStreamByID(ctrlMsg->streamID);
        QueuePair queues = {nullptr, nullptr};
        switch (ctrlMsg->connStatus) {
          case SYN:
#ifdef DEBUGGING
            log(DEBUG, "Received SYN on stream " +
                       std::to_string(ctrlMsg->streamID));
#endif
            if (dataStream != nullptr) {
              queues = (*streamToQueues)[dataStream];
              copyClientInfo(queues, ctrlMsg);
              signalUnshapedProcess(queues.fromShaped->ID, SYN);
            } else {
              // Map from stream (which has not yet started) to client
              streamIDtoCtrlMsg[ctrlMsg->streamID] = *ctrlMsg;
            }
            break;
          case FIN:
            queues = (*streamToQueues)[dataStream];
            if (queues.fromShaped != nullptr) {
#ifdef DEBUGGING
              log(DEBUG, "Received FIN from stream " +
                         std::to_string(ctrlMsg->streamID) +
                         " mapped to queues {" +
                         std::to_string(queues.fromShaped->ID) + "," +
                         std::to_string(queues.toShaped->ID) + "}");
#endif
              queues.fromShaped->markedForDeletion = true;
              signalUnshapedProcess(queues.fromShaped->ID, FIN);
            }
            break;
          default:
            break;
        }
        mapLock.unlock();
      }
        break;
      case Control:
        if (controlStream == nullptr) controlStream = ctrlStream;
        // Else, this (re-identification of control stream) should never happen
        break;
    }
  }
}

void ShapedServer::receivedShapedData(MsQuicStream *stream,
                                      uint8_t *buffer, size_t length) {
  // Check if this is first byte from the other middlebox
  if (stream == controlStream || controlStream == nullptr) {
    handleControlMessages(stream, buffer, length);
    return;
  }

  // Not a control stream... Check for other types
  if (dummyStream == nullptr) {
    uint64_t streamID;
    stream->GetID(&streamID);
    if (streamID == dummyStreamID) {
      // Dummy Data
      if (dummyStream == nullptr) dummyStream = stream;
      return;
    }
  }

  // This is a data stream
  mapLock.lock_shared();
  if ((*streamToQueues)[stream].fromShaped == nullptr) {
    if (!assignQueues(stream)) {
      log(ERROR, "More streams from peer than allowed!");
      return;
    }
  }

  auto fromShaped = (*streamToQueues)[stream].fromShaped;
  mapLock.unlock_shared();
#ifdef RECORD_STATS
  quicIn[fromShaped->ID / 2].push_back(std::chrono::steady_clock::now());
#endif
  while (fromShaped->push(buffer, length) == -1) {
    log(WARNING, "(fromShaped) " + std::to_string(fromShaped->ID) +
                 " is full, waiting for it to be empty");
#ifdef SHAPING
    // Sleep for some time. For performance reasons, this is the same as
    // the interval with the unshaped components checks queues for data.
    std::this_thread::sleep_for(
        std::chrono::microseconds(unshapedClientLoopInterval));
#endif
  }
}

void ShapedServer::sendDummy(size_t dummySize) {
  // We do not have dummy stream yet
  if (dummyStream == nullptr) return;
  auto buffer = reinterpret_cast<uint8_t *>(malloc(dummySize));
  memset(buffer, 0, dummySize);
  if (!shapedServer->send(dummyStream, buffer, dummySize)) {
  }
}

size_t ShapedServer::sendData(size_t dataSize) {
  auto origSize = dataSize;

  mapLock.lock_shared();
  auto tempMap = *queuesToStream;
  mapLock.unlock_shared();
  for (const auto &[queues, stream]: tempMap) {
//    if (stream == nullptr) continue;
    auto queueSize = queues.toShaped->size();
    // No data in this queue, check for FINs and erase mappings
    if (queueSize == 0) {
      if ((*pendingSignal)[queues.toShaped->ID] == FIN) {
        // Send a termination control message
        auto *message =
            reinterpret_cast<struct ControlMessage *>(malloc(sizeof(struct
                ControlMessage)));
        stream->GetID(&message->streamID);
#ifdef DEBUGGING
        log(DEBUG,
            "Sending FIN on stream " + std::to_string(message->streamID)
            + " mapped to queues {" +
            std::to_string(queues.fromShaped->ID) + "," +
            std::to_string(queues.toShaped->ID) + "}");
#endif
        message->streamType = Data;
        message->connStatus = FIN;
        shapedServer->send(controlStream,
                           reinterpret_cast<uint8_t *>(message),
                           sizeof(*message));
        (*pendingSignal).erase(queues.toShaped->ID);
        queues.toShaped->sentFIN = true;
      }
      if (queues.toShaped->markedForDeletion
          && queues.fromShaped->markedForDeletion
          && queues.toShaped->sentFIN) {
        eraseMapping(stream);
      }
      continue;
    }

    // We have sent enough
    if (dataSize == 0) break;
    auto SizeToSendFromQueue = std::min(queueSize, dataSize);
    auto buffer =
        reinterpret_cast<uint8_t *>(malloc(SizeToSendFromQueue + 1));

    queues.toShaped->pop(buffer, SizeToSendFromQueue);
#ifdef RECORD_STATS
    quicOut[queues.fromShaped->ID / 2].push_back(
        std::chrono::steady_clock::now());
#endif
    if (!shapedServer->send(stream, buffer, SizeToSendFromQueue)) {
      uint64_t streamID;
      stream->GetID(&streamID);
      log(ERROR, "Failed to send Shaped response on stream " +
                 std::to_string(streamID));
    } else {
      dataSize -= SizeToSendFromQueue;
    }
  }
  // We expect the data size to be zero if we have sent all the data
  return origSize - dataSize;
}

void ShapedServer::log(logLevels level, const std::string &log) {
  if (logLevel < level) return;
  std::string levelStr;
  switch (level) {
    case DEBUG:
      levelStr = "ShapedServer:DEBUG: ";
      break;
    case ERROR:
      levelStr = "ShapedServer:ERROR: ";
      break;
    case WARNING:
      levelStr = "ShapedServer:WARNING: ";
      break;

  }
  auto time = std::time(nullptr);
  auto localTime = std::localtime(&time);
  std::scoped_lock lock(logWriter);
  std::cerr << std::put_time(localTime, "[%H:%M:%S] ")
            << levelStr << log << std::endl;

}