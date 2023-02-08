//
// Created by Rut Vora
//

#include "ShapedReceiver.h"
#include <utility>

ShapedReceiver::ShapedReceiver(std::string appName,
                               const std::string &serverCert,
                               const std::string &serverKey, int maxPeers,
                               int maxStreamsPerPeer, uint16_t bindPort,
                               uint64_t idleTimeout, double epsilon,
                               double sensitivity,
                               __useconds_t DPCreditorLoopInterval,
                               __useconds_t senderLoopInterval) :
    appName(std::move(appName)), sigInfo(nullptr),
    controlStream(nullptr), dummyStream(nullptr),
    dummyStreamID(QUIC_UINT62_MAX) {
  queuesToStream =
      new std::unordered_map<QueuePair,
          MsQuicStream *, QueuePairHash>(maxStreamsPerPeer);

  streamToQueues =
      new std::unordered_map<MsQuicStream *, QueuePair>(maxStreamsPerPeer);

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
                         QUIC::Receiver::WARNING,
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
    std::cerr << "Start the unshaped process first" << std::endl;
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

[[maybe_unused]] [[noreturn]] void
ShapedReceiver::sendResponseLoop(__useconds_t interval) {
  while (true) {
    std::this_thread::sleep_for(std::chrono::microseconds(interval));
    for (auto &iterator: *queuesToStream) {
      auto size = iterator.first.toShaped->size();
      if (size > 0) {
        std::cout << "Peer2:shaped: Got data in queue: "
                  << iterator.first.toShaped <<
                  std::endl;
        auto buffer = reinterpret_cast<uint8_t *>(malloc(size));
        iterator.first.toShaped->pop(buffer, size);
        MsQuicStream *stream = iterator.second;
        if (stream == nullptr) {
          for (auto &itr: *streamToQueues) {
            if (itr.first != nullptr) {
              stream = itr.first;
              break;
            }
          }
        }
        QUIC_UINT62 streamId;
        stream->GetID(&streamId);
        std::cout << "Peer2:Shaped: Sending response on " << streamId <<
                  std::endl;
        sendResponse(stream, buffer, size);
      }
    }
  }
}

void ShapedReceiver::signalUnshapedProcess(uint64_t queueID,
                                           connectionStatus connStatus) {
  std::scoped_lock lock(writeLock);
  struct SignalInfo::queueInfo queueInfo{queueID, connStatus};
  sigInfo->enqueue(SignalInfo::fromShaped, queueInfo);
  // TODO: Handle case when queue is full

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
      // No sender attached to this queue pair
      (*streamToQueues)[stream] = iterator.first;
      (*queuesToStream)[iterator.first] = stream;

      QUIC_UINT62 streamID;
      stream->GetID(&streamID);
      QueuePair queues = iterator.first;
      if (streamIDtoCtrlMsg.find(streamID) != streamIDtoCtrlMsg.end()) {
        copyClientInfo(queues, &streamIDtoCtrlMsg[streamID]);
        signalUnshapedProcess((*streamToQueues)[stream].fromShaped->queueID,
                              NEW);

        streamIDtoCtrlMsg.erase(streamID);

      }
      std::cout << "Peer2:Shaped: Assigned queues to a new stream: " <<
                std::endl;
      return true;
    }
    return false;
  });
}

void ShapedReceiver::receivedControlMessage(struct ControlMessage *ctrlMsg) {
  switch (ctrlMsg->streamType) {
    case Dummy:
      std::cout << "Dummy stream ID " << ctrlMsg->streamID << std::endl;
      dummyStreamID = ctrlMsg->streamID;
      break;
    case Data: {
      auto dataStream = findStreamByID(ctrlMsg->streamID);
      auto queues = (*streamToQueues)[dataStream];
      if (ctrlMsg->connStatus == NEW) {
        std::cout << "Peer2:Shaped: Data stream new ID " << ctrlMsg->streamID
                  << std::endl;
        if (dataStream != nullptr) {
          copyClientInfo(queues, ctrlMsg);
          signalUnshapedProcess(queues.fromShaped->queueID, NEW);

        } else {
          // Map from stream (which has not yet started) to client
          streamIDtoCtrlMsg[ctrlMsg->streamID] = *ctrlMsg;
        }
      } else if (ctrlMsg->connStatus == TERMINATED) {
        std::cout << "Peer2:Shaped: Data stream terminated ID " <<
                  ctrlMsg->streamID
                  << std::endl;
        queues.fromShaped->markedForDeletion = true;
        queues.toShaped->markedForDeletion = true;
        signalUnshapedProcess(queues.fromShaped->queueID, TERMINATED);

        (*queuesToStream)[queues] = nullptr;
        (*streamToQueues).erase(dataStream);

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
    std::cout << "Peer2:Shaped: Setting control stream" << std::endl;
    size_t numMessages = length / sizeof(struct ControlMessage);

    // struct ControlMessage *messages[length/sizeof(struct ControlMessage)];
    for (int i = 0; i < numMessages; i++) {
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
    auto ctrlMsg = reinterpret_cast<struct ControlMessage *>(buffer);
    std::cout << "Peer2:Shaped: Received control message for stream type " <<
              ctrlMsg->streamType << std::endl;
    receivedControlMessage(ctrlMsg);
    return;
  }

  // Not a control stream... Check for other types
  if (streamID == dummyStreamID) {
    // Dummy Data
    // TODO: Maybe make a queue and drop it in the unshaped side?
//    std::cout << "Peer2:Shaped: Received dummy on: " << dummyStreamID
//              << std::endl;
    if (dummyStream == nullptr) dummyStream = stream;
    return;
  }

  // This is a data stream
  uint64_t tmpStreamID;
  std::cout << "Peer2:Shaped: Received actual data on: "
            << stream->GetID(&tmpStreamID) << std::endl;
  if ((*streamToQueues)[stream].fromShaped == nullptr) {
    std::cout << "Peer2:Shaped: Assigning queues to stream: " << tmpStreamID
              << std::endl;
    if (!assignQueues(stream))
      std::cerr << "More streams than expected!" << std::endl;
  }

  auto fromShaped = (*streamToQueues)[stream].fromShaped;
  // TODO: Check if the buffer needs to be freed after pushing to queue
  while (fromShaped->push(buffer, length) == -1) {
    std::cerr << "Queue for client " << fromShaped->clientAddress
              << ":" << fromShaped->clientPort
              << " is full. Waiting for it to be empty"
              << std::endl;
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
    std::cerr << "Failed to send dummy data" << std::endl;
  }
}

size_t ShapedReceiver::sendData(size_t dataSize) {
  auto origSize = dataSize;

  uint64_t tmpStreamID;
  for (auto &iterator: *queuesToStream) {
    auto queueSize = iterator.first.toShaped->size();
    // No data in this queue, check others
    if (queueSize == 0) continue;
    // Client has already disconnected. No need to send this stream. Unshaped
    // component will clear this queue soon.
    if (iterator.first.toShaped->markedForDeletion) continue;

    std::cout << "Peer2:Shaped: not-null Queue size is: " << queueSize
              << std::endl;
    // We have sent enough
    if (dataSize == 0) break;

    auto SizeToSendFromQueue = std::min(queueSize, dataSize);
    auto buffer = reinterpret_cast<uint8_t *>(malloc(SizeToSendFromQueue));
    iterator.first.toShaped->pop(buffer, SizeToSendFromQueue);
    // We find the queue that has data, lets find the stream which is not null to send data on it.
    auto stream = iterator.second;
    stream->GetID(&tmpStreamID);
    std::cout << "Peer2:Shaped: Sending data to the stream: " << tmpStreamID
              << std::endl;
    std::cout << "Peer2:Shaped: data is: " << buffer << std::endl;
    if (!sendResponse(stream, buffer, SizeToSendFromQueue)) {
      std::cerr << "Failed to send data" << std::endl;
    }
    dataSize -= SizeToSendFromQueue;
  }
  // We expect the data size to be zero if we have sent all the data
  return origSize - dataSize;
}
