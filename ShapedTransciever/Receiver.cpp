//
// Created by Rut Vora
//

#include <stdexcept>
#include <iostream>
#include <sstream>
#include "Receiver.h"


// Load the API table. Necessary before any calls to MsQuic
// It is defined as an extern const in "msquic.hpp"
const MsQuicApi *MsQuic = new MsQuicApi();


void Receiver::log(logLevels level, const std::string &log) {
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

    std::cout << levelStr << log << std::endl;
  }
}

QUIC_STATUS Receiver::streamCallbackHandler(MsQuicStream *stream,
                                            void *context,
                                            QUIC_STREAM_EVENT *event) {
  UNREFERENCED_PARAMETER(context);

  switch (event->Type) {
    case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
      stream->Shutdown(0);
      log(DEBUG, "Shutting stream down as peer aborted");
      break;

    case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
      // TODO Finish Sending
      // stream->Send(...);
      UNREFERENCED_PARAMETER(context);
      stream->Shutdown(0);
      log(DEBUG, "Shutting stream down as peer sent a shutdown signal");
      break;

    case QUIC_STREAM_EVENT_RECEIVE:
      // TODO
      log(DEBUG, "Received data from peer");
      break;

    case QUIC_STREAM_EVENT_SEND_COMPLETE:
      free(event->SEND_COMPLETE.ClientContext);
      log(DEBUG, "Finished a call to streamSend");
      break;

    case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
      //Automatically handled as cleanUpAutoDelete is set when creating the
      // stream class instance in connectionHandler
      log(DEBUG, "Connection Successfully shutdown and cleaned up");
    default:
      break;
  }
  return QUIC_STATUS_SUCCESS;

}

QUIC_STATUS Receiver::connectionHandler(MsQuicConnection *connection,
                                        void *context,
                                        QUIC_CONNECTION_EVENT *event) {
  UNREFERENCED_PARAMETER(connection);
  UNREFERENCED_PARAMETER(context);

  QUIC_STATUS Status = QUIC_STATUS_NOT_SUPPORTED;
  MsQuicStream *stream = nullptr;

  switch (event->Type) {
    case QUIC_CONNECTION_EVENT_CONNECTED:
      //
      // The handshake has completed for the connection.
      //
      log(DEBUG, "New Connection request was received");
      connection->SendResumptionTicket();
      break;

    case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
      stream = new MsQuicStream(event->PEER_STREAM_STARTED.Stream,
                                CleanUpAutoDelete,
                                streamCallbackHandler);
      {
        const void *streamPtr = static_cast<const void *>(stream);
        std::stringstream ss;
        ss << streamPtr;
        log(DEBUG, "Peer started new stream " + ss.str());
      }
      break;

    case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
      connection->Close();
      log(DEBUG, "Shutdown complete. Connection closed");
      break;
    default:
      break;
  }
  return Status;
}

bool Receiver::loadConfiguration(const std::string &certFile,
                                 const std::string &keyFile) {
  // The settings for the QUIC Connection
  auto *settings = new MsQuicSettings;
  settings->SetIdleTimeoutMs(idleTimeoutMs);
  settings->SetServerResumptionLevel(QUIC_SERVER_RESUME_AND_ZERORTT);

  // Configures the server's settings to allow for the peer to open a single
  // bidirectional stream. By default, connections are not configured to allow
  // any streams from the peer.
  //
  settings->SetPeerBidiStreamCount(1);

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
                   int port, logLevels level) : configuration(nullptr),
                                                listener(nullptr),
                                                addr(new QuicAddr(
                                                    QUIC_ADDRESS_FAMILY_UNSPEC)) {
  logLevel = level;
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
                                          this->connectionHandler);
  listener->Start(alpn, &addr->SockAddr);
  log(DEBUG, "Started listening");
}

void Receiver::stopListening() {
  delete listener;
  listener = nullptr;
  log(DEBUG, "Stopped listening");
}