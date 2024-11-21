//
// Created by Rut Vora
//

#include "ShapedServer.h"
#include <utility>
#include <iomanip>

ShapedServer::ShapedServer(config::Peer2Config &peer2Config) :
    dummyStreamID(QUIC_UINT62_MAX) {
  this->appName = peer2Config.appName;
  this->logLevel = peer2Config.logLevel;
  unshapedProcessLoopInterval = peer2Config.unshapedClient.checkQueuesInterval;
  controlStream = dummyStream = nullptr;
  size_t controlMessageQueueSize =
      4 * peer2Config.maxPeers * peer2Config.maxStreamsPerPeer *
      sizeof(ControlMessage);
  controlMessageQueue =
      reinterpret_cast<LamportQueue *>(malloc(sizeof(LamportQueue)
                                              + controlMessageQueueSize));
  new(controlMessageQueue) LamportQueue{INT_MAX, controlMessageQueueSize};
  queuesToStream =
      new std::unordered_map<QueuePair,
          MsQuicStream *, QueuePairHash>(peer2Config.maxStreamsPerPeer);
  streamToQueues =
      new std::unordered_map<MsQuicStream *, QueuePair>(
          peer2Config.maxStreamsPerPeer);
  streamToID =
      new std::unordered_map<MsQuicStream *, QUIC_UINT62>(
          peer2Config.maxStreamsPerPeer);
  pendingSignal =
      new std::unordered_map<uint64_t, connectionStatus>(
          peer2Config.maxStreamsPerPeer);
  unassignedQueues = new std::queue<QueuePair>{};

  initialiseSHM(peer2Config.maxPeers * peer2Config.maxStreamsPerPeer,
                peer2Config.queueSize);

  auto receivedShapedDataFunc = [this](auto &&PH1, auto &&PH2, auto &&PH3) {
    receivedShapedData(std::forward<decltype(PH1)>(PH1),
                       std::forward<decltype(PH2)>(PH2),
                       std::forward<decltype(PH3)>(PH3));
  };
  auto config = peer2Config.shapedServer;
  // Start listening for connections from the other middlebox
  // Add additional stream for dummy data
  shapedServer =
      new QUIC::Server{config.serverCert, config.serverKey,
                       config.listeningPort, receivedShapedDataFunc, logLevel,
                       peer2Config.maxStreamsPerPeer + 2, config.idleTimeout};
  shapedServer->startListening();

  noiseGenerator = new NoiseGenerator{config.noiseMultiplier,
                                      config.sensitivity,
                                      config.maxDecisionSize,
                                      config.minDecisionSize};

  std::thread senderLoopThread(helpers::shaperLoop, queuesToStream,
                               noiseGenerator,
                               [this](auto &&PH1) -> PreparedBuffer {
                                 return prepareDummy(std::forward<decltype(PH1)>
                                                         (PH1));
                               },
                               [this](auto &&PH1) ->
                                   std::vector<PreparedBuffer> {
                                 return prepareData(std::forward<decltype(PH1)>
                                                        (PH1));
                               },
                               [this](MsQuicStream *stream, uint8_t *buffer,
                                      size_t length) {
                                 shapedServer->send(stream, buffer, length);
                               },
                               config.sendingLoopInterval,
                               config.DPCreditorLoopInterval,
                               config.strategy, std::ref(mapLock),
                               config.shaperCores);
  senderLoopThread.detach();

  std::thread updateQueueStatus([this]() { getUpdatedConnectionStatus(); });
  updateQueueStatus.detach();
}

void ShapedServer::printStats() {
    if (shapedServer != nullptr) {
        shapedServer->printStats();
    }
}

inline void ShapedServer::initialiseSHM(int numStreams, size_t queueSize) {
  auto shmAddr = helpers::initialiseSHM(numStreams, appName, queueSize, true);

  // The beginning of the SHM contains the signalStruct struct
  sigInfo = reinterpret_cast<class SignalInfo *>(shmAddr);

  // The rest of the SHM contains the queues
  shmAddr += (sizeof(SignalInfo) + (2 * sizeof(LamportQueue)) +
              (4 * numStreams * sizeof(SignalInfo::queueInfo)));
  for (int i = 0; i < numStreams * 2 + 2; i += 2) {
    auto queue1 =
        (LamportQueue *) (shmAddr +
                          (i * (sizeof(class LamportQueue) + queueSize)));
    auto queue2 =
        (LamportQueue *) (shmAddr +
                          ((i + 1) * (sizeof(class LamportQueue) + queueSize)));

    if (i > 0) unassignedQueues->push({queue1, queue2});
    else dummyQueues = {queue1, queue2};
  }
}

MsQuicStream *ShapedServer::findStreamByID(QUIC_UINT62 ID) {
  for (const auto &[stream, streamID]: *streamToID) {
    if (streamID == ID) return stream;
  }
  return nullptr;
}

[[noreturn]] void ShapedServer::getUpdatedConnectionStatus() {
  struct SignalInfo::queueInfo queueInfo{};
  auto sleepUntil = std::chrono::steady_clock::now();
  while (true) {
    sleepUntil = std::chrono::steady_clock::now() +
                 std::chrono::milliseconds(1);
    while (sigInfo->dequeue(SignalInfo::toShaped, queueInfo)) {
      if (queueInfo.connStatus == FIN) {
        (*pendingSignal)[queueInfo.queueID] = FIN;
      }
    }
    std::this_thread::sleep_until(sleepUntil);
  }
}

void ShapedServer::updateConnectionStatus(uint64_t queueID,
                                          connectionStatus connStatus) {
  std::scoped_lock lock(writeLock);
  struct SignalInfo::queueInfo queueInfo{queueID, connStatus};
  sigInfo->enqueue(SignalInfo::fromShaped, queueInfo);

  // Signal the other process (does not actually kill the unshaped process)
//  kill(sigInfo->unshaped, SIGUSR1);
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
  auto queues = unassignedQueues->front();
  unassignedQueues->pop();
  (*streamToQueues)[stream] = queues;
  (*queuesToStream)[queues] = stream;
  (*streamToID)[stream] = stream->ID();

  QUIC_UINT62 streamID = (*streamToID)[stream];
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
    updateConnectionStatus((*streamToQueues)[stream].fromShaped->ID,
                           SYN);

    streamIDtoCtrlMsg.erase(streamID);

  }
  return true;
}

inline void ShapedServer::eraseMapping(MsQuicStream *stream) {
  auto queues = (*streamToQueues)[stream];
  if (queues.toShaped->size() != 0) {
    log(ERROR, "Requested map clearing before all data was sent!");
    return;
  }
#ifdef DEBUGGING
  log(DEBUG, "Clearing the mapping for the stream " +
             std::to_string((*streamToID)[stream]) + " mapped to queues {" +
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
  auto ctrlMsgSize = sizeof(ControlMessage);
  controlMessageQueue->push(buffer, length);
  auto ctrlMsg = reinterpret_cast<ControlMessage *>(malloc(ctrlMsgSize));
  while (controlMessageQueue->pop((uint8_t *) ctrlMsg, ctrlMsgSize) != -1) {
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
            if (dataStream != nullptr
                &&
                (*streamToQueues).find(dataStream) != (*streamToQueues).end()) {
              queues = (*streamToQueues)[dataStream];
              copyClientInfo(queues, ctrlMsg);
              updateConnectionStatus(queues.fromShaped->ID, SYN);
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
              updateConnectionStatus(queues.fromShaped->ID, FIN);
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
  if (dummyStream == nullptr || stream == dummyStream) {
    if (stream->ID() == dummyStreamID) {
      if (dummyStream == nullptr) dummyStream = stream;
      dummyQueues.fromShaped->push(buffer, length);
    }
    return;
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
  while (fromShaped->push(buffer, length) == -1) {
    log(WARNING, "(fromShaped) " + std::to_string(fromShaped->ID) +
                 " is full, waiting for it to be empty");
#ifdef SHAPING
    // Sleep for some time. For performance reasons, this is the same as
    // the interval with the unshaped components checks queues for data.
    std::this_thread::sleep_for(
        std::chrono::microseconds(unshapedProcessLoopInterval));
#endif
  }
}

PreparedBuffer ShapedServer::prepareDummy(size_t dummySize) {
  // We do not have dummy stream yet
  if (dummyStream == nullptr) return {nullptr, nullptr, 0};
  auto buffer = reinterpret_cast<uint8_t *>(malloc(dummySize));
  memset(buffer, 0, dummySize);
  return {dummyStream, buffer, dummySize};
}

std::vector<PreparedBuffer> ShapedServer::prepareData(size_t dataSize) {
  std::vector<PreparedBuffer> preparedBuffers{};
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
        message->streamID = (*streamToID)[stream];
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
    auto sizeToSendFromQueue = std::min(queueSize, dataSize);
    auto buffer =
        reinterpret_cast<uint8_t *>(malloc(sizeToSendFromQueue + 1));
    if (buffer == nullptr) continue;
    queues.toShaped->pop(buffer, sizeToSendFromQueue);
    preparedBuffers.push_back({stream, buffer, sizeToSendFromQueue});
  }
  return preparedBuffers;
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
