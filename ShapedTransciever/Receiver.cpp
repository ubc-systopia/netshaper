//
// Created by Rut Vora
//

#include <stdexcept>
#include <iostream>
#include <sstream>
#include <utility>
#include "Receiver.h"

void Receiver::log(logLevels level, const std::string
&log) {
  std::string levelStr;
  switch (level) {
    case DEBUG:
      levelStr = "DEBUG: ";
      break;
    case ERROR:
      levelStr = "ERROR: ";
      break;
    case WARNING:
      levelStr = "WARNING: ";
      break;

  }
  if (logLevel >= level) {

    std::cerr << levelStr << log << std::endl;
  }
}

QUIC_STATUS Receiver::streamCallbackHandler(MsQuicStream *stream,
                                            void *context,
                                            QUIC_STREAM_EVENT *event) {
  auto *receiver = (Receiver *) context;

  const void *streamPtr = static_cast<const void *>(stream);
  std::stringstream ss;
  ss << "[Stream] " << streamPtr << " ";
  switch (event->Type) {
    case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
      stream->Shutdown(0);
      ss << "shut down as peer aborted";
      receiver->log(DEBUG, ss.str());
      break;

    case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
      // TODO Finish Sending

      //Send a FIN
      stream->Send(nullptr, 0, QUIC_SEND_FLAG_FIN, nullptr);
      ss << "shut down as peer sent a shutdown signal";
      receiver->log(DEBUG, ss.str());
      break;

    case QUIC_STREAM_EVENT_RECEIVE: {
      auto bufferCount = event->RECEIVE.BufferCount;
      ss << "Received data from peer: ";
      for (int i = 0; i < bufferCount; i++) {
        receiver->onReceive(event->RECEIVE.Buffers[i].Buffer,
                            event->RECEIVE.Buffers[i].Length);

        auto length = event->RECEIVE.Buffers[i].Length;
        ss << " \n\t Length: " << length;
        ss << "\n\t Data: ";
        for (int j = 0; j < length; j++) {
          ss << event->RECEIVE.Buffers[i].Buffer[j];
        }
      }
      receiver->log(DEBUG, ss.str());
    }
      break;

    case QUIC_STREAM_EVENT_SEND_COMPLETE:
      ss << "Finished a call to streamSend";
      receiver->log(DEBUG, ss.str());
      break;

    case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
      //Automatically handled as cleanUpAutoDelete is set when creating the
      // stream class instance in connectionHandler
      ss << "The underlying connection was shutdown and cleaned up "
            "successfully";
      receiver->log(DEBUG, ss.str());
    default:
      break;
  }
  return QUIC_STATUS_SUCCESS;

}

QUIC_STATUS Receiver::connectionHandler(MsQuicConnection *connection,
                                        void *context,
                                        QUIC_CONNECTION_EVENT *event) {
  auto *receiver = (Receiver *) context;

  MsQuicStream *stream;
  std::stringstream ss;
  const void *connectionPtr = static_cast<const void *>(connection);
  ss << "[Connection] " << connectionPtr << " ";

  switch (event->Type) {
    case QUIC_CONNECTION_EVENT_CONNECTED:
      //
      // The handshake has completed for the connection.
      //


      ss << "Connected";
      receiver->log(DEBUG, ss.str());


      connection->SendResumptionTicket();
      break;

    case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
      stream = new MsQuicStream(event->PEER_STREAM_STARTED.Stream,
                                CleanUpAutoDelete,
                                streamCallbackHandler, context);
      {
        const void *streamPtr = static_cast<const void *>(stream);

        ss << "Stream " << streamPtr << " started";
        receiver->log(DEBUG, ss.str());
      }
      break;

    case QUIC_CONNECTION_EVENT_RESUMED:
      ss << "resumed";
      receiver->log(DEBUG, ss.str());
      break;

    case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
      connection->Close();

      ss << "closed successfully";
      receiver->log(DEBUG, ss.str());
      break;

    case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
      ss << "shut down by peer";
      receiver->log(DEBUG, ss.str());
      break;

    case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
      //
      // The connection has been shut down by the transport. Generally, this
      // is the expected way for the connection to shut down with this
      // protocol, since we let idle timeout kill the connection.
      //
      if (event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status ==
          QUIC_STATUS_CONNECTION_IDLE) {

        ss << "shutting down on idle";
        receiver->log(DEBUG, ss.str());


      } else {
        ss << "shut down by underlying transport layer";
        receiver->log(WARNING, ss.str());
      }
      break;

    default:
      break;
  }
  return QUIC_STATUS_SUCCESS;
}

bool Receiver::loadConfiguration(const std::string &certFile,
                                 const std::string &keyFile) {
  // The settings for the QUIC Connection
  auto *settings = new MsQuicSettings;
  settings->SetIdleTimeoutMs(idleTimeoutMs);
  settings->SetServerResumptionLevel(QUIC_SERVER_RESUME_AND_ZERORTT);


  settings->SetPeerBidiStreamCount(maxPeerStreams);

  // Load the X509 certificate
  QUIC_CREDENTIAL_CONFIG config{};
  QUIC_CERTIFICATE_FILE certificateFileStruct;
  config.CertificateFile = &certificateFileStruct;
  config.CertificateFile->CertificateFile = certFile.c_str();
  config.CertificateFile->PrivateKeyFile = keyFile.c_str();
  config.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE;


  MsQuicCredentialConfig credConfig(config);

  configuration = new MsQuicConfiguration(reg, alpn, *settings,
                                          credConfig);
  if (configuration->IsValid()) {
    log(DEBUG, "Configuration loaded successfully!");
    return true;
  }
  log(ERROR, "Error loading configuration. Are you sure the correct server "
             "certificate and keyfile are provided?");
  return false;
}

Receiver::Receiver(const std::string &certFile, const std::string &keyFile,
                   int port, std::function<void(uint8_t *buffer,
                                                size_t length)> onReceiveFunc,
                   logLevels level, int _maxPeerStreams, uint64_t
                   _idleTimeoutMs) : configuration(nullptr),
                                     listener(nullptr),
                                     addr(new QuicAddr(
                                         QUIC_ADDRESS_FAMILY_UNSPEC)),
                                     maxPeerStreams(_maxPeerStreams),
                                     idleTimeoutMs(_idleTimeoutMs),
                                     logLevel(level) {
  onReceive = std::move(onReceiveFunc);
  log(DEBUG, "Loading Configuration...");
  bool success = loadConfiguration(certFile, keyFile);
  if (!success) {
    exit(1);
  }
  addr->SetPort(port);

  {
    std::stringstream ss;
    ss << (alpn.operator const QUIC_BUFFER *())[0].Buffer;
    log(DEBUG, "ALPN: " + ss.str());
  }
  log(DEBUG, "Port: " + std::to_string(port));
}

void Receiver::startListening() {
  // Open the QUIC Listener (Receiver) for the given application and register
  // a listenerCallback function that is called for all events
  if (listener != nullptr) {
    log(WARNING, "startListening called on a receiver that's already "
                 "listening");
    return;
  }

  listener = new MsQuicAutoAcceptListener(reg, *configuration,
                                          this->connectionHandler, this);
  listener->Start(alpn, &addr->SockAddr);
  log(DEBUG, "Started listening");
}

void Receiver::stopListening() {
  delete listener;
  listener = nullptr;
  log(DEBUG, "Stopped listening");
}