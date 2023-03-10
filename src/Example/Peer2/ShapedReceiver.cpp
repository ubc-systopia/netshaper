//
// Created by Rut Vora
//

#include "ShapedReceiver.h"
#include <utility>
#include <iomanip>

ShapedReceiver::ShapedReceiver(std::string appName,
                               const std::string &serverCert,
                               const std::string &serverKey, int maxPeers,
                               int maxStreamsPerPeer, uint16_t bindPort,
                               uint64_t idleTimeout, double epsilon,
                               double sensitivity,
                               __useconds_t DPCreditorLoopInterval,
                               __useconds_t senderLoopInterval) :
    appName(std::move(appName)), logLevel(DEBUG), sigInfo(nullptr),
    controlStream(nullptr), dummyStream(nullptr),
    dummyStreamID(QUIC_UINT62_MAX) {
  queuesToStream =
      new std::unordered_map<QueuePair,
          MsQuicStream *, QueuePairHash>(maxStreamsPerPeer);

  streamToQueues =
      new std::unordered_map<MsQuicStream *, QueuePair>(maxStreamsPerPeer);

  pendingSignal =
      new std::unordered_map<uint64_t, connectionStatus>(maxStreamsPerPeer);

  initialiseSHM(maxPeers * maxStreamsPerPeer);

  auto receivedShapedDataFunc = [this](auto &&PH1, auto &&PH2, auto &&PH3) {
    receivedShapedData(std::forward<decltype(PH1)>(PH1),
                       std::forward<decltype(PH2)>(PH2),
                       std::forward<decltype(PH3)>(PH3));
  };

  // Start listening for connections from the other middlebox
  // Add additional stream for dummy data
  shapedReceiver =
      new QUIC::Receiver{serverCert, serverKey,
                         bindPort,
                         receivedShapedDataFunc,
                         WARNING,
                         maxStreamsPerPeer + 1,
                         idleTimeout};
  shapedReceiver->startListening();

  noiseGenerator = new NoiseGenerator{epsilon, sensitivity};

  std::thread dpCreditorThread(helpers::DPCreditor, &sendingCredit,
                               queuesToStream, noiseGenerator,
                               DPCreditorLoopInterval);
  dpCreditorThread.detach();

//  auto sendShapedData = std::bind(&ShapedReceiver::sendShapedData, this,
//                                  std::placeholders::_1);

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
                         senderLoopInterval);
  sendShaped.detach();
}

inline void ShapedReceiver::initialiseSHM(int numStreams) {
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
    (*queuesToStream)[{queue1, queue2}] = nullptr;
  }
}

bool ShapedReceiver::sendResponse(MsQuicStream *stream, uint8_t *data,
                                  size_t length) {
  // Note: Valgrind reports this as a "definitely lost" block. But QUIC
  //  frees it from another thread after sending is complete
  auto SendBuffer =
      reinterpret_cast<QUIC_BUFFER *>(malloc(sizeof(QUIC_BUFFER)));
  if (SendBuffer == nullptr) {
    return false;
  }

  SendBuffer->Buffer = data;
  SendBuffer->Length = length;

  if (QUIC_FAILED(stream->Send(SendBuffer, 1, QUIC_SEND_FLAG_NONE))) {
    free(SendBuffer);
    return false;
  }
  return true;
}

MsQuicStream *ShapedReceiver::findStreamByID(QUIC_UINT62 ID) {
  QUIC_UINT62 streamID;
  for (auto &iterator: *streamToQueues) {
    if (iterator.first == nullptr) continue;
    iterator.first->GetID(&streamID);
    if (streamID == ID) return iterator.first;
  }
  return nullptr;
}

//[[maybe_unused]] [[noreturn]] void
//ShapedReceiver::sendResponseLoop(__useconds_t interval) {
//  while (true) {
//    std::this_thread::sleep_for(std::chrono::microseconds(interval));
//    for (auto &iterator: *queuesToStream) {
//      auto size = iterator.first.toShaped->size();
//      if (size > 0) {
//        log(DEBUG, "Received response in queue (toShaped) " +
//                   std::to_string(iterator.first.toShaped->ID));
//
//        auto buffer = reinterpret_cast<uint8_t *>(malloc(size));
//        iterator.first.toShaped->pop(buffer, size);
//        MsQuicStream *stream = iterator.second;
//        if (stream == nullptr) {
//          for (auto &itr: *streamToQueues) {
//            if (itr.first != nullptr) {
//              stream = itr.first;
//              break;
//            }
//          }
//        }
//        QUIC_UINT62 streamId;
//        stream->GetID(&streamId);
////        log(DEBUG, "Sending response on stream " + std::to_string(streamId));
//        sendResponse(stream, buffer, size);
//      }
//    }
//  }
//}

void ShapedReceiver::handleQueueSignal(int signum) {
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

void ShapedReceiver::signalUnshapedProcess(uint64_t queueID,
                                           connectionStatus connStatus) {
  std::scoped_lock lock(writeLock);
  struct SignalInfo::queueInfo queueInfo{queueID, connStatus};
  sigInfo->enqueue(SignalInfo::fromShaped, queueInfo);

  // Signal the other process (does not actually kill the unshaped process)
  kill(sigInfo->unshaped, SIGUSR1);
}

void ShapedReceiver::copyClientInfo(QueuePair queues,
                                    struct ControlMessage *ctrlMsg) {
  // Source/Client
  std::strcpy(queues.toShaped->clientAddress,
              ctrlMsg->srcIP);
  std::strcpy(queues.toShaped->clientPort,
              ctrlMsg->srcPort);
  std::strcpy(queues.fromShaped->clientAddress,
              ctrlMsg->srcIP);
  std::strcpy(queues.fromShaped->clientPort,
              ctrlMsg->srcPort);

  //Dest/Server
  std::strcpy(queues.toShaped->serverAddress,
              ctrlMsg->destIP);
  std::strcpy(queues.toShaped->serverPort,
              ctrlMsg->destPort);
  std::strcpy(queues.fromShaped->serverAddress,
              ctrlMsg->destIP);
  std::strcpy(queues.fromShaped->serverPort,
              ctrlMsg->destPort);
}

inline bool ShapedReceiver::assignQueues(MsQuicStream *stream) {
  // Find an unused QueuePair and map it
  return std::ranges::any_of(*queuesToStream, [&](auto &iterator) {
    // iterator.first is QueuePair, iterator.second is sender
    if (iterator.second == nullptr) {
      QueuePair queues = iterator.first;
      // No sender attached to this queue pair
      (*streamToQueues)[stream] = iterator.first;
      (*queuesToStream)[iterator.first] = stream;

      QUIC_UINT62 streamID;
      stream->GetID(&streamID);
      log(DEBUG, "Assigning stream " +
                 std::to_string(streamID) +
                 " to queues {" + std::to_string(queues.fromShaped->ID) + "," +
                 std::to_string(queues.toShaped->ID) + "}");
      //TODO: Check assumption that unshaped sender closed the connection
      // before this queue was re-assigned
      queues.fromShaped->markedForDeletion = false;
      queues.toShaped->markedForDeletion = false;
      queues.toShaped->clear();
      queues.fromShaped->clear();

      if (streamIDtoCtrlMsg.find(streamID) != streamIDtoCtrlMsg.end()) {
        copyClientInfo(queues, &streamIDtoCtrlMsg[streamID]);
        signalUnshapedProcess((*streamToQueues)[stream].fromShaped->ID,
                              SYN);

        streamIDtoCtrlMsg.erase(streamID);

      }
      return true;
    }
    return false;
  });
}

void ShapedReceiver::receivedControlMessage(struct ControlMessage *ctrlMsg) {
  switch (ctrlMsg->streamType) {
    case Dummy:
      log(DEBUG, "Dummy stream is at " + std::to_string(ctrlMsg->streamID));

      dummyStreamID = ctrlMsg->streamID;
      break;
    case Data: {
      auto dataStream = findStreamByID(ctrlMsg->streamID);
      auto &queues = (*streamToQueues)[dataStream];
      if (ctrlMsg->connStatus == SYN) {
        log(DEBUG, "Received SYN on stream " +
                   std::to_string(ctrlMsg->streamID));

        if (dataStream != nullptr) {
          copyClientInfo(queues, ctrlMsg);
          signalUnshapedProcess(queues.fromShaped->ID, SYN);

        } else {
          // Map from stream (which has not yet started) to client
          streamIDtoCtrlMsg[ctrlMsg->streamID] = *ctrlMsg;
        }
      } else if (ctrlMsg->connStatus == FIN) {
        log(DEBUG, "Received FIN on stream " +
                   std::to_string(ctrlMsg->streamID));
        if (queues.fromShaped != nullptr) {
          // Else it's a retransmission of FIN, which has already been handled!
          queues.fromShaped->markedForDeletion = true;
          signalUnshapedProcess(queues.fromShaped->ID, FIN);
        }
      }
    }
      break;
    case Control:
      // This (re-identification of control stream) should never happen
      break;
  }
}

void
ShapedReceiver::receivedShapedData(MsQuicStream *stream, uint8_t *buffer, size_t
length) {
  uint64_t streamID;
  stream->GetID(&streamID);
  // Check if this is first byte from the other middlebox
  if (controlStream == nullptr) {
    log(DEBUG, "Control Stream is at " + std::to_string(streamID));
    size_t numMessages = length / sizeof(struct ControlMessage);

    // struct ControlMessage *messages[length/sizeof(struct ControlMessage)];
    for (size_t i = 0; i < numMessages; i++) {
      auto ctrlMsg = reinterpret_cast<struct ControlMessage *>(buffer +
                                                               (sizeof(struct ControlMessage) *
                                                                i));
      // auto ctrlMsg = static_cast<struct ControlMessage *>(buffer + (sizeof(struct ControlMessage) * i));
      if (ctrlMsg->streamType == Control) {
        controlStream = stream;
//        controlStreamID = streamID;
      } else if (ctrlMsg->streamType == Dummy) {
        dummyStreamID = ctrlMsg->streamID;
      }
    }
    return;
  } else if (controlStream == stream) {
    // A message from the controlStream
    if (length % sizeof(ControlMessage) != 0) {
      log(ERROR, "Received half a control message!");
    } else {
      uint8_t messages = length / sizeof(ControlMessage);
      for (uint8_t i = 0; i < messages; i++) {
        auto ctrlMsg = reinterpret_cast<struct ControlMessage *>(buffer);
        receivedControlMessage(ctrlMsg);
      }
    }
    return;
  }

  // Not a control stream... Check for other types
  if (streamID == dummyStreamID) {
    // Dummy Data
    if (dummyStream == nullptr) dummyStream = stream;
    return;
  }

  // This is a data stream
//  log(DEBUG, "Received data from client on stream " +
//             std::to_string(streamID));
  if ((*streamToQueues)[stream].fromShaped == nullptr) {
    if (!assignQueues(stream))
      log(ERROR, "More streams from peer than allowed!");
  }

  auto fromShaped = (*streamToQueues)[stream].fromShaped;
  while (fromShaped->push(buffer, length) == -1) {
    log(WARNING, "(fromShaped) " + std::to_string(fromShaped->ID) +
                 " is full, waiting for it to be empty");

    // Sleep for some time. For performance reasons, this is the same as
    // the interval with the unshaped components checks queues for data.
    std::this_thread::sleep_for(std::chrono::microseconds(50000));
  }
}

void ShapedReceiver::sendDummy(size_t dummySize) {
  // We do not have dummy stream yet
  if (dummyStream == nullptr) return;
  auto buffer = reinterpret_cast<uint8_t *>(malloc(dummySize));
  memset(buffer, 0, dummySize);
  if (!sendResponse(dummyStream, buffer, dummySize)) {
  }
}

size_t ShapedReceiver::sendData(size_t dataSize) {
  auto origSize = dataSize;

  for (auto &iterator: *queuesToStream) {
    // We have sent enough
    if (dataSize == 0) break;

    auto &queues = iterator.first;
    auto queueSize = queues.toShaped->size();
    // No data in this queue, check others
    if (queueSize == 0) {
      if ((*pendingSignal)[queues.toShaped->ID] == FIN) {
        // Send a termination control message
        auto *message =
            reinterpret_cast<struct ControlMessage *>(malloc(sizeof(struct
                ControlMessage)));
        iterator.second->GetID(&message->streamID);
        log(DEBUG,
            "Sending FIN on stream " + std::to_string(message->streamID));
        message->streamType = Data;
        message->connStatus = FIN;
        sendResponse(controlStream,
                     reinterpret_cast<uint8_t *>(message),
                     sizeof(*message));
        (*pendingSignal).erase(queues.toShaped->ID);
        // TODO: Check if this causes an issue
        if (queues.fromShaped->markedForDeletion) {
          log(DEBUG, "Clearing the mapping for the queuePair {" +
                     std::to_string(queues.fromShaped->ID) + "," +
                     std::to_string(queues.toShaped->ID) + "}");
//          (*streamToQueues)[iterator.second].fromShaped = nullptr;
//          (*streamToQueues)[iterator.second].toShaped = nullptr;
          (*streamToQueues).erase(iterator.second);
          (*queuesToStream)[queues] = nullptr;
        }
      }
      continue;
    }

    auto SizeToSendFromQueue = std::min(queueSize, dataSize);
    auto buffer =
        reinterpret_cast<uint8_t *>(malloc(SizeToSendFromQueue + 1));

    iterator.first.toShaped->pop(buffer, SizeToSendFromQueue);

    auto stream = iterator.second;
    if (!sendResponse(stream, buffer, SizeToSendFromQueue)) {
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

void ShapedReceiver::log(logLevels level, const std::string &log) {
  if (logLevel < level) return;
  std::string levelStr;
  switch (level) {
    case DEBUG:
      levelStr = "SR:DEBUG: ";
      break;
    case ERROR:
      levelStr = "SR:ERROR: ";
      break;
    case WARNING:
      levelStr = "SR:WARNING: ";
      break;

  }
  auto time = std::time(nullptr);
  auto localTime = std::localtime(&time);
  std::scoped_lock lock(logWriter);
  std::cerr << std::put_time(localTime, "[%H:%M:%S] ")
            << levelStr << log << std::endl;

}
