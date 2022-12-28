//
// Created by Rut Vora
//

#include "Sender.h"
#include <sstream>
#include <iostream>

const MsQuicApi *MsQuic = new MsQuicApi();


void Sender::log(logLevels level, const std::string &log) {
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

QUIC_STATUS Sender::streamCallbackHandler(MsQuicStream *stream,
                                          void *context,
                                          QUIC_STREAM_EVENT *event) {

  auto *sender = (Sender *) context;
  const void *streamPtr = static_cast<const void *>(stream);
  std::stringstream ss;
  ss << "[Stream] " << streamPtr << " ";
  switch (event->Type) {
    case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
      stream->Shutdown(0);
      ss << "shut down as peer aborted";
      sender->log(DEBUG, ss.str());
      break;

    case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
      // TODO Finish Sending

      //Send a FIN
      stream->Send(nullptr, 0, QUIC_SEND_FLAG_FIN, nullptr);
      ss << "shut down as peer sent a shutdown signal";
      sender->log(DEBUG, ss.str());
      break;

    case QUIC_STREAM_EVENT_RECEIVE:
      // TODO
      ss << "Received data from peer";
      sender->log(DEBUG, ss.str());
      break;

    case QUIC_STREAM_EVENT_SEND_COMPLETE:
      free(event->SEND_COMPLETE.ClientContext);
      ss << "Finished a call to streamSend";
      sender->log(DEBUG, ss.str());
      break;

    case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
      //Automatically handled as cleanUpAutoDelete is set when creating the
      // stream class instance in connectionHandler
      ss << "The underlying connection was shutdown and cleaned up "
            "successfully";
      sender->log(DEBUG, ss.str());
    default:
      break;
  }
  return QUIC_STATUS_SUCCESS;

}

QUIC_STATUS Sender::connectionHandler(MsQuicConnection *connection,
                                      void *context,
                                      QUIC_CONNECTION_EVENT *event) {

  auto *sender = (Sender *) context;
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
      sender->log(DEBUG, ss.str());
      sender->connected = true;

      break;

    case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
      stream = new MsQuicStream(event->PEER_STREAM_STARTED.Stream,
                                CleanUpAutoDelete,
                                streamCallbackHandler);
      {
        const void *streamPtr = static_cast<const void *>(stream);

        ss << "Stream " << streamPtr << " started";
        sender->log(DEBUG, ss.str());
      }
      break;

    case QUIC_CONNECTION_EVENT_RESUMED:
      ss << "resumed";
      sender->log(DEBUG, ss.str());
      break;

    case QUIC_CONNECTION_EVENT_RESUMPTION_TICKET_RECEIVED:
      ss << "Received the following resumption ticket: ";
      for (int i = 0; i < event->RESUMPTION_TICKET_RECEIVED
          .ResumptionTicketLength; i++) {
        ss << event->RESUMPTION_TICKET_RECEIVED.ResumptionTicket[i];
      }
      sender->log(DEBUG, ss.str());
      break;

    case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
      connection->Close();

      ss << "closed successfully";
      sender->log(DEBUG, ss.str());
      break;

    case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
      ss << "shut down by peer";
      sender->log(DEBUG, ss.str());
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
        sender->log(DEBUG, ss.str());


      } else {
        ss << "shut down by underlying transport layer";
        sender->log(WARNING, ss.str());
      }
      break;

    default:
      break;
  }
  return QUIC_STATUS_SUCCESS;
}

bool Sender::loadConfiguration(bool noServerValidation) {
  auto *settings = new MsQuicSettings;
  settings->SetIdleTimeoutMs(idleTimeoutMs);

  // Configure default client configuration
  QUIC_CREDENTIAL_CONFIG config{};
  config.Type = QUIC_CREDENTIAL_TYPE_NONE;
  config.Flags = QUIC_CREDENTIAL_FLAG_CLIENT;
  if (noServerValidation) {
    config.Flags |= QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
  }

  MsQuicCredentialConfig credConfig(config);

  configuration = new MsQuicConfiguration(reg, alpn, *settings,
                                          credConfig);
  if (configuration->IsValid()) {
    log(DEBUG, "Configuration loaded successfully!");
    return true;
  }
  log(ERROR, "Error loading configuration");
  return false;
}

Sender::Sender(const std::string &serverName, uint16_t port,
               bool noServerValidation,
               logLevels _logLevel,
               uint64_t _idleTimeoutMs) : idleTimeoutMs
                                              (_idleTimeoutMs), configuration
                                              (nullptr), connection(nullptr) {
  logLevel = _logLevel;
  loadConfiguration(noServerValidation);
  connection = new MsQuicConnection(reg, autoCleanup ? CleanUpAutoDelete :
                                         CleanUpManual,
                                    connectionHandler, this);
  connection->Start(*configuration, serverName.c_str(), port);
  if (!connection->IsValid()) {
    std::stringstream ss;
    ss << "Connection to " << serverName << ":" << port << " failed!";
    log(ERROR, ss.str());
    connection->Close();
    throw std::runtime_error("Connection Failed!");
  }

}

MsQuicStream *Sender::startStream() {
  MsQuicStream *stream;
  stream = new MsQuicStream{*connection, QUIC_STREAM_OPEN_FLAG_NONE,
                            autoCleanup ?
                            CleanUpAutoDelete : CleanUpManual,
                            streamCallbackHandler, this};
  while (!connected);  // TODO: Improve this with wait and conditional variables
  if (!stream->Start()) {
    log(ERROR, "Stream could not be started");
    throw std::runtime_error("Stream could not be started");
  }
  const void *streamPtr = static_cast<const void *>(stream);
  std::stringstream ss;
  ss << "[Stream] " << streamPtr << " started";
  log(DEBUG, ss.str());
  return stream;
}

bool Sender::send(MsQuicStream * stream, const std::string& buffer) {
  auto SendBufferRaw = (uint8_t *)malloc(sizeof(QUIC_BUFFER) + buffer.length());
  auto SendBuffer = (QUIC_BUFFER *)SendBufferRaw;
  SendBuffer->Buffer = SendBufferRaw + sizeof(QUIC_BUFFER);
  SendBuffer->Length = buffer.length();
  const void *streamPtr = static_cast<const void *>(stream);
  std::stringstream ss;
  ss << "[Stream ]" << streamPtr;
  if(QUIC_FAILED(stream->Send(SendBuffer, 1, QUIC_SEND_FLAG_NONE, this))) {
    ss << " could not send data";
    log(ERROR, ss.str());
    return false;
  }
  ss << " Data sent successfully";
  log(DEBUG, ss.str());
  return true;
}